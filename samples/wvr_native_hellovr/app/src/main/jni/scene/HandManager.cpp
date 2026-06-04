#define LOG_TAG "APHandModule"

#include <memory>
#include <log.h>

#include "../shared/quat.h"
#include "HandManager.h"

void WVR_PoseState_tToMatrix4(const WVR_PoseState_t &iPoseState, Matrix4 &ioPoseMat)
{
    for (uint32_t r = 0; r < 4; ++r) {
        for (uint32_t c = 0; c < 4; ++c) {
            ioPoseMat[c*4+r] = iPoseState.poseMatrix.m[r][c];
        }
    }
}

void WVR_HandJointData_tToMatrix4(const WVR_Pose_t &iPose, uint64_t iValidBitMask, Matrix4 &ioPoseMat)
{

    q_xyz_quat_struct q_trans = {};

    if ((iValidBitMask & WVR_HandJointValidFlag_RotationValid) == 
            WVR_HandJointValidFlag_RotationValid) {
        q_trans.quat[Q_X] = iPose.rotation.x;
        q_trans.quat[Q_Y] = iPose.rotation.y;
        q_trans.quat[Q_Z] = iPose.rotation.z;
        q_trans.quat[Q_W] = iPose.rotation.w;
    } else {
        q_trans.quat[Q_X] = 0.0;
        q_trans.quat[Q_Y] = 0.0;
        q_trans.quat[Q_Z] = 0.0;
        q_trans.quat[Q_W] = 1.0;
    }

    if ((iValidBitMask & WVR_HandJointValidFlag_PositionValid) == 
            WVR_HandJointValidFlag_PositionValid) {
        q_trans.xyz[0] = iPose.position.v[0];
        q_trans.xyz[1] = iPose.position.v[1];
        q_trans.xyz[2] = iPose.position.v[2];
    } else {
        q_trans.xyz[0] = 0.0;
        q_trans.xyz[1] = 0.0;
        q_trans.xyz[2] = 0.0;
    }

    ioPoseMat = Matrix4(q_trans);
}

Matrix4 WVR_Matrix4fToMatrix4(const WVR_Matrix4f &matPose)
{
    return Matrix4(
        matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], matPose.m[3][0],
        matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], matPose.m[3][1],
        matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], matPose.m[3][2],
        matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], matPose.m[3][3]
    );
}

void ClearWVR_HandTrackerInfo(WVR_HandTrackerInfo_t &ioInfo)
{
    if (ioInfo.jointMappingArray != nullptr) {
        delete[] ioInfo.jointMappingArray;
        ioInfo.jointMappingArray = nullptr;
    }
    if (ioInfo.jointValidFlagArray != nullptr) {
        delete[] ioInfo.jointValidFlagArray;
        ioInfo.jointValidFlagArray = nullptr;
    }

    ioInfo = {};
}


void InitializeWVR_HandTrackerInfo(WVR_HandTrackerInfo_t &ioInfo, uint32_t iJointCount)
{
    //Clear old.
    ClearWVR_HandTrackerInfo(ioInfo);

    if (iJointCount > 0) {
        ioInfo.jointCount = iJointCount;
        ioInfo.jointMappingArray = new WVR_HandJoint[ioInfo.jointCount];
        ioInfo.jointValidFlagArray = new uint64_t[ioInfo.jointCount];
    }
}

void InitializeWVR_HandJointsData(WVR_HandTrackingData_t &ioData)
{
    ioData = {};
    ioData.left.joints = nullptr;
    ioData.right.joints = nullptr;
}

void ClearWVR_HandJointsData(WVR_HandTrackingData_t &ioData)
{
    if (ioData.left.joints != nullptr) {
        delete[] ioData.left.joints;
        ioData.left.joints = nullptr;
    }

    if (ioData.right.joints != nullptr) {
        delete[] ioData.right.joints;
        ioData.right.joints = nullptr;
    }

    ioData = {};
}

void InitializeWVR_HandTrackingData(WVR_HandTrackingData_t &ioData, WVR_HandModelType iHandModelType, const WVR_HandTrackerInfo_t &iTrackerInfo)
{
    InitializeWVR_HandJointsData(ioData);

    if (iTrackerInfo.jointCount > 0) {
        ioData.left.jointCount = iTrackerInfo.jointCount;
        ioData.left.joints = new WVR_Pose_t[iTrackerInfo.jointCount];
        ioData.right.jointCount = iTrackerInfo.jointCount;
        ioData.right.joints = new WVR_Pose_t[iTrackerInfo.jointCount];
    }
}


HandManager::HandManager(WVR_HandTrackerType iType)
: mHandObjs{nullptr, nullptr}
, mTrackingType(iType)
, mHandTrackerInfo({})
, mHandTrackingData({})
, mHandPoseData({})
, mStartFlag(false)
, mHandInitialized(false)
, mIsPinchPoses{false, false}
, mCurrentPinchingStates{false, false}
, mAxes(nullptr)
, mFocusRay(nullptr)
, mFocusPoint(nullptr)
{
    mShift.translate(1,1.5,2);
}

HandManager::~HandManager()
{
}

void HandManager::onCreate()
{
    //createSharedContext();
    
    for (uint32_t hID = 0; hID < Hand_MaxNumber; ++hID) {
        mHandObjs[hID] = new HandObj(this, static_cast<HandTypeEnum>(hID));
        mHandObjs[hID]->initializeGraphicsSystem();
    }

    mAxes = new Axes();
    mAxes->initialize(0.05);

    mFocusRay = new FocusRay();
    mFocusRay->refresh(FocusRayAttribute());

    mFocusPoint = new FocusPoint();
    mFocusPoint->refresh(FocusPointAttribute());
}

void HandManager::onDestroy()
{
    stopHandTracking();

    mAxes->release();
    delete mAxes;
    mAxes = nullptr;

    delete mFocusRay;
    mFocusRay = nullptr;

    delete mFocusPoint;
    mFocusPoint = nullptr;

    for (uint32_t hID = 0; hID < Hand_MaxNumber; ++hID) {
        mHandObjs[hID]->releaseGraphicsSystem();
        delete mHandObjs[hID];
        mHandObjs[hID] = nullptr;
    }
    
    delete mHandAlphaTex;
    mHandAlphaTex = nullptr;
    mHandInitialized = false;

    mPinchPressedCallback = nullptr;
    mPinchReleasedCallback = nullptr;
    //destroySharedContext();
}

void HandManager::createSharedContext()
{
    LOGI("T(%d): %s()++", mTrackingType, __FUNCTION__);

    EGLint ctxAttrs[14];
    int attr = 0;
    ctxAttrs[attr++] = EGL_CONTEXT_CLIENT_VERSION;
    ctxAttrs[attr++] = 3;
    ctxAttrs[attr++] = EGL_CONTEXT_OPENGL_NO_ERROR_KHR;
    ctxAttrs[attr++] = EGL_TRUE;
    ctxAttrs[attr++] = EGL_NONE;

    std::vector<EGLConfig> configs;
    int configNumber;
    eglGetConfigs(eglGetDisplay(EGL_DEFAULT_DISPLAY), nullptr, 0, &configNumber);
    configs.resize(configNumber);
    eglGetConfigs(eglGetDisplay(EGL_DEFAULT_DISPLAY), configs.data(), configs.size(), &configNumber);

    mEGLInitDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    mEGLInitContext = eglCreateContext(
        mEGLInitDisplay,
        configs[0],
        eglGetCurrentContext(),
        ctxAttrs);

    if (mEGLInitContext == EGL_NO_CONTEXT) {
        LOGI("T(%d): create mEGLInitContext failure.", mTrackingType);
    } else {
        LOGI("T(%d): create mEGLInitContext successfully.", mTrackingType);
    }

    LOGI("T(%d): %s()--", mTrackingType, __FUNCTION__);
}

void HandManager::destroySharedContext()
{
    LOGI("T(%d): %s()++", mTrackingType, __FUNCTION__);
    if (mEGLInitContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mEGLInitDisplay, mEGLInitContext);
        mEGLInitContext = EGL_NO_CONTEXT;
        mEGLInitDisplay = EGL_NO_DISPLAY;
    } else {
        LOGI("T(%d): mEGLInitContext is EGL_NO_CONTEXT.", mTrackingType);
    }
    LOGI("T(%d): %s()--", mTrackingType, __FUNCTION__);
}

void HandManager::handleHandTrackingMechanism()
{
    WVR_InteractionMode currentMode = WVR_GetInteractionMode();
    if (WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Left) == true ||
        WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Right) == true) {
        if (mStartFlag == false) {
            startHandTracking();
        }

        if (mHandInitialized == false) {
            loadHandModelAsync();
        }
    }
}

void HandManager::startHandTracking()
{
    WVR_Result result;
    if (mStartFlag == true) {
        LOGE("T(%d): HandManager started!!!", mTrackingType);
        return;
    }

    LOGI("T(%d): AP:startHandTracking()++", mTrackingType);
    uint32_t jointCount = 0u;
    result = WVR_GetHandJointCount(mTrackingType, &jointCount);
    LOGI("T(%d): AP:WVR_GetHandJointCount() done.", mTrackingType);
    if (result != WVR_Success) {
        LOGE("T(%d): WVR_GetHandJointCount failed(%d).", mTrackingType, result);
        return;
    }
    InitializeWVR_HandTrackerInfo(mHandTrackerInfo, jointCount);
    InitializeWVR_HandTrackingData(mHandTrackingData, WVR_HandModelType_WithoutController, mHandTrackerInfo);

    LOGI("T(%d): AP:WVR_GetHandTrackerInfo()", mTrackingType);
    if (WVR_GetHandTrackerInfo(mTrackingType, &mHandTrackerInfo) != WVR_Success) {
        LOGE("T(%d): WVR_GetHandTrackerInfo failed(%d).", mTrackingType, result);
        return;
    }

    LOGI("T(%d): AP:WVR_StartHandTracking()", mTrackingType);
    result = WVR_StartHandTracking(mTrackingType);
    if (result == WVR_Success) {
        mStartFlag.exchange(true);
        LOGI("T(%d): WVR_StartHandTracking successful.", mTrackingType);
    } else {
        LOGE("T(%d): WVR_StartHandTracking error(%d).", mTrackingType, result);
    }
    LOGI("T(%d): AP:startHandTracking()--", mTrackingType);
}

void HandManager::stopHandTracking()
{
    if (!WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Right) || !WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Left)) {
        return;
    }

    mStartFlag.exchange(false);
    WVR_StopHandTracking(WVR_HandTrackerType_Natural);
}


void HandManager::update(DrawModeEnum iMode, size_t iEyeID) {
    if (iMode == DrawMode_Multiview || (iMode == DrawMode_General && iEyeID == 0)) {
        calculateHandMatrices();
    }
}

void HandManager::render(DrawModeEnum iMode, size_t iEyeID,
        const Matrix4 iProjs[DrawMode_MaxModeMumber],
        const Matrix4 iEyes[DrawMode_MaxModeMumber],
        const Matrix4 &iView)
{
    if (iMode == DrawMode_General) {
        for (uint32_t handID = 0; handID < Hand_MaxNumber; ++handID) {
            WVR_Vector3f_t trackingScale = (handID == Hand_Left) ? mHandTrackingData.left.scale : mHandTrackingData.right.scale;
            if (mHandObjs[handID] != nullptr && mIsHandPoseValids[handID] == true) {
                mAxes->renderInner(iProjs[iEyeID], iEyes[iEyeID],
                     iView * mShift, mHandPoseMats[handID]);
                if (mIsPinchPoses[handID] == true) {
                    mFocusRay->render(iProjs[iEyeID], iEyes[iEyeID],
                        iView, mHandRayMats[handID], mHandPinchRatios[handID]);

                    mFocusPoint->render(iProjs[iEyeID], iEyes[iEyeID],
                        iView, mHandRayMats[handID], false, Matrix4(),
                        mFocusPoint->getFarDistance(), mHandPinchRatios[handID]);
                }

                mHandObjs[handID]->updateSkeleton(mJointMats[handID],
                    Vector3(trackingScale.v[0], trackingScale.v[1], trackingScale.v[2]));

                mHandObjs[handID]->render(
                     iProjs[iEyeID], iEyes[iEyeID],
                     iView, mHandPoseMats[handID]);
            }
        }
    } else {

    }
}

void HandManager::calculateHandMatrices()
{
    WVR_Result result = WVR_GetHandTrackingData(
        WVR_HandTrackerType_Natural,
        WVR_HandModelType_WithoutController,
        WVR_PoseOriginModel_OriginOnHead,
        &mHandTrackingData, &mHandPoseData);
                            
    for (uint32_t handType = 0; handType < Hand_MaxNumber; ++handType) {
        WVR_HandJointData_t *handJoints = nullptr;
        WVR_HandPoseState_t *handPose = nullptr;
        if (static_cast<HandTypeEnum>(handType) == Hand_Left) {
            handJoints = &mHandTrackingData.left;
            handPose = &mHandPoseData.left;
        } else {
            handJoints = &mHandTrackingData.right;
            handPose = &mHandPoseData.right;
        }

        //1. Pose data.
        if (handJoints->isValidPose == true) {
            for (uint32_t jCount = 0; jCount < handJoints->jointCount; ++jCount) {
                uint32_t jID = mHandTrackerInfo.jointMappingArray[jCount];
                uint64_t validBits = mHandTrackerInfo.jointValidFlagArray[jCount];
                WVR_HandJointData_tToMatrix4(handJoints->joints[jCount], 
                    validBits, mJointMats[handType][jID]);
            }
            
            if (mIsPrintedSkeErrLog[handType] == true) {
                LOGI("T(%d):Hand[%d] : pose recovered.", mTrackingType, handType);
            }
            mIsPrintedSkeErrLog[handType] = false;
            mIsHandPoseValids[handType] = true;
            mHandPoseMats[handType] = mJointMats[handType][WVR_HandJoint_Wrist];
        } else {
            if (mIsPrintedSkeErrLog[handType] == false) {
                LOGI("T(%d):Hand[%d] : pose invalid.", mTrackingType, handType);
                mIsPrintedSkeErrLog[handType] = true;
            }
            mIsHandPoseValids[handType] = false;
            mHandPoseMats[handType] = Matrix4();
        }

        //2. Ray data.
        if (handPose->base.type == WVR_HandPoseType_Pinch && mTrackingType == WVR_HandTrackerType_Natural) {
            Vector3 rayUp(mHandPoseMats[handType][4], mHandPoseMats[handType][5], mHandPoseMats[handType][6]);
            rayUp.normalize();
            Vector3 rayOri(handPose->pinch.origin.v);
            Vector3 negRayF = (Vector3(handPose->pinch.direction.v) * -1.0f);
            mHandRayMats[handType] = Matrix_LookAtFrom(Vector3(handPose->pinch.origin.v), negRayF, rayUp);
            mHandPinchRatios[handType] = handPose->pinch.strength / 0.75f;
            if (mHandPinchRatios[handType] > 1.0f) {
                mHandPinchRatios[handType] = 1.0f;
            } else if (mHandPinchRatios[handType]  < 0.0f){
                mHandPinchRatios[handType] = 0.0f;
            }
            mIsPinchPoses[handType] = true;
        } else {
            mIsPinchPoses[handType] = false;
        }

        //3. Pinch State
        bool oldPinchingStates = mCurrentPinchingStates[handType];
        mCurrentPinchingStates[handType] = handPose->pinch.isPinching;
        if (oldPinchingStates == true && mCurrentPinchingStates[handType] == false) { //release
            LOGI("T(%d):Hand[%d] : trigger released callback.", mTrackingType, handType);
            if (mPinchReleasedCallback != nullptr) {
                mPinchReleasedCallback(static_cast<HandTypeEnum>(handType));
            }
        } else if (oldPinchingStates == false && mCurrentPinchingStates[handType] == true) { //press
            LOGI("T(%d):Hand[%d] : trigger pressed callback.", mTrackingType, handType);
            if (mPinchPressedCallback != nullptr) {
                mPinchPressedCallback(static_cast<HandTypeEnum>(handType));
            }
        } else {
            //still pressd or released.
        }
    }
}

void HandManager::loadHandModelAsync()
{
    std::lock_guard<std::mutex> lck(mGraphicsChangedMutex);
    LOGI("T(%d): loadHandModelAsync Begin(CSBegin)", mTrackingType);
    WVR_HandRenderModel_t *renderModel = nullptr;
    WVR_GetCurrentNaturalHandModel(&renderModel);

    if (renderModel != nullptr) {
        mHandObjs[Hand_Left]->loadModel(&(*renderModel).left);
        mHandObjs[Hand_Right]->loadModel(&(*renderModel).right);
        mHandAlphaTex = Texture::loadTextureFromBitmapWithoutCached((*renderModel).handAlphaTex);
        mHandObjs[Hand_Left]->setTexture(mHandAlphaTex);
        mHandObjs[Hand_Right]->setTexture(mHandAlphaTex);

        WVR_ReleaseNatureHandModel(&renderModel);
    }
    LOGI("T(%d): loadHandModelAsync End(CSEnd)", mTrackingType);
    mHandInitialized = true;
}