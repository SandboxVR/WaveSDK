
#define LOG_TAG "GenericInteraction"

#include "FocusPoint.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <log.h>

//--------------- focus ---------------------
const GLchar *gFocusVertexShader = R"FDVS(
    #version 310 es
    layout(location = 0) in vec3 position;
    layout(location = 2) in vec2 texCoord;
    uniform mat4      mvpMat;
    out     vec2      fragTexCoord;
    void main()
    {
        fragTexCoord = vec2((texCoord.x - 0.5) * 2.0, (texCoord.y - 0.5) * 2.0);
        gl_Position  = mvpMat * vec4(position.xyz, 1);
    }
)FDVS";


const GLchar *gFocusFragmentShader =  R"FDFS(
    #version 310 es
    #define ENHANCE_CIRCLE
    precision mediump float;
    uniform vec4 circleColor;
    uniform float innerRadius;
    uniform float thinkness;
    uniform float blurRatio;
    uniform float blurRatioInv;

    in vec2 fragTexCoord;
    out vec4 oColor;
    void main()
    {
        float radius = sqrt(fragTexCoord.x * fragTexCoord.x + fragTexCoord.y * fragTexCoord.y) - innerRadius;
        if (radius >= 0.0 && radius <= thinkness) {
            oColor = circleColor;
        } else {
#if defined(ENHANCE_CIRCLE)
            float blurFactor = min(
                clamp( blurRatio - (radius - thinkness), 0.0, blurRatio) * blurRatioInv,
                clamp( blurRatio + radius, 0.0, blurRatio) * blurRatioInv
                );
            oColor = circleColor * blurFactor;
#else
            oColor = vec4(0.0);
#endif
        }
    }
)FDFS";


//---------------------- FocusPointAttribute -------------------------
FocusPointAttribute::FocusPointAttribute()
: mOriginColor{1.0f, 1.0f, 1.0f, 1.0f}
, mEndColor{0.113f, 0.741f, 0.969f, 1.0f}
, mFarDistance(10.0f)
, mFocusRadius(0.16f)
, mIRRadRatio(0.5f)
, mORStartRadRatio(0.75f)
, mOREndRadRatio(0.375f)
, mBlurRatio(0.125f)
{
}

FocusPointAttribute::~FocusPointAttribute() {
}

FocusPoint::FocusPoint()
: mMVPMatLocation(-1)
, mColorLocation(-1)
, mInnerRadLocation(-1)
, mThinknessLocation(-1)
, mOriginColors{1.0f, 1.0f, 1.0f, 1.0f}
, mPressColors{0.113f, 0.741f, 0.969f, 1.0f}
, mFarDistance(10.0f)
, mFocusRadius(0.16f)
, mIRRadRatio(0.67f)
, mThickness(0.33f)
, mOutStartRadRatio(0.75f)
, mOutEndRadRatio(0.375f)
, mBlurRatio(0.125f)
{
    mShift.translate(1,1.5,2);
    mPointShader = std::make_shared<Shader>(
        "FocusPoint",
        "FocusPoint.vert", gFocusVertexShader,
        "FocusPoint.frag", gFocusFragmentShader);
    mPointShader.get()->compile();
 
    mMVPMatLocation       = mPointShader.get()->getUniformLocation("mvpMat");
    mColorLocation        = mPointShader.get()->getUniformLocation("circleColor");
    mInnerRadLocation     = mPointShader.get()->getUniformLocation("innerRadius");
    mThinknessLocation    = mPointShader.get()->getUniformLocation("thinkness");
    mBlurRatioLocation    = mPointShader.get()->getUniformLocation("blurRatio");
    mBlurRatioInvLocation = mPointShader.get()->getUniformLocation("blurRatioInv");

    LOGI("P(%d). Mmvp(%d) CC(%d) IR(%d) Tk(%d) BR(%d) BRInv(%d)", mPointShader.get()->getProgramID(), 
        mMVPMatLocation,
        mColorLocation,
        mInnerRadLocation,
        mThinknessLocation,
        mBlurRatioLocation,
        mBlurRatioInvLocation);
}

FocusPoint::~FocusPoint()
{
    mPointShader.reset();
    mPointMesh.releaseGLComp();
}

void FocusPoint::refresh(const FocusPointAttribute &iAttribute)
{
    mOriginColors[0] = iAttribute.mOriginColor[0];
    mOriginColors[1] = iAttribute.mOriginColor[1];
    mOriginColors[2] = iAttribute.mOriginColor[2];
    mOriginColors[3] = iAttribute.mOriginColor[3];

    mPressColors[0] = iAttribute.mEndColor[0];
    mPressColors[1] = iAttribute.mEndColor[1];
    mPressColors[2] = iAttribute.mEndColor[2];
    mPressColors[3] = iAttribute.mEndColor[3];

    mFarDistance      = iAttribute.mFarDistance;
    mFocusRadius      = iAttribute.mFocusRadius;
    mIRRadRatio       = iAttribute.mIRRadRatio;
    mOutStartRadRatio = iAttribute.mORStartRadRatio;
    mOutEndRadRatio   = iAttribute.mOREndRadRatio;
    mBlurRatio        = iAttribute.mBlurRatio;
 
    //Initialize mesh.
    float ctrlerModelVertices[12] = {
        -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };
    float ctrlerModelTexCoords[8] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    
    uint32_t ctrlerModelIndices[6] = {
        0, 1, 2,
        0, 2, 3
    };

    float arrowModelVertices[12] = {
        -1.0f,  1.0f, -0.0001f,
         1.0f,  1.0f, -0.0001f,
         1.0f, -1.0f, -0.0001f,
        -1.0f, -1.0f, -0.0001f
    };

    float iconModelTexCoords[8] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
    };

    mPointMesh.createVertexBufferData(VertexAttrib_Vertices, ctrlerModelVertices, 12, 3);
    mPointMesh.createVertexBufferData(VertexAttrib_TexCoords, ctrlerModelTexCoords, 8, 2);
    mPointMesh.createIndexBufferData(ctrlerModelIndices, 6, 3);
}

void FocusPoint::render(
    const Matrix4 &iMatProj,
    const Matrix4 &iMatEye,
    const Matrix4 &iMatView,
    const Matrix4 &iMatRayTrans, 
    bool iIsHittingOverlay,
    const Matrix4 &iMatHittedOverlayRot,
    float iHittedDistance,
    float iRatio) {
    Matrix4 focusPosMat;
    focusPosMat.setColumn(3, Vector4(0.0, 0.0, -(iHittedDistance - 0.0002f), 1.0));

    Matrix4 matControllerSpace = iMatRayTrans * focusPosMat;
    Vector4 focusWorldPos      = matControllerSpace * Vector4(0.0, 0.0, 0.0, 1.0);
    Vector4 focusViewSpacePos  = iMatView * focusWorldPos;
    Vector3 focusViewSpacePosVec3(focusViewSpacePos.x, focusViewSpacePos.y, focusViewSpacePos.z);
    float hitDistanceInEyeSpace = focusViewSpacePosVec3.length();
    float scaleFactor = 1.0f;

    if (hitDistanceInEyeSpace > mFarDistance) {
        scaleFactor = mFocusRadius;
    } else {
        scaleFactor = (hitDistanceInEyeSpace / mFarDistance) * mFocusRadius;
    }

    Matrix4 matFocusPos; matFocusPos.setColumn(3, focusWorldPos);
    Matrix4 matScale; matScale.scale(scaleFactor, scaleFactor, 1.0);

    Matrix4 matHittedOverlayRot = iMatHittedOverlayRot;
    matHittedOverlayRot.setColumn(3, Vector4(0.0, 0.0, 0.0, 1.0));

    Matrix4 focusMat;
    Matrix4 worldMat;
    Matrix4 finalMat;

    Matrix4 focusProjMat = iMatProj;
    focusProjMat[8] = focusProjMat[9] = 0.0f;

    if (iIsHittingOverlay == true) {
        worldMat = iMatProj * iMatEye * iMatView * mShift * matFocusPos * matHittedOverlayRot; //
        focusMat = focusProjMat * iMatView * mShift * matFocusPos * matHittedOverlayRot;
    } else {
        worldMat = iMatProj * iMatEye * iMatView * mShift * matControllerSpace; //follow ctrller rotation.
        focusMat = focusProjMat * iMatView * mShift * matControllerSpace;
    }

    finalMat = worldMat * matScale;

    float finalRatio = iRatio;
    if (fabs(iRatio) < 0.0001f) {
        iRatio = 0.0f;
    }
    float innerRDRatio = mIRRadRatio * (1.0 - iRatio);
    
    float finalBlurRatio = mBlurRatio;
    if (fabs(mBlurRatio) < 0.0001f) {
        finalBlurRatio = 0.0001f;
    }

    if (fabs(innerRDRatio) < 0.0001f) {
        finalBlurRatio = 0.0001f;
    }

    float pointColor[4];
    pointColor[0] = mOriginColors[0] * (1.0f - iRatio) + mPressColors[0] * iRatio;
    pointColor[1] = mOriginColors[1] * (1.0f - iRatio) + mPressColors[1] * iRatio;
    pointColor[2] = mOriginColors[2] * (1.0f - iRatio) + mPressColors[2] * iRatio;
    pointColor[3] = mOriginColors[3] * (1.0f - iRatio) + mPressColors[3] * iRatio;

    float thickness = 0.0f;
    thickness = (mOutStartRadRatio * (1.0f - iRatio) + mOutEndRadRatio * iRatio) - innerRDRatio;

    //1. cache depth and alpha setting.
    GLboolean oldDepth, oldAlpha;
    GLboolean oldCullingFace;
    GLint oldDepthFunc;
    GLboolean colorMaskes[4] = {GL_TRUE};
    GLboolean depthMask = GL_TRUE;
    GLboolean lastPolygonOffsetFill;
    GLint lastBlendDstColor, lastBlendSrcColor, lastBlendDstAlpha, lastBlendSrcAlpha;
    GLfloat lastFactor, lastUnits;
    oldDepth = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);
    oldAlpha = glIsEnabled(GL_BLEND);
    lastPolygonOffsetFill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    oldCullingFace = glIsEnabled(GL_CULL_FACE);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &lastFactor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &lastUnits);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMaskes);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &lastBlendSrcColor);
    glGetIntegerv(GL_BLEND_DST_RGB, &lastBlendDstColor);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &lastBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &lastBlendDstAlpha);
    //2. rendering.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(
        GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE, GL_ONE);

    mPointShader->useProgram();
    glUniformMatrix4fv(mMVPMatLocation, 1, false, finalMat.get()); //scale radius to [0,1].
    glUniform1f(mInnerRadLocation, innerRDRatio);
    glUniform1f(mThinknessLocation, thickness);
    glUniform1f(mBlurRatioLocation, finalBlurRatio);
    glUniform1f(mBlurRatioInvLocation, 1.0 / finalBlurRatio);
    glUniform4fv(mColorLocation, 1, pointColor);
    mPointMesh.draw();
    mPointShader->unuseProgram();
    //3. status recovering.
    if (lastPolygonOffsetFill == GL_TRUE) {
        glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glPolygonOffset(lastFactor, lastUnits);

    if (oldDepth == GL_TRUE) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthFunc(oldDepthFunc);

    if (oldAlpha == GL_TRUE) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glBlendFuncSeparate(
        lastBlendSrcColor, lastBlendDstColor,
        lastBlendSrcAlpha, lastBlendDstAlpha);

    if (oldCullingFace == GL_TRUE) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }

    glColorMask(colorMaskes[0], colorMaskes[1], colorMaskes[2], colorMaskes[3]);
    glDepthMask(depthMask);
}