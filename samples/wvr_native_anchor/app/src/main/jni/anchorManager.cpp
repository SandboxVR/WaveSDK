#include "anchorManager.h"

anchorManager::anchorManager() : mAnchor(nullptr), mPlane(nullptr)
{
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

bool anchorManager::createCubeAnchor(Matrix4 pose, Vector3 scale)
{
    // this example anchor name should be "c=scale.x=scale.y=scale.z"
    std::string planeName("c");
    std::string dash("=");

    WVR_SpatialAnchor anchor;
    WVR_SpatialAnchorCreateInfo info;

    info.originModel = WVR_PoseOriginModel_OriginOnHead;
    info.pose = convertToWVRPose(pose);
    planeName += dash;
    planeName += std::to_string(round(scale.x * 10000) / 10000);
    planeName += dash;
    planeName += std::to_string(round(scale.y * 10000) / 10000);
    planeName += dash;
    planeName += std::to_string(round(scale.z * 10000) / 10000);
    planeName += dash;
    std::copy(planeName.begin(), planeName.end(), info.anchorName.name);
    LOGI("addAnchor cube scale %f, %f, %f", round(scale.x * 10000) / 10000, round(scale.y * 10000) / 10000, round(scale.z * 10000) / 10000);

    WVR_Result res = WVR_CreateSpatialAnchor(&info, &anchor);
    if (res != WVR_Success)
    {
        LOGE("add anchor fail (%d)", res);
    }
    else
    {
        LOGI("add anchor success (%d)", res);
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

bool anchorManager::addUndefineCubeAnchor(CubeManager *cm)
{
    for (int a = 0; a < cm->undefineCubes.size(); a++)
    {
        createCubeAnchor(cm->undefineCubes[a].mPose, cm->undefineCubes[a].mScale);
        LOGI("add cube scale[%d] %f %f %f", a, cm->undefineCubes[a].mScale.x, cm->undefineCubes[a].mScale.y, cm->undefineCubes[a].mScale.z);
    }
    return true;
}

bool anchorManager::addCubeAnchor(PlaneManager *pm, Matrix4 controllerPose, Vector3 scale)
{
    // need to rotate to align with plane coordinate
    Matrix4 r90;
    Matrix4 finalPose;
    r90.rotateX(90);
    for (int a = 0; a < pm->intersectionPose_world.size(); a++)
    {
        finalPose = pm->intersectionPose_world[a] * pm->mappingPoseOnPlane(controllerPose, pm->intersectionPose_world[a]);
        finalPose = finalPose * r90;
        createCubeAnchor(finalPose, scale);
    }
    return true;
}

bool anchorManager::clearAnchor()
{
    LOGI("clean anchor %d", anchorCount);
    for (int a = 0; a < anchorCount; a++)
    {
        WVR_DestroySpatialAnchor(*(mAnchor + a));
    }
    return true;
}

bool anchorManager::deleteChosenAnchor(CubeManager *cm)
{
    for (int a = 0; a < cm->leftTouchVector.size(); a++)
    {
        // delete anchor
        WVR_DestroySpatialAnchor(*(mAnchor + cm->leftTouchVector[a].anchorIndex));
    }
    return true;
}

bool anchorManager::deleteChosenAnchor2changing(CubeManager *cm)
{
    for (int a = 0; a < cm->rightTouchVector.size(); a++)
    {
        // delete anchor
        WVR_DestroySpatialAnchor(*(mAnchor + cm->rightTouchVector[a].anchorIndex));
    }
    cm->addUndefineCube();
    return true;
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

    getExistedAnchor(cm);
    getExistedPlanes(pm);

    return true;
}

void anchorManager::startScenePerception(){
    // start perception before get scene plane/mesh (need permission)
    // can call WVR_StopScenePerception when you won't need to update data
    // WVR_StopScenePerception will be automatic called when app is closed
    WVR_Result startPerceptionResult_2d = WVR_StartScenePerception(WVR_ScenePerceptionTarget_2dPlane);
    WVR_Result startPerceptionResult_3d = WVR_StartScenePerception(WVR_ScenePerceptionTarget_SceneMesh);

    if (WVR_Success != startPerceptionResult_2d || WVR_Success != startPerceptionResult_3d)
        LOGE("WVR_StartScenePerception init fail");
}

    void
    anchorManager::deinit()
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
    WVR_GetScenePlanes(nullptr, 0, &planeCount, WVR_PoseOriginModel_OriginOnHead, nullptr); // dead API on SUE
    mPlane = (WVR_ScenePlane *)malloc(sizeof(WVR_ScenePlane) * planeCount);
    WVR_GetScenePlanes(nullptr, planeCount, &planeCount, WVR_PoseOriginModel_OriginOnHead, mPlane); // dead API on SUE
    LOGI("get scene =%u", planeCount);

    // copy to vector
    for (int a = 0; a < planeCount; a++)
    {
        pm->addPlane(a, convertToMatrix4(mPlane[a].pose), Vector3(mPlane[a].extent.width, mPlane[a].extent.height, 1));
    }

    return true;
}

bool anchorManager::getExistedAnchor(CubeManager *cm)
{
    // WVR API get anchor num and allocate memory to get data
    WVR_EnumerateSpatialAnchors(0, &anchorCount, nullptr);
    // clear memory
    if (mAnchor != nullptr)
        free(mAnchor);
    cm->clearCubes();
    // assign memory space for anchor
    mAnchor = (WVR_SpatialAnchor *)malloc(sizeof(WVR_SpatialAnchor) * anchorCount);
    LOGI("get anchor anchorCount=%u", anchorCount);
    WVR_EnumerateSpatialAnchors(anchorCount, &anchorCount, mAnchor);
    bool alreadyHaveAnchor = false;

    // check anchor name
    for (int a = 0; a < anchorCount; a++)
    {
        Matrix4 pose;
        Vector3 scale;
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(mAnchor[a], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        std::string mName(anchorState.anchorName.name);
        if (mName[0] == 'c')
        {
            pose = convertToMatrix4(anchorState.pose);
            scale = getScaleFromName(mName);
            cm->addCubeFromAnchor(a, pose, scale, anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking ? false : true);
            alreadyHaveAnchor = true;
        }
    }

    return alreadyHaveAnchor;
}

bool anchorManager::updateExistedAnchor(CubeManager *cm)
{
    bool allAnchorCanTrack = false;
    LOGI("Anchor cubeVector size= %d", cm->cubeVector.size());
    for (int a = 0; a < cm->cubeVector.size(); a++)
    {
        Matrix4 pose;
        WVR_SpatialAnchorState anchorState;
        WVR_GetSpatialAnchorState(mAnchor[cm->cubeVector[a]->anchorIndex], WVR_PoseOriginModel_OriginOnHead, &anchorState);
        if (anchorState.trackingState == WVR_SpatialAnchorTrackingState_Tracking)
        {
            allAnchorCanTrack = true;
            pose = convertToMatrix4(anchorState.pose);
            cm->cubeVector[a]->mPose = pose;
            cm->cubeVector[a]->anchorLost = false;
        }
        else
        {
            cm->cubeVector[a]->anchorLost = true;
            LOGI("Anchor cubeVector(%d) lost tracking", a);
        }
    }
    return allAnchorCanTrack;
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
