// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#include <jni.h>
#include <log.h>
#include <Context.h>
#include <hellovr.h>
#include <unistd.h>
#include <wvr/wvr.h>

// DEBUG can be disabled when the Activity ask.
extern bool gDebug;
extern bool gDebugOld;
extern bool gMsaa;
extern bool gScene;
extern bool gSceneOld;

MainApplication *app = new MainApplication();

int main(int argc, char *argv[])
{
    LOGENTRY();
    LOGI("NativeMarker main, new MainApplication ");
    LOGI("NativeMarker main, start call app->initVR()");
    if (!app)
        return 1;
    if (!app->initVR())
    {
        LOGW("NativeMarker main, initVR fail,start call app->shutdownVR()");
        app->shutdownVR();
        delete app;
        return 1;
    }

    if (!app->initGL())
    {
        LOGW("NativeMarker main, initGL failed, start call app->shutdownVR()");
        app->shutdownGL();
        app->shutdownVR();
        delete app;
        return 1;
    }

    app->supportCheck();

    while (1)
    {
        if (app->handleInput())
            break;
        if (app->renderFrame())
        {
            LOGE("Unknown render error. Quit.");
            break;
        }
        app->updateHMDMatrixPose();

        if (app->deviceSupport == true)
        {
            if (app->settingPassThrough() != WVR_Success)
            {
                LOGE("Unknown pass through error. Quit.");
                break;
            }
            app->updateMarker();
            app->updateInteractStatus();
        }
    }

    app->shutdownGL();
    app->shutdownVR();

    delete app;
    return 0;
}

extern "C"
{
    JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_init(JNIEnv *env, jobject act, jobject am);
    JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_setFlag(JNIEnv *env, jclass clazz, jint flag);
    JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_onResumeCall(JNIEnv *env, jclass clazz);
};

JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_init(JNIEnv *env, jobject activityInstance, jobject assetManagerInstance)
{
    LOGI("MainActivity_init: call  Context::getInstance()->init");
    Context::getInstance()->init(env, assetManagerInstance);
    LOGI("register WVR main when library loading");
    WVR_RegisterMain(main);
}

JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_setFlag(JNIEnv *env, jclass clazz, jint flag)
{
    // Flags are defined in java Activity.
    gDebug = (flag & 0x1) != 0;
    LOGD("gDebug = %d", gDebug ? 1 : 0);
    gMsaa = (flag & 0x2) != 0;
    LOGD("gMsaa = %d", gMsaa ? 1 : 0);
    gScene = (flag & 0x4) != 0;
    LOGD("gScene = %d", gScene ? 1 : 0);
}

JNIEXPORT void JNICALL Java_com_htc_vr_samples_wvr_1native_1marker_MainActivity_onResumeCall(JNIEnv *env, jclass clazz)
{
    app->onResume();
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    Context *ctx = new Context(vm);
    if (!ctx)
        return JNI_VERSION_1_6;

    return JNI_VERSION_1_6;
}

jint JNI_OnUnLoad(JavaVM *vm, void *reserved)
{
    delete Context::getInstance();
    return 0;
}
