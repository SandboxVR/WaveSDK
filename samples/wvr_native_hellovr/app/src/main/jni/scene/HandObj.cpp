#define LOG_TAG "APHandObj"

//#define DRAW_AXIS

#include <log.h>

#include "../Context.h"
#include "../shared/quat.h"

#include "HandManager.h"
#include "HandObj.h"

HandObj::HandObj(HandManager *iMgr, HandTypeEnum iHandType)
: mManager(iMgr)
, mHandType(iHandType)
, mThickness(0.001f)
, mHandScale(1.0f, 1.0f, 1.0f)
, mContouringOpacity(0.5f)
, mFillingOpacity(0.5f)
, mGraColorA{(255.0f/255.0f), (255.0f/255.0f), (255.0f/255.0f), 0.0f}
, mGraColorB{(255.0f/255.0f), (255.0f/255.0f), (255.0f/255.0f), 0.0f}
, mAxes(nullptr)
{
    mShift.translate(1,1.5,2);
    mAxes = new Axes();
}

HandObj::~HandObj()
{
    releaseGraphicsSystem();
    delete mAxes;
}

void HandObj::initializeGraphicsSystem()
{
    const char *shaderNames[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
        {
            "handDepth",
            "handContouring",
            "handFilling"
        },
        {
            "handDepthMV",
            "handContouringMV",
            "handFillingMV"
        }
    };
    const char *vpaths[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
        {
            "shader/vertex/hand_p0_vertex.glsl",
            "shader/vertex/hand_p1_vertex.glsl",
            "shader/vertex/hand_p2_vertex.glsl"
        },
        {
            "shader/vertex/hand_p0_vertex.glsl",
            "shader/vertex/hand_p1_vertex.glsl",
            "shader/vertex/hand_p2_vertex.glsl"
        }
    };
    const char *fpaths[DrawMode_MaxModeMumber][ShaderProgramID_MaxNumber] = {
        {
            "shader/fragment/hand_p0_fragment.glsl",
            "shader/fragment/hand_p1_fragment.glsl",
            "shader/fragment/hand_p2_fragment.glsl"
        },
        {
            "shader/fragment/hand_p0_fragment.glsl",
            "shader/fragment/hand_p1_fragment.glsl",
            "shader/fragment/hand_p2_fragment.glsl"
        }
    };
    for (uint32_t mode = 0; mode < DrawMode_MaxModeMumber; ++mode) {
        for (uint32_t pID = 0; pID < ShaderProgramID_MaxNumber; ++pID) {
            mShaders[mode][pID] = Shader::findShader(vpaths[mode][pID], fpaths[mode][pID]);
            if (mShaders[mode][pID] != nullptr) {
                LOGI("(%d): Shader find!!!", mHandType);
            } else {
                Context * context = Context::getInstance();
                EnvWrapper ew = context->getEnv();
                JNIEnv * env = ew.get();

                AssetFile vfile(context->getAssetManager(), vpaths[mode][pID]);
                AssetFile ffile(context->getAssetManager(), fpaths[mode][pID]);
                if (!vfile.open() || !ffile.open()) {
                    LOGE("(%d): Unable to read shader files!!!", mHandType);
                    return;
                }

                char *vstr = vfile.toString();
                char *fstr = ffile.toString();

                //LogD(mName, "%s\n%s\n", vpath, vstr);
                //LogD(mName, "%s\n%s\n", fpath, fstr);

                mShaders[mode][pID] = std::make_shared<Shader>(shaderNames[mode][pID], vpaths[mode][pID], vstr, fpaths[mode][pID], fstr);
                bool ret = mShaders[mode][pID]->compile();

                delete [] vstr;
                delete [] fstr;
                if (ret == false) {
                    LOGE("(%d): Compile shader error!!!", mHandType);
                } else {
                    Shader::putShader(mShaders[mode][pID]);
                }
            }

            mProjMatrixLocations[mode][pID]     = mShaders[mode][pID]->getUniformLocation("projMat");
            mViewMatrixLocations[mode][pID]     = mShaders[mode][pID]->getUniformLocation("viewMat");
            mWorldMatrixLocations[mode][pID]    = mShaders[mode][pID]->getUniformLocation("worldMat");
            mNormalMatrixLocations[mode][pID]   = mShaders[mode][pID]->getUniformLocation("normalMat");
            mSkeletonMatrixLocations[mode][pID] = mShaders[mode][pID]->getUniformLocation("boneMats");
            mColorLocations[mode][pID]          = mShaders[mode][pID]->getUniformLocation("color");
            mAlphaTexLocations[mode][pID]       = mShaders[mode][pID]->getUniformLocation("alphaTex");
            mThicknessLocations[mode][pID]      = mShaders[mode][pID]->getUniformLocation("thickness");
            mOpacityLocations[mode][pID]        = mShaders[mode][pID]->getUniformLocation("opacity");
            mGraColorALocations[mode][pID]      = mShaders[mode][pID]->getUniformLocation("graColorA");
            mGraColorBLocations[mode][pID]      = mShaders[mode][pID]->getUniformLocation("graColorB");

            LOGI("H(%d): Mode[%d][%d]: proj(%d) view(%d) world(%d) normal(%d) skeleton(%d) color(%d) alphaTex(%d), thinkness(%d) opac(%d) GCA(%d) GCB(%d)",
                mHandType, 
                mode, pID,
                mProjMatrixLocations[mode][pID], 
                mViewMatrixLocations[mode][pID],
                mWorldMatrixLocations[mode][pID],
                mNormalMatrixLocations[mode][pID],
                mSkeletonMatrixLocations[mode][pID],
                mColorLocations[mode][pID],
                mAlphaTexLocations[mode][pID],
                mThicknessLocations[mode][pID],
                mOpacityLocations[mode][pID],
                mGraColorALocations[mode][pID],
                mGraColorBLocations[mode][pID]);
            }
    }
    mAxes->initialize();
}

void HandObj::setTexture(Texture *iTexture)
{
    mHandAlpha = iTexture;
}

void HandObj::updateSkeleton(const Matrix4 iSkeletonPoses[sMaxSupportJointNumbers], const Vector3 &iHandScale)
{
    mHandScale = iHandScale;
    //
    mWristPose = iSkeletonPoses[HandBone_Wrist];
    Matrix4 wristPoseInv = mWristPose;
    wristPoseInv.invert();

    for (uint32_t jCount = 0; jCount < sMaxSupportJointNumbers; ++jCount) {
        mSkeletonPoses[jCount] = (wristPoseInv * iSkeletonPoses[jCount]); // convert to model space.
    }
}

Vector4 HandObj::calculateJointWorldPosition(uint32_t jID) const
{
    Vector4 wp;
    Vector3 lp = mHandModel.getModelJointLocalPosition(jID);

    if (mHandModel.mJointParentTable[jID] == sIdentityJoint) {
        wp = Vector4(lp.x, lp.y, lp.z, 1.0f);
    } else {
        uint32_t parentJointID = mHandModel.mJointParentTable[jID];
        Matrix4 parentJointTrans = mFinalSkeletonPoses[parentJointID];
        wp = parentJointTrans * Vector4(lp.x, lp.y, lp.z, 1.0f);
    }

    return wp;
}

void HandObj::loadModel(WVR_HandModel_t *iHandModel)
{
    mHandModel.loadModelToGraphicsResource(iHandModel);
}

void HandObj::render(
        const Matrix4 iProj,
        const Matrix4 iEye,
        const Matrix4 &iView,
        const Matrix4 &iHandPose)
{
    //1. cache depth and alpha setting.
    GLboolean oldDepth, oldAlpha;
    GLboolean oldCullingFace;
    GLint oldDepthFunc;
    GLboolean colorMaskes[4] = {GL_TRUE};
    GLboolean depthMask = GL_TRUE;
    GLboolean lastPolygonOffsetFill;
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

    Matrix4 proj = iProj * iEye; 

    //2. draw
    for (uint32_t jointID = 0; jointID < sMaxSupportJointNumbers; ++jointID) {
        if (mHandModel.mJointUsageTable[jointID] == 1) {
            Vector4 wp = calculateJointWorldPosition(jointID);
            mFinalSkeletonPoses[jointID] = mSkeletonPoses[jointID];
            mFinalSkeletonPoses[jointID][12] = wp.x;
            mFinalSkeletonPoses[jointID][13] = wp.y;
            mFinalSkeletonPoses[jointID][14] = wp.z;
            mModelSkeletonPoses[jointID] = mFinalSkeletonPoses[jointID] * mHandModel.mJointInvTransMats[jointID];
        }
        
        memcpy(mSkeletonMatrices + jointID * 16, mModelSkeletonPoses[jointID].get(), 16 * sizeof(GLfloat));
    }

    Matrix4 scaleFactor;
    LOGI("Hand(%d) : mHandScale(%lf, %lf, %lf)", mHandType, mHandScale.x, mHandScale.y, mHandScale.z);
    scaleFactor.scale(mHandScale.x, mHandScale.y, mHandScale.z);
    Matrix4 scaledPose = mShift * mWristPose * scaleFactor;
    //3. render
    //-------------------- step 1 --------------------
    uint32_t mode = DrawMode_General;
    if (mHandModel.mInitialized == false) {
        return;
    }

    Shader* targetShader = nullptr;
    uint32_t stepID = ShaderProgramID_FinalDepthWriting;
    targetShader = mShaders[mode][stepID].get();
    
    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    if (targetShader == nullptr) {
        LOGI("target shader nullptr"); return;
    }
    
    if (mHandAlpha == nullptr) {
        LOGI("mHandAlpha nullptr"); return;
    }
    targetShader->useProgram();

    glActiveTexture(GL_TEXTURE0);
    mHandAlpha->bindTexture();
    glUniform1i(mAlphaTexLocations[mode][stepID], 0);
    glUniform1f(mThicknessLocations[mode][stepID], mThickness);
    glUniform1f(mOpacityLocations[mode][stepID], mContouringOpacity);
    glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA);
    glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB);
    glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, proj.get());
    glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, iView.get());
    glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get());
    glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false, mSkeletonMatrices);

    mHandModel.mMesh.draw();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    //-------------------- step 2 --------------------
    stepID = ShaderProgramID_FinalContouring;
    targetShader = mShaders[mode][stepID].get();

    if (targetShader == nullptr) {
        LOGI("target shader nullptr"); return;
    }
    
    if (mHandAlpha == nullptr) {
        LOGI("mHandAlpha nullptr"); return;
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);

    glBlendFuncSeparate(
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE, GL_ONE);
    glFrontFace(GL_CCW);
    glCullFace(GL_FRONT);

    targetShader->useProgram();
    glActiveTexture(GL_TEXTURE0);
    mHandAlpha->bindTexture();

    glUniform1i(mAlphaTexLocations[mode][stepID], 0);
    glUniform1f(mThicknessLocations[mode][stepID], mThickness);
    glUniform1f(mOpacityLocations[mode][stepID], mContouringOpacity);
    glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA);
    glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB);
    glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, proj.get());
    glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, iView.get());
    glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get());
    glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false, mSkeletonMatrices);

    mHandModel.mMesh.draw();

    //-------------------- step 3 --------------------
    stepID = ShaderProgramID_FinalFilling;
    targetShader = mShaders[mode][stepID].get();

    glBlendFuncSeparate(
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
         GL_ONE, GL_ONE);
    glCullFace(GL_BACK);

    if (targetShader == nullptr) {
        LOGI("target shader nullptr"); return;
    }
    
    if (mHandAlpha == nullptr) {
        LOGI("mHandAlpha nullptr"); return;
    }

    targetShader->useProgram();

    glActiveTexture(GL_TEXTURE0);
    mHandAlpha->bindTexture();
    glUniform1i(mAlphaTexLocations[mode][stepID], 0);
    glUniform1f(mOpacityLocations[mode][stepID], mFillingOpacity);
    glUniform4fv(mGraColorALocations[mode][stepID], 1, mGraColorA);
    glUniform4fv(mGraColorBLocations[mode][stepID], 1, mGraColorB);
    glUniformMatrix4fv(mProjMatrixLocations[mode][stepID], 1, false, proj.get());
    glUniformMatrix4fv(mViewMatrixLocations[mode][stepID], 1, false, iView.get());
    glUniformMatrix4fv(mWorldMatrixLocations[mode][stepID], 1, false, scaledPose.get());
    glUniformMatrix4fv(mSkeletonMatrixLocations[mode][stepID], sMaxSupportJointNumbers, false, mSkeletonMatrices);

    mHandModel.mMesh.draw();

    targetShader->unuseProgram();
    mHandAlpha->unbindTexture();

    glBlendFuncSeparate(
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

#if defined(DRAW_AXIS)
    for (uint32_t jointID = 0; jointID < sMaxSupportJointNumbers; ++jointID) {
        if (mHandModel.mJointUsageTable[jointID] == 1) {
            //final solution
            Vector4 wp = calculateJointWorldPosition(jointID);
            Matrix4 modelSkeletonPose = mSkeletonPoses[jointID];
            modelSkeletonPose[12] = wp.x;
            modelSkeletonPose[13] = wp.y;
            modelSkeletonPose[14] = wp.z;
            Matrix4 worldJointMat = mWristPose * modelSkeletonPose;
            float color1[4] = {1.0, 0.0, 0.0, 1.0};
            mAxes->renderInner(iProj, iEye, iView, worldJointMat, true, color1);
            float color2[4] = {1.0, 1.0, 0.0, 1.0};
            Matrix4 realPoseWorldJointMat = mWristPose * mFinalSkeletonPoses[jointID];
            mAxes->renderInner(iProj, iEye, iView, realPoseWorldJointMat, true, color2);
        }
    }
#endif

    //4. status recovering.
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

    if (oldCullingFace == GL_TRUE) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }

    glColorMask(colorMaskes[0], colorMaskes[1], colorMaskes[2], colorMaskes[3]);
    glDepthMask(depthMask);
}

void HandObj::renderMultiView(
    const Matrix4 iProjs[DrawMode_MaxModeMumber],
    const Matrix4 iEyes[DrawMode_MaxModeMumber],
    const Matrix4 &iView,
    const Matrix4 &iHandPose)
{
}

void HandObj::releaseHandGraphicsResource()
{
    mHandModel.releaseGraphicsResource();
    mHandAlpha = nullptr;
    mAxes->release();
}

void HandObj::releaseGraphicsSystem()
{
    releaseHandGraphicsResource();
}