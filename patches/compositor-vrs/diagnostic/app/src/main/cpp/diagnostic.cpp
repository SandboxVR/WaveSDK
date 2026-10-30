#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <wvr/wvr.h>
#include <wvr/wvr_device.h>
#include <wvr/wvr_events.h>
#include <wvr/wvr_render.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

namespace {
constexpr const char* kTag = "VrsDiagnosticNative";
constexpr uint32_t kSize = 1024;
std::atomic<bool> paused{true};
std::atomic<bool> stop{false};
std::atomic<uint64_t> frames{0};
std::atomic<uint64_t> submitErrors{0};
std::atomic<int> lastSubmit{0};
std::atomic<const char*> state{"registered"};

void fill(int x, int y, int width, int height, float r, float g, float b) {
    glScissor(x, y, width, height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// Procedural, head-locked color bars and checkerboard; no streaming or assets.
void pattern(unsigned eye) {
    static constexpr float colors[8][3] = {
        {1, 1, 1}, {1, 1, 0}, {0, 1, 1}, {0, 1, 0},
        {1, 0, 1}, {1, 0, 0}, {0, 0, 1}, {0.05f, 0.05f, 0.05f}};
    glEnable(GL_SCISSOR_TEST);
    fill(0, 0, kSize, kSize, 0.03f, 0.03f, 0.03f);
    for (int i = 0; i < 8; ++i) {
        fill(i * 128, 768, 128, 256, colors[i][0], colors[i][1], colors[i][2]);
    }
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            const float value = ((x + y) % 2 == 0) ? 0.7f : 0.1f;
            fill(x * 64, y * 64, 64, 64, value, value, value);
        }
    }
    fill(508, 0, 8, 768, 0, 1, 0);
    fill(0, 380, kSize, 8, 0, 1, 0);
    fill(32, 32, 96, 96, eye == 0 ? 1.0f : 0.0f, 0, eye == 1 ? 1.0f : 0.0f);
    glDisable(GL_SCISSOR_TEST);
}

int diagnosticMain(int, char**) {
    state.store("initializing-wave");
    const auto init = WVR_Init(WVR_AppType_VRContent);
    __android_log_print(ANDROID_LOG_INFO, kTag, "WVR_Init result=%d", init);
    if (init != WVR_InitError_None) {
        state.store("wave-init-failed");
        return 1;
    }
    WVR_RenderInitParams_t params{};
    params.graphicsApi = WVR_GraphicsApiType_OpenGL;
    params.renderConfig = WVR_RenderConfig_Default;
    const auto renderInit = WVR_RenderInit(&params);
    __android_log_print(ANDROID_LOG_INFO, kTag,
                       "WVR_RenderInit result=%d GL=%s renderer=%s texture=%ux%u",
                       renderInit, reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                       reinterpret_cast<const char*>(glGetString(GL_RENDERER)), kSize, kSize);
    if (renderInit != WVR_RenderError_None) {
        state.store("render-init-failed");
        WVR_Quit();
        return 2;
    }

    WVR_TextureQueueHandle_t queues[2]{};
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    bool ready = framebuffer != 0;
    for (auto& queue : queues) {
        queue = WVR_ObtainTextureQueue(WVR_TextureTarget_2D, WVR_TextureFormat_RGBA,
                                     WVR_TextureType_UnsignedByte, kSize, kSize, 0);
        if (queue == nullptr || WVR_GetTextureQueueLength(queue) == 0) ready = false;
    }
    state.store(ready ? "running" : "texture-queue-failed");
    int previousSubmit = 0;
    while (ready && !stop.load()) {
        WVR_Event_t event{};
        while (WVR_PollEventQueue(&event)) {
            if (event.common.type == WVR_EventType_Quit) stop.store(true);
        }
        if (stop.load()) break;
        if (paused.load()) {
            state.store("paused");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        state.store("running");
        WVR_DevicePosePair_t poses[3]{};
        WVR_GetSyncPose(WVR_PoseOriginModel_OriginOnHead, poses, 3);
        const WVR_PoseState_t* pose = nullptr;
        for (const auto& pair : poses) {
            if (pair.type == WVR_DeviceType_HMD && pair.pose.isValidPose) pose = &pair.pose;
        }
        bool submitted = true;
        for (unsigned eye = 0; eye < 2; ++eye) {
            const int index = WVR_GetAvailableTextureIndex(queues[eye]);
            if (index < 0) {
                submitted = false;
                submitErrors.fetch_add(1);
                continue;
            }
            auto texture = WVR_GetTexture(queues[eye], index);
            texture.layout.leftLowUVs.v[0] = texture.layout.leftLowUVs.v[1] = 0;
            texture.layout.rightUpUVs.v[0] = texture.layout.rightUpUVs.v[1] = 1;
            const auto waveEye = eye == 0 ? WVR_Eye_Left : WVR_Eye_Right;
            WVR_PreRenderEye(waveEye, &texture);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture.id)), 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                __android_log_print(ANDROID_LOG_ERROR, kTag, "incomplete eye framebuffer");
                state.store("framebuffer-failed");
                ready = false;
                submitted = false;
                break;
            }
            glViewport(0, 0, kSize, kSize);
            pattern(eye);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glFlush();
            const auto result = WVR_SubmitFrame(waveEye, &texture, pose, WVR_SubmitExtend_Default);
            lastSubmit.store(static_cast<int>(result));
            if (result != WVR_SubmitError_None) {
                submitErrors.fetch_add(1);
                submitted = false;
                if (previousSubmit != static_cast<int>(result)) {
                    __android_log_print(ANDROID_LOG_WARN, kTag, "WVR_SubmitFrame eye=%u result=%d", eye, result);
                }
            }
            previousSubmit = static_cast<int>(result);
        }
        if (submitted) frames.fetch_add(1);
        else std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (framebuffer != 0) glDeleteFramebuffers(1, &framebuffer);
    for (auto queue : queues) if (queue != nullptr) WVR_ReleaseTextureQueue(queue);
    WVR_Quit();
    if (ready) state.store("stopped");
    __android_log_print(ANDROID_LOG_INFO, kTag, "native main exit frames=%llu errors=%llu",
                       static_cast<unsigned long long>(frames.load()),
                       static_cast<unsigned long long>(submitErrors.load()));
    return ready ? 0 : 3;
}
}

extern "C" JNIEXPORT void JNICALL
Java_com_sandboxvr_wave_vrsdiagnostic_MainActivity_nativeRegister(JNIEnv*, jclass) {
    stop.store(false);
    paused.store(true);
    WVR_RegisterMain(diagnosticMain);
    __android_log_print(ANDROID_LOG_INFO, kTag, "WVR_RegisterMain complete");
}

extern "C" JNIEXPORT void JNICALL
Java_com_sandboxvr_wave_vrsdiagnostic_MainActivity_nativeSetPaused(JNIEnv*, jclass, jboolean value) {
    paused.store(value == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sandboxvr_wave_vrsdiagnostic_MainActivity_nativeStop(JNIEnv*, jclass) {
    stop.store(true);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_sandboxvr_wave_vrsdiagnostic_MainActivity_nativeStatus(JNIEnv* env, jclass) {
    char result[256];
    std::snprintf(result, sizeof(result), "state=%s frames=%llu submitErrors=%llu lastSubmit=%d paused=%d",
                  state.load(), static_cast<unsigned long long>(frames.load()),
                  static_cast<unsigned long long>(submitErrors.load()), lastSubmit.load(), paused.load());
    return env->NewStringUTF(result);
}
