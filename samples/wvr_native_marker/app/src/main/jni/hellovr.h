// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#pragma once
#include <stdio.h>
#include <string>
#include <vector>

#include <GLES3/gl31.h>
#include "shared/Matrices.h"

#include <wvr/wvr_device.h>
#include <wvr/wvr_events.h>
#include <wvr/wvr_types.h>
#include <ScanLine.h>
#include "Text.h"
#include "markerManager.h"

class Context;
class HandManager;
class Texture;
class Controller;
class FrameBufferObject;
class Object;

class MainApplication
{
public:
    bool deviceSupport;
    WVR_Result passThroughOn = WVR_Error_SystemInvalid;

    MainApplication();
    virtual ~MainApplication();

    bool initVR();
    bool initGL();
    void shutdownVR();
    void shutdownGL();
    void setupCameras();
    bool supportCheck();
    WVR_Result settingPassThrough();
#if defined(USE_CONTROLLER) || defined(USE_CUSTOM_CONTROLLER)
    void setupControllers();
    void handleControllerConnectEvent(bool iIsCtrlerStatusChanged);
#else
    void setupControllerCubes();
    void setupControllerCubeForDevice(WVR_DeviceType deviceType);
    ControllerCube *findOrLoadControllerCube(WVR_DeviceType deviceType);
#endif

    void onResume();
    bool handleInput();
    void processVREvent(const WVR_Event_t &event);
    bool renderFrame();

    void renderStereoTargets();
    void drawControllers();
    void renderScene(WVR_Eye nEye);

    void updateTime();
    void updateHMDMatrixPose();
    void updateEyeToHeadMatrix(bool is6DoF);
    void updateInteractStatus();
    void updateMarker();

    inline Matrix4 wvrmatrixConverter(const WVR_Matrix4f_t &mat) const
    {
        return Matrix4(
            mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
            mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
            mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
            mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]);
    }

protected:
    WVR_DevicePosePair_t mVRDevicePairs[WVR_DEVICE_COUNT_LEVEL_1];

    Matrix4 mDevicePoseArray[WVR_DEVICE_COUNT_LEVEL_1];

    int mControllerCount;
    int mControllerCount_Last;
    int mValidPoseCount;
    int mValidPoseCount_Last;

    std::string mPoseClasses;                     // what classes we saw poses for this frame
    char mDevClassChar[WVR_DEVICE_COUNT_LEVEL_1]; // for each device, a character representing its class

    float mNearClip;
    float mFarClip;

    Matrix4 mHMDPose;
    Matrix4 mEyePosLeft;
    Matrix4 mEyePosRight;

    Matrix4 mProjectionLeft;
    Matrix4 mProjectionRight;

    uint32_t mRenderWidth;
    uint32_t mRenderHeight;

    Controller *mControllerObjs[2];

    uint32_t mIndexLeft;
    uint32_t mIndexRight;

    void *mLeftEyeQ;
    void *mRightEyeQ;

    std::vector<FrameBufferObject *> mLeftEyeFBO;
    std::vector<FrameBufferObject *> mRightEyeFBO;

    std::vector<FrameBufferObject *> mLeftEyeFBOMSAA;
    std::vector<FrameBufferObject *> mRightEyeFBOMSAA;

    Matrix4 mWorldTranslation; // a little backward and upper to avoid been in a cube.
    float mWorldRotation;      // a little backward and upper to avoid been in a cube.
    float mDriveAngle;
    float mDriveSpeed;

    float mTimeDiff;
    uint32_t mTimeAccumulator2S; // add in micro second.
    struct timeval mRtcTime;
    int mFrameCount = 0;
    float mFPS = 0;

    uint32_t mClockCount = 0;

    bool m3DOF;
    bool mMove;

    bool mIs6DoFPose = true;

    Text *mText;
    ScanLine *mScanLine;
    PlaneManager *mPlanes;
    CubeManager *mCubes;
    MarkerManager *mMarkerManager;

    std::vector<float> mLUV{0, 0};
    std::vector<float> mUUV{0, 0};

    WVR_InteractionMode mInteractionMode;

    void switchResolution();
    HandManager *mHandManager;
};