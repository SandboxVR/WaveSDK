#pragma once

#include <string>

#include "DrawEnum.h"
#include "../shared/Matrices.h"
#include "../object/Mesh.h"
#include "../object/Texture.h"
#include "../object/Shader.h"
#include "Mesh.h"

class Axes
{
public:
    static const float sDummyColor[4];
public:
    explicit Axes(); //3cm
    ~Axes();
public:
    void initialize(float iLength = 0.03f);
    void renderInner(
        const Matrix4 iProj,
        const Matrix4 iEye,
        const Matrix4 &iView,
        const Matrix4 &iWorldPose,
        bool iUseColor = false,
        const float iColor[4] = sDummyColor);
    void release();  
protected:
    std::shared_ptr<Shader> mShaders[DrawMode_MaxModeMumber];
    int32_t mMatrixLocations[DrawMode_MaxModeMumber];
    int32_t mColorLocations[DrawMode_MaxModeMumber];
    int32_t mUseColorLocations[DrawMode_MaxModeMumber];
protected:
    Mesh mAxesMesh;
    float mLength;
};