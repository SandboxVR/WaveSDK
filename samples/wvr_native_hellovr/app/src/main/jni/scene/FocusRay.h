#pragma once

#include <vector>

#include "../object/Shader.h"
#include "../shared/Matrices.h"
#include "../shared/Vectors.h"

#include "Mesh.h"

class FocusRayGradientAttribute {
public:
    FocusRayGradientAttribute();
    ~FocusRayGradientAttribute();
public:
    float mRadius;
    float mRatio;
    float mOriginColor[4];
    float mEndColor[4];
};

class FocusRayAttribute {
public:
    FocusRayAttribute();
    ~FocusRayAttribute();
public:
    float mLength;
    uint32_t mStride;
    std::vector<FocusRayGradientAttribute> mGradInfos;
};

class FocusRay {
public:
    explicit FocusRay();
    ~FocusRay();
public:
    void refresh(const FocusRayAttribute &iAttribute);
    void render(
        const Matrix4 &iProj,
        const Matrix4 &iEye,
        const Matrix4 &iView,
        const Matrix4 &iRayPose,
        float iRatio);
    
protected:
    std::vector<Vector4> mStartColors;
    std::vector<Vector4> mEndColors;
    std::shared_ptr<Shader> mShader;
    int32_t mMVPMatLocation;
    int32_t mStartColorLocation;
    int32_t mEndColorLocation;
    int32_t mRatioLocation;
    Matrix4 mShift;

    Mesh mMesh;
};