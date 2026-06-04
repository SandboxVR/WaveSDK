#define LOG_TAG "GenericInteraction"

#include "FocusRay.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <log.h>

#include "../Context.h"

const GLchar *gFocusRayVertexShader = R"FocusRayVS(
#version 310 es

layout(location = 0) in vec3 v3Position;
layout(location = 5) in ivec4 v4BoneID;

uniform mat4 mvpMat;
uniform vec4 boneStartColors[48]; //Os
uniform vec4 boneEndColors[48]; //Os
uniform float ratio;

out vec4 v4fColor;

void main() {
    v4fColor = boneStartColors[v4BoneID[0]] * (1.0 - ratio) + boneEndColors[v4BoneID[0]] * ratio;
    gl_Position = mvpMat * vec4(v3Position, 1.0);
}

)FocusRayVS";

const GLchar *gFocusRayFragmentShader = R"FocusRayFS(
#version 310 es

precision mediump float;

in vec4 v4fColor;
out vec4 oColor;

void main() {
    oColor = v4fColor;
}

)FocusRayFS";

FocusRayGradientAttribute::FocusRayGradientAttribute()
: mRadius(0.0f)
, mRatio(1.0f)
, mOriginColor{0.0f, 0.0f, 0.0f, 0.0f}
, mEndColor{0.0f, 0.0f, 0.0f, 0.0f}
{
}

FocusRayGradientAttribute::~FocusRayGradientAttribute()
{
}

//---------------------- FocusRayAttribute -------------------------
FocusRayAttribute::FocusRayAttribute()
: mLength(0.3f)
, mStride(15)
{
    mGradInfos.resize(4);
    mGradInfos[0].mRadius = 0.005f;
    mGradInfos[0].mRatio = 0.0f;
    mGradInfos[0].mOriginColor[0] = 1.0f;
    mGradInfos[0].mOriginColor[1] = 1.0f;
    mGradInfos[0].mOriginColor[2] = 1.0f;
    mGradInfos[0].mOriginColor[3] = 1.0f;
    mGradInfos[0].mEndColor[0] = 0.0196f;
    mGradInfos[0].mEndColor[1] = 0.537f;
    mGradInfos[0].mEndColor[2] = 0.788f;
    mGradInfos[0].mEndColor[3] = 1.0f;

    mGradInfos[1].mRadius = 0.00434f;
    mGradInfos[1].mRatio = 0.33f;
    mGradInfos[1].mOriginColor[0] = 1.0f;
    mGradInfos[1].mOriginColor[1] = 1.0f;
    mGradInfos[1].mOriginColor[2] = 1.0f;
    mGradInfos[1].mOriginColor[3] = 1.0f;
    mGradInfos[1].mEndColor[0] = 0.0f;
    mGradInfos[1].mEndColor[1] = 0.702f;
    mGradInfos[1].mEndColor[2] = 0.89f;
    mGradInfos[1].mEndColor[3] = 1.0f;

    mGradInfos[2].mRadius = 0.00367f;
    mGradInfos[2].mRatio = 0.67f;
    mGradInfos[2].mOriginColor[0] = 1.0f;
    mGradInfos[2].mOriginColor[1] = 1.0f;
    mGradInfos[2].mOriginColor[2] = 1.0f;
    mGradInfos[2].mOriginColor[3] = 1.0f;
    mGradInfos[2].mEndColor[0] = 0.8f;
    mGradInfos[2].mEndColor[1] = 0.941f;
    mGradInfos[2].mEndColor[2] = 0.976f;
    mGradInfos[2].mEndColor[3] = 0.75f;

    mGradInfos[3].mRadius = 0.003f;
    mGradInfos[3].mRatio = 1.0f;
    mGradInfos[3].mOriginColor[0] = 1.0f;
    mGradInfos[3].mOriginColor[1] = 1.0f;
    mGradInfos[3].mOriginColor[2] = 1.0f;
    mGradInfos[3].mOriginColor[3] = 1.0f;
    mGradInfos[3].mEndColor[0] = 0.8f;
    mGradInfos[3].mEndColor[1] = 0.941f;
    mGradInfos[3].mEndColor[2] = 0.976f;
    mGradInfos[3].mEndColor[3] = 0.0f;
}

FocusRayAttribute::~FocusRayAttribute() {

}

FocusRay::FocusRay()
: mMVPMatLocation(-1)
, mStartColorLocation(-1)
, mEndColorLocation(-1)
, mRatioLocation(-1)
{
    mShift.translate(1,1.5,2);
    mShader = std::make_shared<Shader>(
        "FocusRay", "FocusRay.vert", gFocusRayVertexShader, "FocusRay.frag", gFocusRayFragmentShader);
    mShader.get()->compile();

    mMVPMatLocation     = mShader.get()->getUniformLocation("mvpMat");
    mStartColorLocation = mShader.get()->getUniformLocation("boneStartColors");
    mEndColorLocation   = mShader.get()->getUniformLocation("boneEndColors");
    mRatioLocation      = mShader.get()->getUniformLocation("ratio");

    LOGI("P(%d). Mmvp(%d) SC(%d) EC(%d) ratio(%d)", mShader.get()->getProgramID(), 
        mMVPMatLocation, mStartColorLocation, mEndColorLocation, mRatioLocation);
}

FocusRay::~FocusRay() {
    mMesh.releaseGLComp();
    mShader.reset();
}

void FocusRay::refresh(const FocusRayAttribute &iAttribute)
{
    mMesh.releaseGLComp();
    //1. create mesh
    std::vector<float> vertices;
    std::vector<uint32_t> boneIDs;
    std::vector<uint32_t> indices;
    uint32_t verticesNum = (iAttribute.mGradInfos.size() * iAttribute.mStride) + 2;
    uint32_t lvNum = iAttribute.mGradInfos.size();
    //--- vertices
    //(side)
    vertices.resize(verticesNum * 3);
    boneIDs.resize(verticesNum * 4);
    uint32_t vID = 0;
    for (uint32_t lv = 0; lv < lvNum; ++lv) {
        for (uint32_t v = 0; v < iAttribute.mStride; ++v) {
            float radAngle = (float)(v) / (float)(iAttribute.mStride) * 2.0f * 3.14159f;
            Vector3 vertex;
            vertex.x = iAttribute.mGradInfos[lv].mRadius * std::cos(radAngle);
            vertex.y = iAttribute.mGradInfos[lv].mRadius * std::sin(radAngle);
            vertex.z = -iAttribute.mLength * iAttribute.mGradInfos[lv].mRatio;
            vertices[vID * 3 + 0] = vertex.x;
            vertices[vID * 3 + 1] = vertex.y;
            vertices[vID * 3 + 2] = vertex.z;
            boneIDs[vID * 4 + 0] = lv;
            boneIDs[vID * 4 + 1] = lv;
            boneIDs[vID * 4 + 2] = lv;
            boneIDs[vID * 4 + 3] = lv;
            vID++;
        }
    }
    //(top)
    {
        vertices[vID * 3 + 0] = 0.0f;
        vertices[vID * 3 + 1] = 0.0f;
        vertices[vID * 3 + 2] = 0.0f;
        boneIDs[vID * 4 + 0] = 0;
        boneIDs[vID * 4 + 1] = 0;
        boneIDs[vID * 4 + 2] = 0;
        boneIDs[vID * 4 + 3] = 0;
        vID++;
    }
    //(bottom)
    {
        uint32_t idx = iAttribute.mGradInfos.size() - 1;
        vertices[vID * 3 + 0] = 0.0f;
        vertices[vID * 3 + 1] = 0.0f;
        vertices[vID * 3 + 2] = -iAttribute.mLength;
        boneIDs[vID * 4 + 0] = idx;
        boneIDs[vID * 4 + 1] = idx;
        boneIDs[vID * 4 + 2] = idx;
        boneIDs[vID * 4 + 3] = idx;
        vID++;
    }
    //--- indices.
    //(side)
    for (uint32_t lv = 0; lv < lvNum - 1; ++lv) {
        for (uint32_t v = 0; v < iAttribute.mStride; ++v) {
            if (v == (iAttribute.mStride - 1)) {
                indices.push_back( lv      * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride +  0);

                indices.push_back( lv      * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride +  0);
                indices.push_back( lv      * iAttribute.mStride +  0);
            } else {
                indices.push_back( lv      * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride + (v + 1));

                indices.push_back( lv      * iAttribute.mStride +  v);
                indices.push_back((lv + 1) * iAttribute.mStride + (v + 1));
                indices.push_back( lv      * iAttribute.mStride + (v + 1));
            }
        }
    }
    //(top)
    {
        int idx = verticesNum - 2;
        for (uint32_t v = 0; v < iAttribute.mStride; ++v) {
            indices.push_back(idx);
            indices.push_back(v);
            indices.push_back((v + 1) % iAttribute.mStride);
        }
    }
    //(bottom)
    {
        int idx = verticesNum - 1;
        int finalLV = (lvNum - 1) * iAttribute.mStride;
        for (uint32_t v = 0; v < iAttribute.mStride; ++v) {
            indices.push_back(idx);
            indices.push_back(finalLV + v);
            indices.push_back(finalLV + ((v + 1) % iAttribute.mStride));
        }
    }

    mMesh.createVertexBufferData(VertexAttrib_Vertices, vertices.data(), vertices.size(), 3);
    mMesh.createVertexBoneIDData(boneIDs.data(), boneIDs.size(), 4);
    mMesh.createIndexBufferData(indices.data(), indices.size(), 3);

    mStartColors.resize(lvNum);
    mEndColors.resize(lvNum);
    for (uint32_t lv = 0; lv < lvNum; ++lv) {
        mStartColors[lv].x = iAttribute.mGradInfos[lv].mOriginColor[0];
        mStartColors[lv].y = iAttribute.mGradInfos[lv].mOriginColor[1];
        mStartColors[lv].z = iAttribute.mGradInfos[lv].mOriginColor[2];
        mStartColors[lv].w = iAttribute.mGradInfos[lv].mOriginColor[3];
        mEndColors[lv].x = iAttribute.mGradInfos[lv].mEndColor[0];
        mEndColors[lv].y = iAttribute.mGradInfos[lv].mEndColor[1];
        mEndColors[lv].z = iAttribute.mGradInfos[lv].mEndColor[2];
        mEndColors[lv].w = iAttribute.mGradInfos[lv].mEndColor[3];
    }
}

void FocusRay::render(
        const Matrix4 &iProj,
        const Matrix4 &iEye,
        const Matrix4 &iView,
        const Matrix4 &iRayPose,
        float iRatio)
{
    Matrix4 mvp = iProj * iEye * iView * mShift * iRayPose;
    mShader->useProgram();

    glUniformMatrix4fv(mMVPMatLocation, 1, false, mvp.get());
    glUniform4fv(mStartColorLocation, mStartColors.size(), (float*)mStartColors.data());
    glUniform4fv(mEndColorLocation, mEndColors.size(), (float*)mEndColors.data());
    glUniform1f(mRatioLocation, iRatio);
    mMesh.draw();

    mShader->unuseProgram();
}