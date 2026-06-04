#pragma once

#include <mutex>
#include <thread>

#include "Controller.h"

class CustomController
{
public:
    explicit CustomController(WVR_DeviceType iCtrlerType);
    ~CustomController();
public:
    void loadControllerEmitterAsync();
    void render(DrawModeEnum iMode, const Matrix4 iProjs[DrawMode_MaxModeMumber], const Matrix4 iEyes[DrawMode_MaxModeMumber], const Matrix4 &iView, const Matrix4 &iCtrlerPose);
    bool isThisCtrlerType(WVR_DeviceType iCtrlerType) const;
    void switchCtrlerType();
    WVR_DeviceType getCtrlerType() const;
protected:
    void drawCtrler(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
protected:
    void initializeGLComp();
    void releaseGLComp();
protected:
    bool mInitialized;
    WVR_DeviceType mCtrlerType;
    std::thread mGetEmitterFuncThread;
    std::mutex mLoadingThreadMutex; //**** IMPORTANT : only can used in lambda function in loadControllerEmitterAsync
protected:
    Mesh mCustomMesh;
protected:
    Matrix4 mEmitterPose;
    Mesh mRayMesh;
    Shader *mTargetShader;
    std::shared_ptr<Shader> mShaders[DrawMode_MaxModeMumber];
    int32_t mMatrixLocations[DrawMode_MaxModeMumber];
    int32_t mColorLocations[DrawMode_MaxModeMumber];
};