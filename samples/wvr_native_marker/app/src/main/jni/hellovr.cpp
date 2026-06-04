// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#include <log.h>
#include <string.h>
#include <unistd.h>
#include <limits>
#include <sys/types.h>
#include <sys/time.h>
#include <cstdlib>
#include <math.h>
#include <Texture.h>
#include <Controller.h>
#include <FrameBufferObject.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include <wvr/wvr.h>
#include <wvr/wvr_hand_render_model.h>
#include <wvr/wvr_ctrller_render_model.h>
#include <wvr/wvr_render.h>
#include <wvr/wvr_device.h>
#include <wvr/wvr_projection.h>
#include <wvr/wvr_overlay.h>
#include <wvr/wvr_system.h>
#include <wvr/wvr_events.h>

#include "scene/HandManager.h"

#include "hellovr.h"

// Return micro second.  Should always positive because now is bigger.
#define timeval_subtract(now, last) \
    ((now.tv_sec - last.tv_sec) * 1000000LL + now.tv_usec - last.tv_usec)

#undef LOGENTRY
#define LOGENTRY(...)

bool sceneMeshPermissionGranted = false;
bool gDebug = true;
bool gDebugOld = gDebug;
bool gMsaa = true;
bool gScene = false;
bool gSceneOld = gScene;
bool gUseScale = true;
float gScale = 1;

#define LOGDIF(args...) \
    if (gDebug)         \
    LOGD(args)

#define VR_MAX_CLOCKS 200

// To demonstrate how to use heavy-effect foveated rendering
#define ENABLE_LOW_FOVEATED_RENDERING 0

// To demonstrate how to use WaveVR AdaptiveQuality
#define DISABLE_ADAPTIVE_QUALITY 0

static void dumpMatrix(const char *name, const Matrix4 &mat)
{
    const float *ptr = mat.get();
    LOGD("%s =\n"
         " ⎡%+6f  %+6f  %+6f  %+6f⎤\n"
         " ⎢%+6f  %+6f  %+6f  %+6f⎥\n"
         " ⎢%+6f  %+6f  %+6f  %+6f⎥\n"
         " ⎣%+6f  %+6f  %+6f  %+6f⎦\n",
         name,
         ptr[0], ptr[4], ptr[8], ptr[12],
         ptr[1], ptr[5], ptr[9], ptr[13],
         ptr[2], ptr[6], ptr[10], ptr[14],
         ptr[3], ptr[7], ptr[11], ptr[15]);
}

MainApplication::MainApplication()
    : deviceSupport(false), mControllerCount(0), mControllerCount_Last(-1), mValidPoseCount(0),
      mValidPoseCount_Last(-1), mPoseClasses(""), m3DOF(true), mMove(true), mTimeDiff(0.0f),
      mDriveSpeed(0.0f), mDriveAngle(0.0f), mIndexLeft(0), mIndexRight(0), mLeftEyeQ(NULL), mRightEyeQ(NULL)
{
    // other initialization tasks are done in init
    memset(mDevClassChar, 0, sizeof(mDevClassChar));
    mControllerObjs[0] = nullptr;
    mControllerObjs[1] = nullptr;

    LOGI("MainApplication::MainApplication()");
}

MainApplication::~MainApplication()
{
    // work is done in Shutdown
    LOGI("Shutdown");
}

static void printGLString(const char *name, GLenum s)
{
    const char *v = (const char *)glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

void MainApplication::onResume()
{
    if (mMarkerManager->getObserveState() != WVR_MarkerObserverState_Detecting)
    {
        mMarkerManager->refreshTrackableMarker();
    }
    else
    {
        mMarkerManager->getDetectingData();
    }
}

bool MainApplication::initVR()
{
    LOGENTRY();
    LOGI("MainApplication::initVR()");

    // Loading the WVR Runtime
    WVR_InitError eError = WVR_InitError_None;
    eError = WVR_Init(WVR_AppType_VRContent);
    if (eError != WVR_InitError_None)
    {
        LOGE("Unable to init VR runtime: %s", WVR_GetInitErrorString(eError));
        return false;
    }

    //---------- Full key mapping Sample (you can receive all keyevent)--------
    WVR_InputAttribute array[31];
    for (int i = 0; i < sizeof(array) / sizeof(*array); i++)
    {
        array[i].id = (WVR_InputId)(i + 1);
        array[i].capability = WVR_InputType_Button | WVR_InputType_Touch | WVR_InputType_Analog;
        array[i].axis_type = WVR_AnalogType_1D;
    }
    WVR_SetInputRequest(WVR_DeviceType_HMD, array, sizeof(array) / sizeof(*array));
    WVR_SetInputRequest(WVR_DeviceType_Controller_Right, array, sizeof(array) / sizeof(*array));
    WVR_SetInputRequest(WVR_DeviceType_Controller_Left, array, sizeof(array) / sizeof(*array));

    WVR_SetArmModel(WVR_SimulationType_Auto); // add for use arm mode.

    // Must initialize render runtime before all OpenGL code.
    WVR_RenderInitParams_t param;
    param = {WVR_GraphicsApiType_OpenGL, WVR_RenderConfig_Default};

    WVR_RenderError pError = WVR_RenderInit(&param);
    if (pError != WVR_RenderError_None)
    {
        LOGE("Present init failed - Error[%d]", pError);
    }

    mInteractionMode = WVR_GetInteractionMode();

    mMarkerManager = new MarkerManager();
    mMarkerManager->startMarker();

    return true;
}

bool MainApplication::initGL()
{
    LOGENTRY();
    mNearClip = 0.1f;
    mFarClip = 30.0f;
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(true);
    mLUV[0] = 0.0f;
    mLUV[1] = 0.0f;
    mUUV[0] = gScale;
    mUUV[1] = gScale;
    GLenum glerr = glGetError();
    if (glerr != GL_NO_ERROR)
    {
        LOGE("glGetError() before initGL: %d", glerr);
    }

#define OBJ_ERROR_CHECK(obj)                  \
    if (obj->hasError() || obj->hasGLError()) \
    return false

    mText = new Text(0.01f, false);
    mText->setEnable(true);

    mScanLine = new ScanLine();

    mControllerObjs[0] = new Controller(WVR_DeviceType_Controller_Right);
    mControllerObjs[0]->mCursorShift.translate(-0.011, -0.028, 0);
    mControllerObjs[1] = new Controller(WVR_DeviceType_Controller_Left);
    mControllerObjs[1]->mCursorShift.translate(0.011, -0.028, 0);

    mPlanes = new PlaneManager();
    mCubes = new CubeManager();

    setupCameras();

    // Setup stereo render targets
    WVR_GetRenderTargetSize(&mRenderWidth, &mRenderHeight);
    LOGD("Recommended size is %ux%u", mRenderWidth, mRenderHeight);
    if (mRenderWidth == 0 || mRenderHeight == 0)
    {
        LOGE("Please check server configure");
        return false;
    }

    mIndexLeft = 0;
    mIndexRight = 0;

    mLeftEyeQ = WVR_ObtainTextureQueue(WVR_TextureTarget_2D, WVR_TextureFormat_RGBA, WVR_TextureType_UnsignedByte, mRenderWidth, mRenderHeight, 0);
    for (int i = 0; i < WVR_GetTextureQueueLength(mLeftEyeQ); i++)
    {
        FrameBufferObject *fbo;

        fbo = new FrameBufferObject((int)(long)WVR_GetTexture(mLeftEyeQ, i).id, mRenderWidth, mRenderHeight, true);
        if (!fbo)
            return false;
        if (fbo->hasError())
        {
            delete fbo;
            return false;
        }
        mLeftEyeFBOMSAA.push_back(fbo);

        fbo = new FrameBufferObject((int)(long)WVR_GetTexture(mLeftEyeQ, i).id, mRenderWidth, mRenderHeight);
        if (!fbo)
            return false;
        if (fbo->hasError())
        {
            delete fbo;
            return false;
        }
        mLeftEyeFBO.push_back(fbo);
    }
    mRightEyeQ = WVR_ObtainTextureQueue(WVR_TextureTarget_2D, WVR_TextureFormat_RGBA, WVR_TextureType_UnsignedByte, mRenderWidth, mRenderHeight, 0);
    for (int i = 0; i < WVR_GetTextureQueueLength(mRightEyeQ); i++)
    {
        FrameBufferObject *fbo;

        fbo = new FrameBufferObject((int)(long)WVR_GetTexture(mRightEyeQ, i).id, mRenderWidth, mRenderHeight, true);
        if (!fbo)
            return false;
        if (fbo->hasError())
        {
            delete fbo;
            return false;
        }
        mRightEyeFBOMSAA.push_back(fbo);

        fbo = new FrameBufferObject((int)(long)WVR_GetTexture(mRightEyeQ, i).id, mRenderWidth, mRenderHeight);
        if (!fbo)
            return false;
        if (fbo->hasError())
        {
            delete fbo;
            return false;
        }
        mRightEyeFBO.push_back(fbo);
    }

    setupControllers();

#undef OBJ_ERROR_CHECK

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

#if ENABLE_LOW_FOVEATED_RENDERING
    WVR_RenderFoveationMode(WVR_FoveationMode_Enable);
#endif

#if defined(DISABLE_ADAPTIVE_QUALITY) && DISABLE_ADAPTIVE_QUALITY
    // 1. WaveVR AQ is enabled with WVR_QualityStrategy_SendQualityEvent by default from WaveVR SDK 3.2.0
    // 2. WVR_EnableAdaptiveQuality must be invoked after WVR_RenderInit().
    // 3. If you don't want to use AQ, you need to disable it.
    WVR_EnableAdaptiveQuality(false);

    /* There are some strategies in WaveVR AQ can to choose.
     *
     * EX: Use auto foveated rendering to improve performance while it is not good enough.
     * WVR_EnableAdaptiveQuality(true, WVR_QualityStrategy_AutoFoveation);
     *
     * EX: Receive recommended quailty chagned event and auto foveated rendering to improve performance while
     *     it is not good enough.
     * WVR_EnableAdaptiveQuality(true, WVR_QualityStrategy_SendQualityEvent | WVR_QualityStrategy_AutoFoveation);
     */
#endif
    mHandManager = new HandManager(WVR_HandTrackerType_Natural);
    mHandManager->onCreate();

    return true;
}

void MainApplication::shutdownGL()
{
    LOGENTRY();

    mHandManager->onDestroy();
    delete mHandManager;
    mHandManager = nullptr;

    for (uint32_t cID = 0; cID < 2; ++cID)
    {
        if (mControllerObjs[cID] != nullptr)
        {
            delete mControllerObjs[cID];
        }
        mControllerObjs[cID] = nullptr;
    }

    if (mLeftEyeQ != 0)
    {
        for (int i = 0; i < WVR_GetTextureQueueLength(mLeftEyeQ); i++)
        {
            delete mLeftEyeFBOMSAA.at(i);
            delete mLeftEyeFBO.at(i);
        }
        WVR_ReleaseTextureQueue(mLeftEyeQ);
    }

    if (mRightEyeQ != 0)
    {
        for (int i = 0; i < WVR_GetTextureQueueLength(mRightEyeQ); i++)
        {
            delete mRightEyeFBOMSAA.at(i);
            delete mRightEyeFBO.at(i);
        }
        WVR_ReleaseTextureQueue(mRightEyeQ);
    }
}

void MainApplication::shutdownVR()
{
    mMarkerManager->stopMarker();
    WVR_Quit();
}

class ControllerState
{
private:
    bool mLastTouchpadPressed;
    bool mTouchpadClicked;
    bool mMenuClicked;
    bool mLastMenuPressed;

public:
    ControllerState() : mLastTouchpadPressed(false), mTouchpadClicked(false), mLastMenuPressed(false), mMenuClicked(false)
    {
    }

    void updateState(WVR_DeviceType deviceType)
    {
        bool bTouchpadPressed = WVR_GetInputButtonState(deviceType, WVR_InputId_Alias1_Touchpad);
        if (mLastTouchpadPressed && (bTouchpadPressed) == 0)
        {
            mTouchpadClicked = true;
        }
        else
        {
            mTouchpadClicked = false;
        }
        mLastTouchpadPressed = bTouchpadPressed;

        bool bMenuPressed = WVR_GetInputButtonState(deviceType, WVR_InputId_Alias1_Menu);
        if (mLastMenuPressed && (bMenuPressed) == 0)
        {
            mMenuClicked = true;
        }
        else
        {
            mMenuClicked = false;
        }
        mLastMenuPressed = bMenuPressed;
    }

    inline bool touchpadClicked() const
    {
        return mTouchpadClicked;
    }

    inline bool menuClicked() const
    {
        return mMenuClicked;
    }
};

//-----------------------------------------------------------------------------
// Purpose: Poll events.  Quit application if return true.
//-----------------------------------------------------------------------------
bool MainApplication::handleInput()
{
    LOGENTRY();
    static ControllerState states[WVR_DEVICE_COUNT_LEVEL_1];

    bool resolutionChange = false;

    if (gScene != gSceneOld)
    {
        gSceneOld = gScene;
    }

    if (gDebug != gDebugOld)
    {
        gDebugOld = gDebug;
        setupCameras();
    }

    // Process WVR events
    bool isCtrlerStatusChange = false;
    WVR_Event_t event;

    while (WVR_PollEventQueue(&event))
    {
        if (event.common.type == WVR_EventType_Quit)
        {
            return true;
        }

        if (event.common.type == WVR_EventType_DeviceConnected ||
            event.common.type == WVR_EventType_DeviceDisconnected)
        {
            isCtrlerStatusChange = true;
            if (event.common.type == WVR_EventType_DeviceConnected)
            {
                LOGI("WVR_EventType_DeviceConnected");
            }
            else
            {
                LOGI("WVR_EventType_DeviceDisconnected");
            }
        }

        processVREvent(event);
        handleControllerConnectEvent(isCtrlerStatusChange);
        mHandManager->handleHandTrackingMechanism();

        if (event.common.type == WVR_EventType_ButtonPressed)
        {
            if (event.device.deviceType == WVR_DeviceType_Controller_Right && event.input.inputId == WVR_InputId_Alias1_Trigger)
            {
                LOGI("BTCheck right WVR_InputId_Alias1_Trigger (add aruco to trackable)");
                mMarkerManager->addTrackableMarker(mPlanes->nearestRightSelect);
            }
            if (event.device.deviceType == WVR_DeviceType_Controller_Left && event.input.inputId == WVR_InputId_Alias1_Trigger)
            {
                LOGI("BTCheck left WVR_InputId_Alias1_Trigger (delete trackable)");
                if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
                {
                    LOGI("left D");
                    mMarkerManager->deleteTrackableMarker(mPlanes->nearestLeftSelect);
                }
                else
                {
                    LOGI("left T");
                    mMarkerManager->stopTrackingTrackableMarker(mCubes->nearestLeftSelect);
                }
            }
            if (event.input.inputId == WVR_InputId_Alias1_X)
            {
                static bool stopTrack = true;
                LOGI("BTCheck WVR_InputId_Alias1_X (Stop/Start all trackable / Clear all trackable)");
                if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
                {
                    mMarkerManager->deleteAllTrackableMarker();
                }
                else
                {
                    if (stopTrack == true)
                        mMarkerManager->stopTrackingAllTrackableMarker();
                    else
                        mMarkerManager->startTrackingAllTrackableMarker();

                    stopTrack = !stopTrack;
                }
            }
            if (event.input.inputId == WVR_InputId_Alias1_Y)
            {
                LOGI("BTCheck WVR_InputId_Alias1_Y (passThrough on/off)");
                static bool showUnderlay = true;
                WVR_ShowPassthroughUnderlay(showUnderlay);
                showUnderlay = !showUnderlay;
            }
            if (event.input.inputId == WVR_InputId_Alias1_A)
            {
                LOGI("BTCheck WVR_InputId_Alias1_A (Start/Stop detecting)");
                if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
                {
                    mMarkerManager->tryTrackingTrackableMarker();
                }
                else
                {
                    mMarkerManager->tryDetectingArucoMarker();
                }
            }
        }
    }
    if (resolutionChange)
    {
        switchResolution();
    }
    return false;
}

void MainApplication::switchResolution()
{
#if ENABLE_LOW_FOVEATED_RENDERING
    LOGI("menu key pressed");
#else
    if (gUseScale == true)
    {
        if (std::abs(gScale - 1.0) <= std::numeric_limits<float>::epsilon())
        {
            gScale = 0.5;
        }
        else
        {
            gScale += 0.1;
        }
        mUUV[0] = gScale;
        mUUV[1] = gScale;

        FrameBufferObject *fbo = NULL;

        for (int i = 0; i < WVR_GetTextureQueueLength(mLeftEyeQ); i++)
        {
            fbo = gMsaa ? mLeftEyeFBOMSAA.at(i) : mLeftEyeFBO.at(i);
            fbo->resizeFrameBuffer(gScale);
        }

        for (int i = 0; i < WVR_GetTextureQueueLength(mRightEyeQ); i++)
        {
            fbo = gMsaa ? mRightEyeFBOMSAA.at(i) : mRightEyeFBO.at(i);
            fbo->resizeFrameBuffer(gScale);
        }
    }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Processes a single VR event
//-----------------------------------------------------------------------------
void MainApplication::processVREvent(const WVR_Event_t &event)
{
    switch (event.common.type)
    {
    case WVR_EventType_DeviceConnected:
    {
#if defined(USE_CONTROLLER) || defined(USE_CUSTOM_CONTROLLER)
#else
        setupControllerCubeForDevice(event.device.deviceType);
        LOGD("Device %u attached. Setting up controller cube.\n", event.device.deviceType);
#endif
    }
    break;
    case WVR_EventType_ButtonPressed:
    case WVR_EventType_ButtonUnpressed:
    case WVR_EventType_TouchTapped:
    case WVR_EventType_TouchUntapped:
    {
#if defined(USE_CONTROLLER)
        WVR_DeviceType ctrlType = event.device.deviceType;
        LOGI("Device Btn Event:C(%d)EType(%d)", ctrlType, event.common.type);
        for (uint32_t cID = 0; cID < 2; ++cID)
        {
            if (mControllerObjs[cID] != nullptr)
            {
                if (mControllerObjs[cID]->isThisCtrlerType(ctrlType) == true)
                {
                    mControllerObjs[cID]->refreshButtonStatus(event);
                }
            }
        }
#else
#endif
    }
    break;
    case WVR_EventType_DeviceRoleChanged:
    {
#if defined(USE_CONTROLLER) || defined(USE_CUSTOM_CONTROLLER)
        LOGI("DeviceRoleChanged.");
        for (uint32_t cID = 0; cID < 2; ++cID)
        {
            if (mControllerObjs[cID] != nullptr)
            {
                mControllerObjs[cID]->switchCtrlerType();
            }
        }
#else
        LOGD("Device %u role change.\n", event.device.deviceType);
#endif
    }
    break;
    case WVR_EventType_DeviceStatusUpdate:
    {
        LOGD("Device %u updated.\n", event.device.deviceType);
    }
    break;
    case WVR_EventType_IpdChanged:
    {
        WVR_RenderProps_t props;
        bool ret = WVR_GetRenderProps(&props);
        float ipd = 0;
        if (ret)
        {
            ipd = props.ipdMeter;
        }
        LOGI("IPD is changed (%.4f) and renew the transform from eye to head.", ipd);
        mProjectionLeft = wvrmatrixConverter(
            WVR_GetProjection(WVR_Eye_Left, mNearClip, mFarClip));
        mProjectionRight = wvrmatrixConverter(
            WVR_GetProjection(WVR_Eye_Right, mNearClip, mFarClip));

        mEyePosLeft = wvrmatrixConverter(
                          WVR_GetTransformFromEyeToHead(WVR_Eye_Left))
                          .invert();
        mEyePosRight = wvrmatrixConverter(
                           WVR_GetTransformFromEyeToHead(WVR_Eye_Right))
                           .invert();

        dumpMatrix("ProjectionLeft", mProjectionLeft);
        dumpMatrix("ProjectionRight", mProjectionRight);
        dumpMatrix("EyePosLeft", mEyePosLeft);
        dumpMatrix("EyePosRight", mEyePosRight);
    }
    break;
    case WVR_EventType_InteractionModeChanged:
    {
        mInteractionMode = WVR_GetInteractionMode();
        LOGI("Receive WVR_EventType_InteractionModeChanged mode = %d", mInteractionMode);
    }
    break;
        // If WaveVR AQ have enabled with WVR_QualityStrategy_SendQualityEvent, you will receive
        // the wvr recommendedQuality_Lower/Higher event based on rendering performance.
    case WVR_EventType_RecommendedQuality_Lower:
    {
        /* Once you got this recommend event:
         * 1. If your conetent rendering quality has already in the worst quality, you can ignore this event.
         * 2. Or, you can adjust the rendering resolution lower gradually by re-create texture queue, disable MSAA, etc.
         */
        LOGI("[Sample] Get WVR_EventType_RecommendedQuality_Lower");
    }
    break;
    case WVR_EventType_RecommendedQuality_Higher:
    {
        /* 1. Once you got this recommend event:
         * 1. If your conetent rendering quality has already in the best quality, you can ignore this event.
         * 2. Or, you can adjust the rendering resolution higher ASAP by re-create texture queue, enable MSAA, etc.
         */
        LOGI("[Sample] Get WVR_EventType_RecommendedQuality_Higher");
    }
    break;
    default:
        break;
    }
}

void MainApplication::handleControllerConnectEvent(bool iIsCtrlerStatusChanged)
{
    // 1. check controller connection state, if controller is connected, we loading. Otherwise, we release.
    //*** reload empty controller won't memory leak because we will release resource before we initialize graphics res.
    //*** release empty controller won't cause crash because we will check validation resource is exist before we release.
    if (iIsCtrlerStatusChanged == true)
    {
        LOGI("[APCtrler] Trigger connect or disconnect.");
        for (uint32_t cID = 0; cID < 2; ++cID)
        {
            if (mControllerObjs[cID] != nullptr)
            {
                // 2.1 get ctrler device type.
                WVR_DeviceType ctrlerType = mControllerObjs[cID]->getCtrlerType();
                bool ctrlerConStatus = WVR_IsDeviceConnected(ctrlerType);
                if (ctrlerConStatus == true)
                { // connect
                    mControllerObjs[cID]->loadControllerModelAsync();
                }
            }
        }
    }
}

bool MainApplication::supportCheck()
{
    uint64_t supported = WVR_GetSupportedFeatures();

    // check feature
    if (!(supported & WVR_SupportedFeature_PassthroughOverlay))
    {
        LOGE("WVR_SupportedFeature_PassthroughOverlay is not supported");
        return false;
    }
    if (!(supported & WVR_SupportedFeature_Marker))
    {
        LOGE("WVR_SupportedFeature_Marker is not supported");
        return false;
    }
    deviceSupport = true;
    return true;
}

WVR_Result MainApplication::settingPassThrough()
{
    if (passThroughOn != WVR_Success)
    {
        passThroughOn = WVR_ShowPassthroughUnderlay(true);
    }
    return passThroughOn;
}

bool MainApplication::renderFrame()
{
    LOGENTRY();

    unsigned int ext = WVR_SubmitExtend_Default;

    mIndexLeft = WVR_GetAvailableTextureIndex(mLeftEyeQ);
    mIndexRight = WVR_GetAvailableTextureIndex(mRightEyeQ);

    // for now as fast as possible
    drawControllers();
    renderStereoTargets();

    ext |= WVR_SubmitExtend_Default;
#if ENABLE_LOW_FOVEATED_RENDERING
#else
    if (gScale < 1 && gScale > 0)
        ext |= WVR_SubmitExtend_PartialTexture;
#endif

    WVR_TextureParams_t leftEyeTexture = WVR_GetTexture(mLeftEyeQ, mIndexLeft);
    WVR_SubmitError e;

    leftEyeTexture.layout.leftLowUVs.v[0] = 0;
    leftEyeTexture.layout.leftLowUVs.v[1] = 0;
    leftEyeTexture.layout.rightUpUVs.v[0] = 1;
    leftEyeTexture.layout.rightUpUVs.v[1] = 1;
#if ENABLE_LOW_FOVEATED_RENDERING
#else
    if (gScale < 1 && gScale > 0)
    {
        leftEyeTexture.layout.leftLowUVs.v[0] = mLUV[0];
        leftEyeTexture.layout.leftLowUVs.v[1] = mLUV[1];
        leftEyeTexture.layout.rightUpUVs.v[0] = mUUV[0];
        leftEyeTexture.layout.rightUpUVs.v[1] = mUUV[1];
    }
#endif

    e = WVR_SubmitFrame(WVR_Eye_Left, &leftEyeTexture, &(mVRDevicePairs[WVR_DEVICE_HMD].pose), (WVR_SubmitExtend)ext);
    if (e != WVR_SubmitError_None)
        return true;

    // Right eye
    WVR_TextureParams_t rightEyeTexture = WVR_GetTexture(mRightEyeQ, mIndexRight);

    rightEyeTexture.layout.leftLowUVs.v[0] = 0;
    rightEyeTexture.layout.leftLowUVs.v[1] = 0;
    rightEyeTexture.layout.rightUpUVs.v[0] = 1;
    rightEyeTexture.layout.rightUpUVs.v[1] = 1;

#if ENABLE_LOW_FOVEATED_RENDERING
#else
    if (gScale < 1 && gScale > 0)
    {
        rightEyeTexture.layout.leftLowUVs.v[0] = mLUV[0];
        rightEyeTexture.layout.leftLowUVs.v[1] = mLUV[1];
        rightEyeTexture.layout.rightUpUVs.v[0] = mUUV[0];
        rightEyeTexture.layout.rightUpUVs.v[1] = mUUV[1];
    }
#endif

    e = WVR_SubmitFrame(WVR_Eye_Right, &rightEyeTexture, &(mVRDevicePairs[WVR_DEVICE_HMD].pose), (WVR_SubmitExtend)ext);
    if (e != WVR_SubmitError_None)
        return true;

    updateTime();

    // Clear
    {
        // We want to make sure the glFinish waits for the entire present to complete, not just the submission
        // of the command. So, we do a clear here right here so the glFinish will wait fully for the swap.
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Spew out the controller and pose count whenever they change.
    if (mControllerCount != mControllerCount_Last || mValidPoseCount != mValidPoseCount_Last)
    {
        mValidPoseCount_Last = mValidPoseCount;
        mControllerCount_Last = mControllerCount;

        LOGD("PoseCount:%d(%s) Controllers:%d\n", mValidPoseCount, mPoseClasses.c_str(), mControllerCount);
    }

    usleep(1);
    // LOGD("renderFrame end");

    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Draw all of the controllers as X/Y/Z lines
//-----------------------------------------------------------------------------
void MainApplication::drawControllers()
{
    // don't draw controllers if somebody else has input focus
    if (WVR_IsInputFocusCapturedBySystem())
        return;

    std::vector<float> buffer;

    int vertCount = 0;
    mControllerCount = 0;
    WVR_DeviceType type;
    for (uint32_t id = WVR_DEVICE_HMD + 1; id < WVR_DEVICE_COUNT_LEVEL_1; ++id)
    {
        if ((mVRDevicePairs[id].type != WVR_DeviceType_Controller_Right) && (mVRDevicePairs[id].type != WVR_DeviceType_Controller_Left))
        {
            //            LOGD("drawControllers(): not Controller : %d ", mVRDevicePairs[id].type);
            continue;
        }

        if (!WVR_IsDeviceConnected(mVRDevicePairs[id].type))
        {
            //            LOGD("drawControllers(): DeviceType: %d pose is disconnected :", mVRDevicePairs[id].type);
            continue;
        }

        if (!mVRDevicePairs[id].pose.isValidPose)
        {
            //            LOGD("drawControllers(): DeviceType: %d pose is invalid", mVRDevicePairs[id].type);
            continue;
        }

        Matrix4 mat;
        if (m3DOF)
        {
            // If the controller is 3DOF always put the model in the bottom of the view.
            mat = mDevicePoseArray[WVR_DEVICE_HMD];
            mat.invert();
            float angleY = atan2f(-mat[8], mat[10]); // Yaw
            float angleX = asin(-mat[9]);            // Pitch
            float angleZ = atan2f(mat[1], mat[5]);   // Roll

            mat.identity().rotateY(-angleY / M_PI * 180.0f);
            mat.rotateX(angleX / M_PI * 180.0f);
            mat.rotateZ(angleZ / M_PI * 180.0f);
            mat *= mDevicePoseArray[id];

            int offset = mVRDevicePairs[id].type - WVR_DeviceType_Controller_Right;
            float x = (offset % 2) == 0 ? 0.1f : -0.1f;
            float z = -0.45f - (offset / 2) * 0.3f;
            mat.setColumn(3, Vector4(x, -0.12f, z, 1));
        }
        else
        {
            mat = mDevicePoseArray[id];
        }
        mControllerCount += 1;

        Matrix4 WorldFromHead;
        WorldFromHead = mDevicePoseArray[WVR_DEVICE_HMD];
        Matrix4 WorldFromController_new = WorldFromHead * mat;

        Matrix4 mat4WorldRotation;
        mat4WorldRotation.rotate(mWorldRotation, 0, 1, 0); // if world is rotated , the ray of controller is also changed too.

        uint32_t ctrlerRealID = 0;
        for (uint32_t cID = 0; cID < 2; ++cID)
        {
            if (mControllerObjs[cID] != nullptr)
            {
                if (mControllerObjs[cID]->getCtrlerType() == mVRDevicePairs[id].type)
                {
                    ctrlerRealID = cID;
                    break;
                }
            }
        }
    }
}

void MainApplication::setupCameras()
{
    mProjectionLeft = wvrmatrixConverter(
        WVR_GetProjection(WVR_Eye_Left, mNearClip, mFarClip));
    mProjectionRight = wvrmatrixConverter(
        WVR_GetProjection(WVR_Eye_Right, mNearClip, mFarClip));

    mEyePosLeft = wvrmatrixConverter(
                      WVR_GetTransformFromEyeToHead(WVR_Eye_Left))
                      .invert();
    mEyePosRight = wvrmatrixConverter(
                       WVR_GetTransformFromEyeToHead(WVR_Eye_Right))
                       .invert();

    dumpMatrix("ProjectionLeft", mProjectionLeft);
    dumpMatrix("ProjectionRight", mProjectionRight);
    dumpMatrix("EyePosLeft", mEyePosLeft);
    dumpMatrix("EyePosRight", mEyePosRight);

    mWorldTranslation.identity();
    mWorldRotation = 0;
    gettimeofday(&mRtcTime, NULL);
}

void MainApplication::renderStereoTargets()
{
    LOGENTRY();
    glClearColor(0, 0, 0, 0);
    FrameBufferObject *fbo = NULL;

    fbo = gMsaa ? mLeftEyeFBOMSAA.at(mIndexLeft) : mLeftEyeFBO.at(mIndexLeft);
    fbo->bindFrameBuffer();

    WVR_TextureParams_t leftEyeTexture = WVR_GetTexture(mLeftEyeQ, mIndexLeft);
#if ENABLE_LOW_FOVEATED_RENDERING
    WVR_RenderFoveationParams_t foveated;
    foveated.focalX = foveated.focalY = 0.0f;
    foveated.fovealFov = 30.0f;
    foveated.periQuality = WVR_PeripheralQuality_Low;
    fbo->glViewportFull();
    WVR_PreRenderEye(WVR_Eye_Left, &leftEyeTexture, &foveated);
#else
    if (gScale < 1 && gScale > 0)
        fbo->glViewportScale(mLUV, mUUV);
    else
        fbo->glViewportFull();
    WVR_PreRenderEye(WVR_Eye_Left, &leftEyeTexture);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene(WVR_Eye_Left);
    fbo->unbindFrameBuffer();

    // Right Eye
    fbo = gMsaa ? mRightEyeFBOMSAA.at(mIndexRight) : mRightEyeFBO.at(mIndexRight);
    fbo->bindFrameBuffer();
    WVR_TextureParams_t rightEyeTexture = WVR_GetTexture(mRightEyeQ, mIndexRight);
#if ENABLE_LOW_FOVEATED_RENDERING
    foveated.focalX = foveated.focalY = 0.0f;
    foveated.fovealFov = 30.0f;
    foveated.periQuality = WVR_PeripheralQuality_Low;
    fbo->glViewportFull();
    WVR_PreRenderEye(WVR_Eye_Right, &rightEyeTexture, &foveated);
#else
    if (gScale < 1 && gScale > 0)
        fbo->glViewportScale(mLUV, mUUV);
    else
        fbo->glViewportFull();
    WVR_PreRenderEye(WVR_Eye_Right, &rightEyeTexture);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene(WVR_Eye_Right);
    fbo->unbindFrameBuffer();
    mText->clearTexts();
}

void MainApplication::renderScene(WVR_Eye nEye)
{
    WVR_RenderMask(nEye);
    bool isInputCapturedBySystem = WVR_IsInputFocusCapturedBySystem();

    if (isInputCapturedBySystem == false)
    {
        for (uint32_t cID = 0; cID < 2; ++cID)
        {
            if (mControllerObjs[cID] != nullptr && mInteractionMode != WVR_InteractionMode_Gaze)
            {
                uint32_t ctrlerRealID = 0;
                for (uint32_t devID = 0; devID < WVR_DEVICE_COUNT_LEVEL_1; ++devID)
                {
                    if (mControllerObjs[cID]->getCtrlerType() == mVRDevicePairs[devID].type)
                    {
                        ctrlerRealID = devID;
                        break;
                    }
                }
                if (mVRDevicePairs[ctrlerRealID].pose.isValidPose == true)
                {
                    Matrix4 ctrlerPose = mDevicePoseArray[ctrlerRealID];
                    Matrix4 projs[2], eyes[2];
                    Matrix4 view;
                    if (m3DOF)
                    {
                        view = mHMDPose;
                    }
                    if (nEye == WVR_Eye_Left)
                    {
                        projs[0] = mProjectionLeft;
                        eyes[0] = mEyePosLeft;
                        mControllerObjs[cID]->render(DrawMode_General, projs, eyes, view, ctrlerPose);
                    }
                    else if (nEye == WVR_Eye_Right)
                    {
                        projs[0] = mProjectionRight;
                        eyes[0] = mEyePosRight;
                        mControllerObjs[cID]->render(DrawMode_General, projs, eyes, view, ctrlerPose);
                    }
                }
                else
                {
                    mControllerObjs[cID]->resetButtonEffects();
                }
            }
        }
    }

    if (isInputCapturedBySystem == false && mInteractionMode == WVR_InteractionMode_Hand)
    {
        Matrix4 projs[2], eyes[2];
        projs[WVR_Eye_Left] = mProjectionLeft;
        eyes[WVR_Eye_Left] = mEyePosLeft;
        projs[WVR_Eye_Right] = mProjectionRight;
        eyes[WVR_Eye_Right] = mEyePosRight;
        mHandManager->updateAndRender(DrawMode_General, nEye, projs, eyes, mHMDPose);
    }

    if (nEye == WVR_Eye_Left)
    {
        if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
        {
            mPlanes->render(mProjectionLeft, mEyePosLeft, mHMDPose, mMarkerManager->detectingArucoData);
            mScanLine->render();
        }
        else
        {
            mCubes->renderCubes(mProjectionLeft, mEyePosLeft, mHMDPose, mMarkerManager->trackableMarkerData);
        }
        mText->drawSequence(mProjectionLeft, mEyePosLeft, mHMDPose, Vector4(0, 0, 1.0f, 0.4f));
    }
    if (nEye == WVR_Eye_Right)
    {
        if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
        {
            mPlanes->render(mProjectionRight, mEyePosRight, mHMDPose, mMarkerManager->detectingArucoData);
            mScanLine->render();
        }
        else
        {
            mCubes->renderCubes(mProjectionRight, mEyePosRight, mHMDPose, mMarkerManager->trackableMarkerData);
        }
        mText->drawSequence(mProjectionRight, mEyePosRight, mHMDPose, Vector4(0, 0, 1.0f, 0.4f));
    }

    glUseProgram(0);

    GLenum glerr = glGetError();
    if (glerr != GL_NO_ERROR)
    {
        LOGW("glGetError(): %d", glerr);
    }
}

void MainApplication::updateTime()
{
    // Process time variable.
    struct timeval now;
    gettimeofday(&now, NULL);

    mClockCount++;
    if (mRtcTime.tv_usec > now.tv_usec)
        mClockCount = 0;
    if (mClockCount >= VR_MAX_CLOCKS)
        mClockCount--;

    uint32_t timeDiff = timeval_subtract(now, mRtcTime);
    mTimeDiff = timeDiff / 1000000.0f;
    mTimeAccumulator2S += timeDiff;
    mRtcTime = now;
    mFrameCount++;
    if (mTimeAccumulator2S > 2000000)
    {
        mFPS = mFrameCount / (mTimeAccumulator2S / 1000000.0f);
        LOGI("NativeAnchor FPS %3.0f", mFPS);

        mFrameCount = 0;
        mTimeAccumulator2S = 0;
    }
}

void MainApplication::updateEyeToHeadMatrix(bool is6DoF)
{
    if (is6DoF != mIs6DoFPose)
    {
        if (is6DoF)
        {
            mEyePosLeft = wvrmatrixConverter(
                              WVR_GetTransformFromEyeToHead(WVR_Eye_Left, WVR_NumDoF_6DoF))
                              .invert();
            mEyePosRight = wvrmatrixConverter(
                               WVR_GetTransformFromEyeToHead(WVR_Eye_Right, WVR_NumDoF_6DoF))
                               .invert();
        }
        else
        {
            mEyePosLeft = wvrmatrixConverter(
                              WVR_GetTransformFromEyeToHead(WVR_Eye_Left, WVR_NumDoF_3DoF))
                              .invert();
            mEyePosRight = wvrmatrixConverter(
                               WVR_GetTransformFromEyeToHead(WVR_Eye_Right, WVR_NumDoF_3DoF))
                               .invert();
        }
    }
    mIs6DoFPose = is6DoF;
}

void MainApplication::updateHMDMatrixPose()
{
    LOGENTRY();
    WVR_GetSyncPose(WVR_PoseOriginModel_OriginOnHead, mVRDevicePairs, WVR_DEVICE_COUNT_LEVEL_1);
    mValidPoseCount = 0;
    mPoseClasses = "";
    for (int nDevice = 0; nDevice < WVR_DEVICE_COUNT_LEVEL_1; ++nDevice)
    {
        if (mVRDevicePairs[nDevice].pose.isValidPose)
        {
            mValidPoseCount++;
            mDevicePoseArray[nDevice] = wvrmatrixConverter(mVRDevicePairs[nDevice].pose.poseMatrix);
            switch (mVRDevicePairs[nDevice].type)
            {
            case WVR_DeviceType_HMD: // nDevice=0
                break;
            case WVR_DeviceType_Controller_Right: // nDevice=1
                mControllerObjs[0]->saveCursorPose(mDevicePoseArray[nDevice]);
                if (deviceSupport == true)
                {
                    if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
                    {
                        mText->addSequenceText(textObject{false,
                                                          std::vector<std::string>{"A: Start tracking", "Trigger: Select to add tracking marker"},
                                                          Vector3{0.0, 0.02, 0.0},
                                                          Vector3{0.0, 0.0, 0.0},
                                                          mControllerObjs[0]->mCursorPose});
                    }
                    else
                    {
                        mText->addSequenceText(textObject{false,
                                                          std::vector<std::string>{"A: Start detecting"},
                                                          Vector3{0.0, 0.02, 0.0},
                                                          Vector3{0.0, 0.0, 0.0},
                                                          mControllerObjs[0]->mCursorPose});
                    }
                }
                else{
                    mText->addSequenceText(textObject{false,
                                                      std::vector<std::string>{"Device not support marker feature"},
                                                      Vector3{0.0, 0.02, 0.0},
                                                      Vector3{0.0, 0.0, 0.0},
                                                      mControllerObjs[0]->mCursorPose});
                }
                break;
            case WVR_DeviceType_Controller_Left: // nDevice=2
                mControllerObjs[1]->saveCursorPose(mDevicePoseArray[nDevice]);
                if (deviceSupport == true){
                    if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
                    {
                        mText->addSequenceText(textObject{false,
                                                          std::vector<std::string>{"X: Delete all tracking markers", "Trigger: Select to delete tracking marker"},
                                                          Vector3{0.0, 0.02, 0.0},
                                                          Vector3{0.0, 0.0, 0.0},
                                                          mControllerObjs[1]->mCursorPose});
                    }
                    else
                    {
                        mText->addSequenceText(textObject{false,
                                                          std::vector<std::string>{"X: Stop/Start tracking all markers", "Trigger: Select to stop tracking marker"},
                                                          Vector3{0.0, 0.02, 0.0},
                                                          Vector3{0.0, 0.0, 0.0},
                                                          mControllerObjs[1]->mCursorPose});
                    }
                }
                else{
                    mText->addSequenceText(textObject{false,
                                                      std::vector<std::string>{"Device not support marker feature"},
                                                      Vector3{0.0, 0.02, 0.0},
                                                      Vector3{0.0, 0.0, 0.0},
                                                      mControllerObjs[1]->mCursorPose});
                }
                break;
            default:
                break;
            }
            if (mDevClassChar[nDevice] == 0)
            {
                switch (WVR_DeviceType_HMD + nDevice)
                {
                case WVR_DeviceType_HMD:
                    mDevClassChar[nDevice] = 'H';
                    break;
                case WVR_DeviceType_Controller_Right:
                    mDevClassChar[nDevice] = 'R';
                    break;
                case WVR_DeviceType_Controller_Left:
                    mDevClassChar[nDevice] = 'L';
                    break;
                default:
                    mDevClassChar[nDevice] = '?';
                    break;
                }
            }
            mPoseClasses += mDevClassChar[nDevice];
        }
    }

    if (mVRDevicePairs[WVR_DEVICE_HMD].pose.isValidPose)
    { // always in when device wake up
        updateEyeToHeadMatrix(mVRDevicePairs[WVR_DEVICE_HMD].pose.is6DoFPose);
        Matrix4 hmd = mDevicePoseArray[WVR_DEVICE_HMD];

        if (!mMove)
        {
            // The controller make the sample not simple.  If you don't have
            // a controller, we just need invert the hmd pose.

            // When the head turn left, acturally the object turn right.
            // When the head move left, acturally the object move right.
            // So we need invert the hmd matrix.
            mHMDPose = hmd.invert();
        }
        else
        {
            // In order to add translation and rotation to HMD. We need seperate
            // the translation and rotation into two matrix from HMD.
            Matrix4 hmdRotation = hmd; //[R 0]
            //[T 1]
            hmdRotation.setColumn(3, Vector4(0, 0, 0, 1));
            Matrix4 hmdTranslation;
            hmdTranslation.setColumn(3, Vector4(hmd[12], hmd[13], hmd[14], 1));

            // Update world rotation.
            mWorldRotation += -mDriveAngle * mTimeDiff; // mDriveAngle always zero
            Matrix4 mat4WorldRotation;
            mat4WorldRotation.rotate(mWorldRotation, 0, 1, 0);
            // Update WorldTranslation
            Vector4 direction = (mat4WorldRotation * hmdRotation) * Vector4(0, 0, 1, 0); // Not apply the tranlsation of hmdpose
            direction *= -mDriveSpeed * mTimeDiff;                                       // mDriveSpeed always zero
            direction.w = 1;                                                             // direction=(0, 0, 0, 1)
            // Move toward -z
            Matrix4 update;
            update.setColumn(3, direction);
            mWorldTranslation *= update;

            // Check world bound
            if (mWorldTranslation[12] >= mFarClip / 2)
                mWorldTranslation[12] = mFarClip / 2;
            if (mWorldTranslation[12] <= -mFarClip / 2)
                mWorldTranslation[12] = -mFarClip / 2;
            if (mWorldTranslation[13] >= mFarClip / 2)
                mWorldTranslation[13] = mFarClip / 2;
            if (mWorldTranslation[13] <= -mFarClip / 2)
                mWorldTranslation[13] = -mFarClip / 2;
            if (mWorldTranslation[14] >= mFarClip / 2)
                mWorldTranslation[14] = mFarClip / 2;
            if (mWorldTranslation[14] <= -mFarClip / 2)
                mWorldTranslation[14] = -mFarClip / 2;

            // DEFINE: The invert A^-1 is notated A'
            // The invert property: (AB)' = B'A'
            // "WT" means "world translation", "HR" means "hmd rotation", and so on.
            // We can get the model tranform matrix as (WT*HT*WR*HR)' = HR'*WR'*WT'*HT'
            // The tranlation matrix property: TA = AT , then: (TA)' = (AT)'
            // So we can put tranlsation matrix any where.
            // We apply WR' to vertex first, then do HR'.  If not, the world will be weired when look up or down.
            mHMDPose = (mWorldTranslation * hmdTranslation * mat4WorldRotation * hmdRotation).invert();
        }
    }
    if (gDebug)
        dumpMatrix("hmd", mHMDPose);
}

void MainApplication::updateMarker()
{
    if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
    {
        mMarkerManager->getDetectingData();
    }
    else
    {
        mMarkerManager->getTrackingData();
    }

    if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
    {
        mText->addMarkerText(mMarkerManager->detectingArucoData);
    }
    else
    {
        mText->addMarkerText(mMarkerManager->trackableMarkerData);
    }
}

void MainApplication::updateInteractStatus()
{
    if (mMarkerManager->getObserveState() == WVR_MarkerObserverState_Detecting)
    {
        mPlanes->checkEachInteract(mControllerObjs[0]->mCursorPose, mControllerObjs[1]->mCursorPose, mMarkerManager->detectingArucoData);
    }
    else
    {
        mCubes->checkDeleteInteract(mControllerObjs[1]->mCursorPose, mMarkerManager->trackableMarkerData);
    }
}

void MainApplication::setupControllers()
{
    for (uint32_t cID = 0; cID < 2; ++cID)
    {
        if (mControllerObjs[cID] != nullptr)
        {

            mControllerObjs[cID]->loadControllerModelAsync();
        }
        else
        {
            LOGW("[APCtrlerObj] CtrlerObj(%d) is nullptr", cID);
        }
    }
}
