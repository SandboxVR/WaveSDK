#include "anchorManager.h"
#include <unistd.h>
#include <fstream>

BGWork::BGWork()
{
    task.persistDataSize = 0;
    task.persistData = nullptr;
    task.workType = IDEAL;

    taskPtr = &task;

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_condv, NULL);

    std::function<void()> importExportFunc = [this]()
    {
        while (true)
        {
            // wait work
            pthread_mutex_lock(&m_mutex);
            while (taskPtr->workType == IDEAL)
            {
                LOGI("BGThread wait job...");
                pthread_cond_wait(&m_condv, &m_mutex);
            }

            if (taskPtr->workType == IMPORT_ANCHOR)
            {
                LOGI("BGThread IMPORT_ANCHOR...");
                // open persisted anchor in device. This file is pushed by other device
                std::ifstream importFile("/data/user/0/com.htc.vr.samples.wvr_native_anchor/files/import.bin", std::ios::in);
                if (importFile.is_open())
                {
                    LOGI("import anchor file exist, replace import data");
                    if (taskPtr->persistData != nullptr)
                    {
                        free(taskPtr->persistData);
                        taskPtr->persistDataSize = 0;
                    }

                    importFile.seekg(0, std::ifstream::end);
                    taskPtr->persistDataSize = importFile.tellg();
                    LOGI("import persisted anchor Size=%d", taskPtr->persistDataSize);
                    importFile.seekg(0, std::ifstream::beg);
                    taskPtr->persistData = (char *)malloc(sizeof(char) * taskPtr->persistDataSize);
                    importFile.read(taskPtr->persistData, taskPtr->persistDataSize);
                    importFile.close();
                }

                if (taskPtr->persistDataSize > 0)
                {
                    WVR_ImportPersistedSpatialAnchor(taskPtr->persistDataSize, taskPtr->persistData);
                }
                else
                {
                    LOGW("no persisted data can be import");
                }
                LOGI("BGThread IMPORT_ANCHOR done");
            }
            else if (taskPtr->workType == EXPORT_ANCHOR)
            {
                LOGI("BGThread EXPORT_ANCHOR...");
                if (taskPtr->persistData != nullptr)
                {
                    free(taskPtr->persistData);
                    taskPtr->persistDataSize = 0;
                }

                if (WVR_ExportPersistedSpatialAnchor(&taskPtr->persistedName, 0, &taskPtr->persistDataSize, nullptr) == WVR_Success)
                {
                    LOGI("export persisted anchor persistDataSize=%d", taskPtr->persistDataSize);
                    taskPtr->persistData = (char *)malloc(sizeof(char) * taskPtr->persistDataSize);
                }

                if (WVR_ExportPersistedSpatialAnchor(&taskPtr->persistedName, taskPtr->persistDataSize, &taskPtr->persistDataSize, taskPtr->persistData) != WVR_Success)
                {
                    LOGE("export persisted anchor fail");
                    free(taskPtr->persistData);
                    taskPtr->persistDataSize = 0;
                }
                else
                {
                    // store persisted anchor in device. can push this file to other device
                    std::ofstream binFile("/data/user/0/com.htc.vr.samples.wvr_native_anchor/files/export.bin", std::ios::out | std::ios::binary);
                    binFile.write(taskPtr->persistData, taskPtr->persistDataSize);
                    binFile.close();
                }
                LOGI("BGThread EXPORT_ANCHOR done");
            }

            taskPtr->workType = IDEAL;
            pthread_mutex_unlock(&m_mutex);
        }
        return NULL;
    };

    bgThread = std::thread(importExportFunc);
}

BGWork::~BGWork()
{
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_condv);
}

bool BGWork::exportPersisted(WVR_SpatialAnchorName anchorName)
{
    if (pthread_mutex_trylock(&m_mutex) == 0)
    {
        taskPtr->workType = EXPORT_ANCHOR;
        taskPtr->persistedName = anchorName;
        pthread_cond_signal(&m_condv);
        pthread_mutex_unlock(&m_mutex);
        return true;
    }
    LOGI("exportPersisted fail; thread busy");
    return false;
}

bool BGWork::importPersisted()
{
    if (pthread_mutex_trylock(&m_mutex) == 0)
    {
        taskPtr->workType = IMPORT_ANCHOR;
        pthread_cond_signal(&m_condv);
        pthread_mutex_unlock(&m_mutex);
        return true;
    }
    LOGI("importPersisted fail; thread busy");
    return false;
}

bool BGWork::checkStatus()
{
    if (pthread_mutex_trylock(&m_mutex) == 0)
    {
        pthread_mutex_unlock(&m_mutex);
        return true;
    }
    return false;
}

anchorManager::anchorManager() : allSpatialAnchor(nullptr), mPlane(nullptr)
{
    // create back ground thread to handle persisted anchor import/export
    backGroundThread = new BGWork();
}

anchorManager::~anchorManager()
{
}

WVR_Pose_t anchorManager::convertToWVRPose(Matrix4 pose)
{
    WVR_Pose_t output;
    output.position.v[0] = pose[12];
    output.position.v[1] = pose[13];
    output.position.v[2] = pose[14];
    double quat[4];
    pose.ToQuat(quat);
    output.rotation.x = (float)quat[0];
    output.rotation.y = (float)quat[1];
    output.rotation.z = (float)quat[2];
    output.rotation.w = (float)quat[3];
    return output;
}

Matrix4 anchorManager::convertToMatrix4(WVR_Pose_t pose)
{
    q_xyz_quat_struct mQuatStruct;
    mQuatStruct.xyz[0] = (double)pose.position.v[0];
    mQuatStruct.xyz[1] = (double)pose.position.v[1];
    mQuatStruct.xyz[2] = (double)pose.position.v[2];

    mQuatStruct.quat[0] = (double)pose.rotation.x;
    mQuatStruct.quat[1] = (double)pose.rotation.y;
    mQuatStruct.quat[2] = (double)pose.rotation.z;
    mQuatStruct.quat[3] = (double)pose.rotation.w;

    Matrix4 output(mQuatStruct);
    return output;
}

bool anchorManager::createCubeAnchor(anchorType type, Matrix4 pose, Vector3 scale)
{
    // this example anchor name should be "s/c/p=time=scale.x=scale.y=scale.z="
    WVR_SpatialAnchorCreateInfo info;
    info.originModel = WVR_PoseOriginModel_OriginOnHead;
    info.pose = convertToWVRPose(pose);

    // anchor name()
    std::string dash("=");
    // type tag
    std::string spatialName;
    switch (type)
    {
    case spatial:
        // represent spatial anchor
        spatialName = {"s"};
        break;
    case cached:
        // represent cached anchor
        spatialName = {"c"};
        break;
    case persisted:
        // represent persisted anchor
        spatialName = {"p"};
        break;
    default:
        LOGE("createCubeAnchor type get error");
        break;
    }
    spatialName += dash;

    // time tag
    static int preventSameName = 0;
    preventSameName = (preventSameName + 1) % 10; // this allow create 10 times in one second
    uint64_t sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string tempString = std::to_string(sec);
    tempString = std::to_string(preventSameName) + tempString;
    spatialName += tempString;
    spatialName += dash;

    // scale tag
    spatialName += std::to_string(round(scale.x * 10000) / 10000);
    spatialName += dash;
    spatialName += std::to_string(round(scale.y * 10000) / 10000);
    spatialName += dash;
    spatialName += std::to_string(round(scale.z * 10000) / 10000);
    spatialName += dash;

    // add ending char
    spatialName.push_back('\0');

    std::copy(spatialName.begin(), spatialName.end(), info.anchorName.name);
    LOGI("addAnchor , scale:(%f, %f, %f)", round(scale.x * 10000) / 10000, round(scale.y * 10000) / 10000, round(scale.z * 10000) / 10000);

    // create anchor
    WVR_SpatialAnchor anchor;
    LOGI("create spatial anchor name(%d)=%s", spatialName.size(), spatialName.c_str());
    switch (type)
    {
    case spatial:
        // create spatical anchor
        if (WVR_CreateSpatialAnchor(&info, &anchor) != WVR_Success)
        {
            LOGE("create spatial anchor fail");
        }
        break;
    case cached:
        if (WVR_CreateSpatialAnchor(&info, &anchor) != WVR_Success)
        {
            LOGE("create spatial anchor fail");
            return false;
        }
        // create cached anchor
        WVR_SpatialAnchorCacheInfo cacheInfo;
        cacheInfo.spatialAnchor = anchor;
        cacheInfo.cachedSpatialAnchorName = info.anchorName;
        LOGI("create cached anchor");
        if (WVR_CacheSpatialAnchor(&cacheInfo) != WVR_Success)
            LOGE("create cached anchor fail");

        // destroy spatial anchor. In this sample, we will refrsh all spatial anchor by
        // using WVR_EnumerateCachedSpatialAnchorNames and WVR_CreateSpatialAnchorFromCacheName
        WVR_DestroySpatialAnchor(anchor);
        break;
    case persisted:
        // check if persisted anchor is full
        WVR_PersistedSpatialAnchorCountGetInfo countInfo;
        WVR_GetPersistedSpatialAnchorCount(&countInfo);
        if (countInfo.currentTrackingCount < countInfo.maximumTrackingCount)
        {
            if (WVR_CreateSpatialAnchor(&info, &anchor) != WVR_Success)
            {
                LOGE("create spatial anchor fail");
                return false;
            }
            // create persisted anchor
            WVR_SpatialAnchorPersistInfo persistInfo;
            persistInfo.spatialAnchor = anchor;
            persistInfo.persistedSpatialAnchorName = info.anchorName;
            LOGI("create persisted anchor");
            if (WVR_PersistSpatialAnchor(&persistInfo) != WVR_Success)
                LOGE("create persisted anchor fail");

            // destroy spatial anchor. In this sample, we will refrsh all spatial anchor by
            // using WVR_EnumerateCachedSpatialAnchorNames and WVR_CreateSpatialAnchorFromCacheName
            WVR_DestroySpatialAnchor(anchor);
        }
        else
        {
            LOGE("persist anchor is full");
        }
        break;
    default:
        LOGE("type get error");
        break;
    }

    return true;
}

Vector3 anchorManager::getScaleFromName(std::string str)
{
    size_t pos;
    std::string ss;
    float out[3];
    std::string dash = "=";

    pos = str.find("=");
    ss = str.substr(0, pos);
    str.erase(0, pos + dash.length());

    pos = str.find("=");
    ss = str.substr(0, pos);
    str.erase(0, pos + dash.length());

    pos = str.find("=");
    ss = str.substr(0, pos);
    out[0] = std::stof(ss);
    str.erase(0, pos + dash.length());

    pos = str.find("=");
    ss = str.substr(0, pos);
    out[1] = std::stof(ss);
    str.erase(0, pos + dash.length());

    pos = str.find("=");
    ss = str.substr(0, pos);
    out[2] = std::stof(ss);

    return Vector3(out[0], out[1], out[2]);
}

bool anchorManager::addUndefineCubeAnchor(anchorType type, CubeManager *cm)
{
    for (int a = 0; a < cm->undefineCubes.size(); a++)
    {
        createCubeAnchor(type, cm->undefineCubes[a].mPose, cm->undefineCubes[a].mScale);
        LOGI("add cube scale[%d] %f %f %f", a, cm->undefineCubes[a].mScale.x, cm->undefineCubes[a].mScale.y, cm->undefineCubes[a].mScale.z);
    }
    return true;
}

bool anchorManager::addCubeAnchor(anchorType type, PlaneManager *pm, Matrix4 controllerPose, Vector3 scale)
{
    if (pm->intersectionPose_world.size() > 0)
    {
        // need to rotate to align with plane coordinate
        Matrix4 r90;
        Matrix4 finalPose;
        r90.rotateX(90);
        for (int a = 0; a < pm->intersectionPose_world.size(); a++)
        {
            finalPose = pm->intersectionPose_world[a] * pm->mappingPoseOnPlane(controllerPose, pm->intersectionPose_world[a]);
            finalPose = finalPose * r90;
            createCubeAnchor(type, finalPose, scale);
        }
    }
    else
    {
        createCubeAnchor(type, controllerPose, scale);
    }

    return true;
}

bool anchorManager::clearTypeAnchor(anchorType type, CubeManager *cm)
{
    bool result = true;

    LOGI("clear all anchors %d", (*cm->targetCubeVector).size());
    for (int a = 0; a < (*cm->targetCubeVector).size(); a++)
    {
        // clear all spatial anchors
        if (!WVR_DestroySpatialAnchor(*(allSpatialAnchor + (*cm->targetCubeVector)[a]->anchorIndex)))
        {
            result = false;
            LOGE("delete spatial anchor %d fail", a);
        }
    }

    switch (type)
    {
    case cached:
        // clear cached anchors
        LOGI("clear all cached anchors");
        if (!WVR_ClearCachedSpatialAnchors())
        {
            result = false;
            LOGE("clear cached anchor fail");
        }
        break;
    case persisted:
        // clear persisted anchors
        LOGI("clear all persisted anchors");
        if (!WVR_ClearPersistedSpatialAnchors())
        {
            result = false;
            LOGE("clear persist anchor fail");
        }
        break;
    default:
        LOGE("clear anchor get wrong type");
        break;
    }
    return result;
}

bool anchorManager::deleteChosenAnchor(anchorType type, CubeManager *cm)
{
    for (int a = 0; a < cm->leftTouchVector.size(); a++)
    {
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(allSpatialAnchor[cm->leftTouchVector[a].anchorIndex], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        // delete cached/persist data
        if (type == cached)
        {
            // in this sample, cachedSpatialAnchorName equal to spatialAnchorName
            // Therefore, we can uncache anchor via spatialAnchorName
            LOGI("delete cache anchor");
            WVR_UncacheSpatialAnchor(&anchorState.anchorName);
        }
        else if (type == persisted)
        {
            // in this sample, persistedSpatialAnchorName equal to spatialAnchorName
            // Therefore, we can unpersist anchor via spatialAnchorName
            LOGI("delete persist anchor");
            WVR_UnpersistSpatialAnchor(&anchorState.anchorName);
        }

        // delete spatial anchor
        LOGI("delete spatial anchor");
        WVR_DestroySpatialAnchor(*(allSpatialAnchor + cm->leftTouchVector[a].anchorIndex));
    }

    return true;
}

// only used in persist anchor
bool anchorManager::exportPersistCubeAnchor(anchorType type, CubeManager *cm)
{
    if (cm->leftTouchVector.size() > 0)
    {
        WVR_SpatialAnchorState mAnchorState;
        WVR_GetSpatialAnchorState(allSpatialAnchor[cm->leftTouchVector[0].anchorIndex], WVR_PoseOriginModel_OriginOnHead, &mAnchorState);
        return backGroundThread->exportPersisted(mAnchorState.anchorName);
    }
    // do nothing
    LOGI("no persisted anchor can be operated");
    return false;
}

// only used in persist anchor
bool anchorManager::importPersistCubeAnchor(anchorType type, CubeManager *cm)
{
    return backGroundThread->importPersisted();
}

bool anchorManager::deleteChosenAnchor2changing(anchorType type, CubeManager *cm)
{
    for (int a = 0; a < cm->rightTouchVector.size(); a++)
    {
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(allSpatialAnchor[cm->rightTouchVector[a].anchorIndex], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        // delete cached/persist data
        if (type == cached)
        {
            // in this sample, cachedSpatialAnchorName equal to spatialAnchorName
            // Therefore, we can uncache anchor via spatialAnchorName
            WVR_UncacheSpatialAnchor(&anchorState.anchorName);
        }
        else if (type == persisted)
        {
            // in this sample, persistedSpatialAnchorName equal to spatialAnchorName
            // Therefore, we can unpersist anchor via spatialAnchorName
            WVR_UnpersistSpatialAnchor(&anchorState.anchorName);
        }

        // delete spatial anchor
        WVR_DestroySpatialAnchor(*(allSpatialAnchor + cm->rightTouchVector[a].anchorIndex));
    }
    cm->addUndefineCube();
    return true;
}

bool anchorManager::getExistedSpatialAnchor(CubeManager *cm)
{
    // WVR API get all spatial anchor num and allocate memory to get data
    allSpatialAnchorCount = 0;
    WVR_EnumerateSpatialAnchors(0, &allSpatialAnchorCount, nullptr);
    LOGI("get all spatial anchor anchorCount=%u", allSpatialAnchorCount);

    // clear memory
    if (allSpatialAnchor != nullptr)
        free(allSpatialAnchor);
    cm->clearAllTargetCubes();

    // assign memory space for anchor
    allSpatialAnchor = (WVR_SpatialAnchor *)malloc(sizeof(WVR_SpatialAnchor) * allSpatialAnchorCount);
    WVR_EnumerateSpatialAnchors(allSpatialAnchorCount, &allSpatialAnchorCount, allSpatialAnchor);

    // classified different spatial anchors
    for (int a = 0; a < allSpatialAnchorCount; a++)
    {
        Matrix4 pose;
        Vector3 scale;
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(allSpatialAnchor[a], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        std::string mName(anchorState.anchorName.name);

        switch (mName[0])
        {
        case 's':
            pose = convertToMatrix4(anchorState.pose);
            scale = getScaleFromName(mName);
            cm->addCubeFromAnchor(spatial, a, pose, scale, anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking ? false : true);
            break;
        case 'c':
            pose = convertToMatrix4(anchorState.pose);
            scale = getScaleFromName(mName);
            cm->addCubeFromAnchor(cached, a, pose, scale, anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking ? false : true);
            break;
        case 'p':
            pose = convertToMatrix4(anchorState.pose);
            scale = getScaleFromName(mName);
            cm->addCubeFromAnchor(persisted, a, pose, scale, anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking ? false : true);
            break;
        default:
            LOGE("Wrong anchor name exist: %s", mName.c_str());
            break;
        }
    }
    return true;
}

bool anchorManager::getExistedCachedAnchor()
{
    // get cache anchor name
    cachedAnchorCount = 0;
    WVR_EnumerateCachedSpatialAnchorNames(0, &cachedAnchorCount, nullptr);
    LOGI("Enumerate cached count=%d", cachedAnchorCount);
    WVR_SpatialAnchorName *allCachedName;
    allCachedName = (WVR_SpatialAnchorName *)malloc(sizeof(WVR_SpatialAnchorName) * cachedAnchorCount);
    WVR_EnumerateCachedSpatialAnchorNames(cachedAnchorCount, &cachedAnchorCount, allCachedName);
    // Let all cached anchor as spatial anchor
    WVR_SpatialAnchorFromCacheNameCreateInfo cacheInfo;
    for (int a = 0; a < cachedAnchorCount; a++)
    {
        cacheInfo.cachedSpatialAnchorName = allCachedName[a];
        cacheInfo.spatialAnchorName = allCachedName[a];
        WVR_SpatialAnchor *ptr; // will get this anchor address later(via WVR_EnumerateSpatialAnchors)
        LOGI("create spatial anchor from cached anchor");
        if (WVR_CreateSpatialAnchorFromCacheName(&cacheInfo, ptr) != WVR_Success)
            LOGI("already have same cached anchor as spatial anchor");
    }
    free(allCachedName);
    return true;
}

bool anchorManager::getExistedPersistedAnchor()
{
    // get persisted anchor name
    persistedAnchorCount = 0;
    WVR_EnumeratePersistedSpatialAnchorNames(0, &persistedAnchorCount, nullptr);
    LOGI("Enumerate persisted count=%d", persistedAnchorCount);
    WVR_SpatialAnchorName *allPersistedName;
    allPersistedName = (WVR_SpatialAnchorName *)malloc(sizeof(WVR_SpatialAnchorName) * persistedAnchorCount);
    WVR_EnumeratePersistedSpatialAnchorNames(persistedAnchorCount, &persistedAnchorCount, allPersistedName);
    // Let all persisted anchor as spatial anchor
    WVR_SpatialAnchorFromPersistenceNameCreateInfo persistInfo;
    for (int a = 0; a < persistedAnchorCount; a++)
    {
        persistInfo.persistedSpatialAnchorName = allPersistedName[a];
        persistInfo.spatialAnchorName = allPersistedName[a];
        WVR_SpatialAnchor *ptr; // will get this anchor address later(via WVR_EnumerateSpatialAnchors)
        LOGI("create spatial anchor from persisted anchor");
        if (WVR_CreateSpatialAnchorFromPersistenceName(&persistInfo, ptr) != WVR_Success)
            LOGI("already have same persisted anchor as spatial anchor");
    }
    free(allPersistedName);
    return true;
}

bool anchorManager::updateExistedAnchor(CubeManager *cm)
{
    LOGI("update anchor size= %d", (*cm->targetCubeVector).size());
    bool allAnchorCanTrack = true;
    for (int a = 0; a < (*cm->targetCubeVector).size(); a++)
    {
        Matrix4 pose;
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(allSpatialAnchor[(*cm->targetCubeVector)[a]->anchorIndex], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        if (anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking)
        {
            pose = convertToMatrix4(anchorState.pose);
            (*cm->targetCubeVector)[a]->mPose = pose;
            (*cm->targetCubeVector)[a]->anchorLost = false;
        }
        else
        {
            allAnchorCanTrack = false;
            (*cm->targetCubeVector)[a]->anchorLost = true;
            LOGI("Anchor cubeVector(%d) lost tracking", a);
        }
    }
    return allAnchorCanTrack;
}

bool anchorManager::init(CubeManager *cm, PlaneManager *pm)
{
    // Loading the WVR scene
    WVR_Result result = WVR_StartScene();
    if (result != WVR_Success)
    {
        LOGE("WVR_StartScene init fail %d", result);
        return false;
    }

    startScenePerception();

    getExistedCachedAnchor();
    getExistedPersistedAnchor();
    getExistedSpatialAnchor(cm);

    getExistedPlanes(pm);

    return true;
}

void anchorManager::startScenePerception()
{
    // start perception before get scene plane/mesh (need permission)
    // can call WVR_StopScenePerception when you won't need to update data
    // WVR_StopScenePerception will be automatic called when app is closed
    WVR_Result startPerceptionResult_2d = WVR_StartScenePerception(WVR_ScenePerceptionTarget_2dPlane);
    WVR_Result startPerceptionResult_3d = WVR_StartScenePerception(WVR_ScenePerceptionTarget_SceneMesh);

    if (WVR_Success != startPerceptionResult_2d || WVR_Success != startPerceptionResult_3d)
        LOGE("WVR_StartScenePerception init fail");
}

void anchorManager::deinit()
{
    if (WVR_Success != WVR_StopScenePerception(WVR_ScenePerceptionTarget_2dPlane))
    {
        LOGE("WVR_StopScenePerception deinit fail");
    }
    WVR_StopScene();
}

bool anchorManager::getExistedPlanes(PlaneManager *pm)
{
    // clear previous data
    pm->clearPlanes();
    if (mPlane != nullptr)
    {
        free(mPlane);
    }

    // WVR API get scene num and allocate memory to get data
    WVR_GetScenePlanes(nullptr, 0, &planeCount, WVR_PoseOriginModel_OriginOnHead, nullptr);
    mPlane = (WVR_ScenePlane *)malloc(sizeof(WVR_ScenePlane) * planeCount);
    WVR_GetScenePlanes(nullptr, planeCount, &planeCount, WVR_PoseOriginModel_OriginOnHead, mPlane);
    LOGI("get planes =%u", planeCount);

    // copy to vector
    for (int a = 0; a < planeCount; a++)
    {
        pm->addPlane(a, convertToMatrix4(mPlane[a].pose), Vector3(mPlane[a].extent.width, mPlane[a].extent.height, 1));
    }

    return true;
}

void anchorManager::getMesh_2D(PlaneManager *pm, sceneMeshData2D *sceneMesh2D)
{
    std::vector<Plane> planes;
    planes = pm->getPlanes();

    // check if is necessary to update 2D mesh
    // Strongly recommend not to call WVR_GetSceneMeshBuffer every frame, due to performance concern
    if (planes.size() == 0)
    {
        sceneMesh2D->clearAllData();
        return;
    }
    else if (sceneMesh2D->mLastMeshID != mPlane[0].meshBufferId)
    {
        LOGI("plane sceneMesh data should be updated");
        sceneMesh2D->clearAllData();
        sceneMesh2D->mLastMeshID = mPlane[0].meshBufferId;
        sceneMesh2D->shouldPush2GPU = true;

        for (int b = 0; b < planes.size(); b++)
        {
            WVR_SceneMeshBuffer buffer0 = {0, 0, nullptr, 0, 0, nullptr};

            // get sceneMesh size
            if (WVR_GetSceneMeshBuffer(mPlane[planes[b].scenePlaneIndex].meshBufferId, WVR_PoseOriginModel_OriginOnHead, &buffer0) != WVR_Success)
            {
                LOGE("Fail to get plane sceneMesh size");
                sceneMesh2D->clearAllData();
            }
            else
                LOGI("Get sceneMesh index size=%d", buffer0.indexCountOutput);

            std::vector<WVR_Vector3f> vertexBuffer(buffer0.vertexCountOutput);
            std::vector<uint32_t> indexBuffer(buffer0.indexCountOutput);
            WVR_SceneMeshBuffer buffer1 = {buffer0.vertexCountOutput, buffer0.vertexCountOutput, vertexBuffer.data(),
                                           buffer0.indexCountOutput, buffer0.indexCountOutput, indexBuffer.data()};

            // get sceneMesh data
            if (WVR_GetSceneMeshBuffer(mPlane[planes[b].scenePlaneIndex].meshBufferId, WVR_PoseOriginModel_OriginOnHead, &buffer1) != WVR_Success)
            {
                LOGE("Fail to get sceneMesh data");
                sceneMesh2D->clearAllData();
                return;
            }
            sceneMesh2D->mMeshIndex.push_back(indexBuffer);
            sceneMesh2D->mMeshVertex.push_back(vertexBuffer);
        }
        sceneMesh2D->getOrderMesh();
    }
    else // No need to update
    {
        return;
    }
}

void anchorManager::getMesh_3D(sceneMeshData3D *sceneMesh3D)
{
    sceneMesh3D->shouldPush2GPU = false;
    WVR_ScenePerceptionState spstate;
    WVR_GetScenePerceptionState(WVR_ScenePerceptionTarget_SceneMesh, &spstate);
    if (spstate == WVR_ScenePerceptionState_Completed)
    {
        uint32_t count_mesh;
        // get number of sceneMeshes
        if (WVR_GetSceneMeshes(sceneMesh3D->mMeshType, 0, &count_mesh, nullptr) != WVR_Success)
        {
            LOGE("Fail to get sceneMeshes count");
            sceneMesh3D->clearAllData();
            return;
        }
        else
            LOGI("SceneMesh ID size=%d", count_mesh);

        if (count_mesh == 0)
        {
            sceneMesh3D->clearAllData();
            return;
        }

        // Get sceneMeshes ID
        std::vector<WVR_SceneMesh> meshes(count_mesh);
        if (WVR_GetSceneMeshes(sceneMesh3D->mMeshType, count_mesh, &count_mesh, meshes.data()) != WVR_Success)
        {
            LOGE("Fail to get sceneMeshes ID");
            sceneMesh3D->clearAllData();
            return;
        }

        // check if is necessary to update collider mesh(When mesh is changed, all meshBufferId change)
        // Strongly recommend not to call WVR_GetSceneMeshBuffer every frame, due to performance concern
        if (sceneMesh3D->mLastMeshID != meshes[0].meshBufferId)
        {
            LOGI("collider/visual sceneMesh data should be updated");
            sceneMesh3D->clearAllData();
            sceneMesh3D->mLastMeshID = meshes[0].meshBufferId;
            sceneMesh3D->shouldPush2GPU = true;

            for (int a = 0; a < count_mesh; a++)
            {
                WVR_SceneMeshBuffer buffer0 = {0, 0, nullptr, 0, 0, nullptr};
                // get sceneMesh size
                if (WVR_GetSceneMeshBuffer(meshes[a].meshBufferId, WVR_PoseOriginModel_OriginOnHead, &buffer0) != WVR_Success)
                {
                    LOGE("Fail to get collider/visual sceneMesh size");
                    sceneMesh3D->clearAllData();
                    return;
                }
                else
                    LOGI("Get sceneMesh index size=%d", buffer0.indexCountOutput);

                std::vector<WVR_Vector3f> vertexBuffer(buffer0.vertexCountOutput);
                std::vector<uint32_t> indexBuffer(buffer0.indexCountOutput);
                WVR_SceneMeshBuffer buffer1 = {buffer0.vertexCountOutput, buffer0.vertexCountOutput, vertexBuffer.data(),
                                               buffer0.indexCountOutput, buffer0.indexCountOutput, indexBuffer.data()};

                // get sceneMesh data
                if (WVR_GetSceneMeshBuffer(meshes[a].meshBufferId, WVR_PoseOriginModel_OriginOnHead, &buffer1) != WVR_Success)
                {
                    LOGE("Fail to get sceneMesh data");
                    sceneMesh3D->clearAllData();
                    return;
                }
                sceneMesh3D->mMeshIndex.push_back(indexBuffer);
                sceneMesh3D->mMeshVertex.push_back(vertexBuffer);
            }
            sceneMesh3D->getOrderMesh();
        }
        else // No need to update
        {
            return;
        }
    }
    else
    {
        LOGI("Scene perception not complete");
        sceneMesh3D->clearAllData();
    }
}
