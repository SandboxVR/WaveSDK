#pragma once

#include <mutex>
#include <thread>
#include <list>

#include <wvr/wvr.h>
#include <wvr/wvr_ctrller_render_model.h>
#include <wvr/wvr_device.h>
#include <wvr/wvr_events.h>

#include "DrawEnum.h"

#include "../shared/Matrices.h"
#include "../object/Mesh.h"
#include "../object/Texture.h"
#include "../object/Shader.h"

enum CtrlerCompEnum
{
    CtrlerComp_Body              = 0,
    CtrlerComp_TouchPad          = 1,
    CtrlerComp_AppButton         = 2,
    CtrlerComp_HomeButton        = 3, //SystemKey
    CtrlerComp_DigitalTriggerKey = 4,
    CtrlerComp_TriggerKey        = 5,
    CtrlerComp_VolumeKey         = 6,
    CtrlerComp_VolumeUpKey       = 7,
    CtrlerComp_VolumeDownKey     = 8,
    CtrlerComp_Grip              = 9,
    CtrlerComp_DPad_Left         = 10,
    CtrlerComp_DPad_Right        = 11,
    CtrlerComp_DPad_Up           = 12,
    CtrlerComp_DPad_Down         = 13,
    CtrlerComp_TouchPad_Touch    = 14,
    CtrlerComp_BeamOrigin        = 15,
    CtrlerComp_Emitter           = 16,
    CtrlerComp_Battery           = 17,
    CtrlerComp_BumperKey         = 18,
    CtrlerComp_Thumbstick        = 19,
    CtrlerComp_ButtonA           = 20,
    CtrlerComp_ButtonB           = 21,
    CtrlerComp_ButtonX           = 22,
    CtrlerComp_ButtonY           = 23,
    CtrlerComp_MaxCompNumber,
};

enum AnimationPosesEnum
{
    AnimationPoses_Origin = 0,
    AnimationPoses_Pressed,
    AnimationPoses_MaxX,
    AnimationPoses_MinX,
    AnimationPoses_MaxY,
    AnimationPoses_MinY,
    AnimationPoses_MaxDefineValue,
};

enum CtrlerBtnStateEnum
{
    CtrlerBtnState_None = 0,
    CtrlerBtnState_Tapped,
    CtrlerBtnState_Pressed,
};


class AnimationPose {
public:
    AnimationPose(){}
    AnimationPose(const WVR_CtrlerModelAnimPoseData_t &iPose);
    ~AnimationPose(){}
public:
    AnimationPose &operator=(const WVR_CtrlerModelAnimPoseData_t &iPose);
    Matrix4 toMat4() const;
    AnimationPose lerp(const AnimationPose &iDst, float iRatio) const;
    std::string toString() const;
public:
    Vector3 mPosition;
    Vector3 mRotation;
    Vector3 mScale;
};

class AnimationNode {
public:
    AnimationNode() : mType(0), mUseBlueEffect(0) {}
    ~AnimationNode(){}
public:
    void setAnimation(const WVR_CtrlerModelAnimNodeData_t &iNodeData);
    Matrix4 getAnimationMatrix(float iAnalogX, float iAnalogY, bool iIsBtnPressed) const;
    bool isValid() const {return (mType > 0 && mType <= 3);}
protected:
    AnimationPose getMinMaxAnimationPose(float iAnalogX, float iAnalogY) const;
public:
    AnimationPose mPoses[AnimationPoses_MaxDefineValue];
    uint32_t mType; //Type 0 : no Effect. Type 1 : 1D analog trigger. Type 2 : 2D analog thumb trigger  
    uint32_t mUseBlueEffect;// 0 : no effect. 1 : blue effect.
};

class Controller
{
public:
    class ExtraMesh
    {
    public:
        ExtraMesh()
        : mDefaultDraw(false)
        , mTexID(-1)
        {
        }
        ~ExtraMesh()
        {
        }
    public:
        std::string mName;
        Mesh mMesh;
        bool mDefaultDraw;
        int32_t mTexID;
        Matrix4 mLocalMat;
    };
public:
    static const std::string sControllerCompNames[CtrlerComp_MaxCompNumber];
    static const WVR_InputId sControllerCompIDTable[CtrlerComp_MaxCompNumber];
public:
    explicit Controller(WVR_DeviceType iCtrlerType);
    ~Controller();
public:
    void loadControllerModelAsync();
    void render(DrawModeEnum iMode, const Matrix4 iProjs[DrawMode_MaxModeMumber], const Matrix4 iEyes[DrawMode_MaxModeMumber], const Matrix4 &iView, const Matrix4 &iCtrlerPose);
    bool isThisCtrlerType(WVR_DeviceType iCtrlerType) const;
    void switchCtrlerType();
    void refreshButtonStatus(const WVR_Event_t &iEvent);
    void handleDisconnected();
    WVR_DeviceType getCtrlerType() const;
public:
    void setButtonEffectColor(float r, float g, float b, float a);
    void resetButtonEffects();
protected:
    void initializeGLComp();
    void releaseGLComp();
protected:
    void initializeCtrlerModelGLComp();//protected by mCachedDataMutex!!!
    void releaseCtrlerModelGLComp();
    void initializeCtrlerModelAnimData(WVR_CtrlerModelAnimData_t *iAnimData);
    uint32_t getCompIdxByName(const std::string &iName) const;
protected:
    void drawCtrlerBody(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
    void drawCtrlerBattery(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
    void drawCtrlerButtonEffect(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
    void drawCtrlerTouchPad(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
    void drawCtrlerRay(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
    void drawCtrlerExtraMeshes(DrawModeEnum iMode, const Matrix4 iMVPs[DrawMode_MaxModeMumber]);
protected:
    void refreshBatteryStatus();
protected:
    float mBtnEffect[4];
    bool mIsShowBattery;
    bool mIsUseAnimations;
    int32_t mBatteryLevel;
    float mCalmDownTime;
protected:
    WVR_CtrlerModel_t *mCachedData;
    bool mIsDataReady;
    std::mutex mCachedDataMutex;
    std::mutex mLoadingThreadMutex; //**** IMPORTANT : only can used in lambda function in loadModelAsync
    bool mInitialized;
    WVR_DeviceType mCtrlerType;
    std::thread mLoadModelFuncThread;
protected: //component
    bool mCompExistFlags[CtrlerComp_MaxCompNumber];
    bool mCompDefaultDraw[CtrlerComp_MaxCompNumber];
    Mesh mCompMeshes[CtrlerComp_MaxCompNumber];
    int32_t mCompTexID[CtrlerComp_MaxCompNumber];
    Matrix4 mCompLocalMats[CtrlerComp_MaxCompNumber];
    AnimationNode mAnimationNodes[CtrlerComp_MaxCompNumber];
    CtrlerBtnStateEnum mCompStates[CtrlerComp_MaxCompNumber];
    std::list<ExtraMesh> mExtraMeshes;
protected: //battery
    std::vector<Texture*> mBatLvTex;
    std::vector<int32_t> mBatMinLevels;
    std::vector<int32_t> mBatMaxLevels;
    std::chrono::system_clock::time_point mLastUpdateTime;
protected: //volume key.
    bool mIsOneVolumeKey;
protected: //touchpad plane matrix
    Matrix4 mTouchPadPlaneMat;
    float mFloatingDistance;
    float mRadius;
    Vector4 mTouchPadDotOffset;
    float mTouchpadSacleFactor;
    bool mIsNeedRevertInputY;
protected:
    Matrix4 mEmitterPose;
    Mesh mRayMesh;
protected: //shader
    std::vector<Texture*> mTextureTable;
    Shader *mTargetShader;
    std::shared_ptr<Shader> mShaders[DrawMode_MaxModeMumber];
    int32_t mDiffTexLocations[DrawMode_MaxModeMumber];
    int32_t mMatrixLocations[DrawMode_MaxModeMumber];
    int32_t mUseEffectLocations[DrawMode_MaxModeMumber];
    int32_t mEffectColorLocations[DrawMode_MaxModeMumber];
    std::string mCurrentRenderModelName;
protected:
    Matrix4 mShift;
public:
    Matrix4 getEmitterPose();
};
