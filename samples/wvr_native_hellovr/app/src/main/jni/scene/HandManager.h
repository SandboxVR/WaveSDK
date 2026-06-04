#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <string>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wvr/wvr_types.h>
#include <wvr/wvr_hand.h>
#include <wvr/wvr_device.h>

#include "DrawEnum.h"

#include "../shared/Matrices.h"
#include "../object/Mesh.h"
#include "../object/Texture.h"
#include "../object/Shader.h"
#include "Axes.h"

#include "HandConstant.h"
#include "HandObj.h"
#include "FocusRay.h"
#include "FocusPoint.h"

typedef std::function<void (HandTypeEnum)> PinchPressedCallback;
typedef std::function<void (HandTypeEnum)> PinchReleasedCallback;
class HandManager
{
public:
    HandManager(WVR_HandTrackerType iTrackingType);
    ~HandManager();
public:
    void onCreate();
    void onDestroy();
public:
    void registerPinchPressedCallback(const PinchPressedCallback &iFunc) {
        mPinchPressedCallback = iFunc;
    }

    void registerPinchReleasedCallback(const PinchReleasedCallback &iFunc) {
        mPinchReleasedCallback = iFunc;
    }

    Matrix4 getHandRayMat(HandTypeEnum iType) const {
        if (iType != Hand_MaxNumber) {
            return mHandRayMats[(uint32_t)iType];
        } else {
            return Matrix4();
        }
    }

    bool isPinchPose(HandTypeEnum iType) const {
        if (iType != Hand_MaxNumber) {
            return mIsPinchPoses[(uint32_t)iType];
        } else {
            return false;
        }
    }

    bool isHandPoseValids(HandTypeEnum iType) const {
        if (iType != Hand_MaxNumber) {
            return mIsHandPoseValids[(uint32_t)iType];
        } else {
            return false;
        }
    }

    void update(DrawModeEnum iMode, size_t iEyeID);
    void render(DrawModeEnum iMode, size_t iEyeID,
        const Matrix4 iProjs[DrawMode_MaxModeMumber],
        const Matrix4 iEyes[DrawMode_MaxModeMumber],
        const Matrix4 &iView);

public:
    void handleHandTrackingMechanism();
protected:
    void startHandTracking();
    void stopHandTracking();
protected:
    void createSharedContext();
    void destroySharedContext();
    void loadHandModelAsync();
protected:
    void calculateHandMatrices();
protected:
    WVR_HandTrackerType mTrackingType;
    WVR_HandTrackerInfo_t mHandTrackerInfo;
    WVR_HandTrackingData_t mHandTrackingData;
    WVR_HandPoseData_t mHandPoseData;
    std::atomic<bool> mStartFlag;
protected:
    bool mIsPrintedSkeErrLog[Hand_MaxNumber];
protected:
    Matrix4 mJointMats[Hand_MaxNumber][sMaxSupportJointNumbers]; //Store mapping-convert poses.
    Matrix4 mHandPoseMats[Hand_MaxNumber];
    bool mIsHandPoseValids[Hand_MaxNumber];
protected:
    Matrix4 mHandRayMats[Hand_MaxNumber];
    float mHandPinchRatios[Hand_MaxNumber];
    bool mCurrentPinchState[Hand_MaxNumber];
    bool mIsPinchPoses[Hand_MaxNumber];
    bool mCurrentPinchingStates[Hand_MaxNumber];
    PinchPressedCallback mPinchPressedCallback;
    PinchReleasedCallback mPinchReleasedCallback;
protected:
    HandObj *mHandObjs[Hand_MaxNumber];
    Texture *mHandAlphaTex;
    Axes *mAxes;
    FocusRay *mFocusRay;
    FocusPoint *mFocusPoint;
    Matrix4 mShift;
    bool mHandInitialized;
protected:
    std::thread mLoadModelThread;
    std::mutex mGraphicsChangedMutex;
    EGLContext mEGLInitContext;
    EGLDisplay mEGLInitDisplay;
};