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
        LOGE("addAnchor plane fail (%d)", res);
    }
    else
    {
        LOGI("addAnchor plane success (%d)", res);
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
        finalPose = pm->intersectionPose_world[a] * pm->mapingPoseOnPlane(controllerPose, pm->intersectionPose_world[a]);
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

    // start perception before get scene plane
    // can call WVR_StopScenePerception when you won't need to update data
    // WVR_StopScenePerception will be automatic called when app is closed
    if (WVR_Success != WVR_StartScenePerception(WVR_ScenePerceptionTarget_2dPlane))
    {
        LOGE("WVR_StartScenePerception init fail %d", result);
        return false;
    }

    getExistedAnchor(cm);
    getExistedPlanes(pm);

    return true;
}

void anchorManager::deinit(){
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
        pm->addPlane(convertToMatrix4(mPlane[a].pose), Vector3(mPlane[a].extent.width, mPlane[a].extent.height, 1));
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
            cm->addCubeFromAnchor(a, pose, scale, anchorState.trackingState==WVR_SpatialAnchorTrackingState_Tracking?false:true);
            alreadyHaveAnchor = true;
        }
    }

    return alreadyHaveAnchor;
}

bool anchorManager::updateExistedAnchor(PlaneManager *pm, CubeManager *cm)
{
    bool allAnchorCanTrack = false;
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