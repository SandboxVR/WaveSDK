#pragma once

#include <string>

#include "../object/Shader.h"
#include "../shared/Matrices.h"
#include "../shared/Vectors.h"

#include "Mesh.h"

class FocusPointAttribute
{
public:
    FocusPointAttribute();
    ~FocusPointAttribute();
public:
    float mOriginColor[4];
    float mEndColor[4];
    float mFarDistance;
    float mFocusRadius;
    float mIRRadRatio;
    float mORStartRadRatio;
    float mOREndRadRatio;
    float mBlurRatio;
    float mArrowWidth;
    float mArrowHeight;
    float mArrowColor[4];
};

class FocusPoint
{
public:
    explicit FocusPoint();
    virtual ~FocusPoint();
public:
    float getFarDistance() const {
        return mFarDistance;
    }

    void refresh(const FocusPointAttribute &iAttribute);

    void render(
        const Matrix4 &iMatProj,
        const Matrix4 &iMatEye,
        const Matrix4 &iMatView,
        const Matrix4 &iMatRayTrans,
        bool iIsHittingOverlay,
        const Matrix4 &iMatHittedOverlayRot,
        float iHittedDistance,
        float iRatio);
protected:
    Matrix4 mShift;
    Mesh mPointMesh; //Quad
    std::shared_ptr<Shader> mPointShader;
    GLint mMVPMatLocation; //common variable.
    GLint mColorLocation; //material
    GLint mInnerRadLocation;
    GLint mThinknessLocation;
    GLint mBlurRatioLocation;
    GLint mBlurRatioInvLocation;
    float mOriginColors[4];
    float mPressColors[4];
    float mFarDistance;
    float mFocusRadius;
    float mIRRadRatio;
    float mThickness;
    float mOutStartRadRatio;
    float mOutEndRadRatio;
    float mBlurRatio;
};