#define LOG_TAG "APAxes"

#include <cmath>
#include <vector>

#include <log.h>

#include "../Context.h"
#include "../shared/quat.h"

#include "Axes.h"

#define WIDTH(x) x *std::tan(30.0f / 180.0f)

const float Axes::sDummyColor[4] = {0.0, 0.0, 0.0, 1.0};

Axes::Axes(float iLength)
{
    mLength = iLength;
    // 1. add program.
    const char *shaderNames[DrawMode_MaxModeMumber] = {
        "Axes",
        "AxesMV"};
    const char *vpaths[DrawMode_MaxModeMumber] = {
        "shader/vertex/axes_vertex.glsl",
        "shader/vertex/axes_vertex.glsl"};
    const char *fpaths[DrawMode_MaxModeMumber] = {
        "shader/fragment/axes_fragment.glsl",
        "shader/fragment/axes_fragment.glsl"};

    for (uint32_t mode = 0; mode < DrawMode_MaxModeMumber; ++mode)
    {
        mShaders[mode] = Shader::findShader(vpaths[mode], fpaths[mode]);
        if (mShaders[mode] != nullptr)
        {
            LOGI("Shader find!!!");
        }
        else
        {
            Context *context = Context::getInstance();
            EnvWrapper ew = context->getEnv();
            JNIEnv *env = ew.get();

            AssetFile vfile(context->getAssetManager(), vpaths[mode]);
            AssetFile ffile(context->getAssetManager(), fpaths[mode]);
            if (!vfile.open() || !ffile.open())
            {
                LOGE("Unable to read shader files!!!");
                return;
            }

            char *vstr = vfile.toString();
            char *fstr = ffile.toString();

            mShaders[mode] = std::make_shared<Shader>(shaderNames[mode], vpaths[mode], vstr, fpaths[mode], fstr);
            bool ret = mShaders[mode]->compile();

            delete[] vstr;
            delete[] fstr;
            if (ret == false)
            {
                LOGE("Compile shader error!!!");
            }
            else
            {
                Shader::putShader(mShaders[mode]);
            }
        }

        mMatrixLocations[mode] = mShaders[mode]->getUniformLocation("matrix");
        mColorLocations[mode] = mShaders[mode]->getUniformLocation("targetColor");
        mUseColorLocations[mode] = mShaders[mode]->getUniformLocation("useColor");
        LOGI("Mode[%d]: proj(%d) view(%d) world(%d)",
             mode,
             mMatrixLocations[mode],
             mColorLocations[mode],
             mUseColorLocations[mode]);
    }

    // 2. initialize mesh.
    std::vector<float> vertices;
    std::vector<float> colors;
    std::vector<uint32_t> indices;
    //+x axis.
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(0);

    vertices.push_back(mLength);
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(1);

    vertices.push_back(0.0);
    vertices.push_back(WIDTH(mLength));
    vertices.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(2);

    //+y axis.
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(3);

    vertices.push_back(0.0);
    vertices.push_back(mLength);
    vertices.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(4);

    vertices.push_back(0.0);
    vertices.push_back(0.0);
    vertices.push_back(WIDTH(mLength));
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    indices.push_back(5);

    //+z axis.
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(1.0);
    indices.push_back(6);

    vertices.push_back(WIDTH(mLength));
    vertices.push_back(0.0);
    vertices.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(1.0);
    indices.push_back(7);

    vertices.push_back(0.0);
    vertices.push_back(0.0);
    vertices.push_back(mLength);
    colors.push_back(0.0);
    colors.push_back(0.0);
    colors.push_back(1.0);
    colors.push_back(1.0);
    indices.push_back(8);

    mAxesMesh.createVertexBufferData(VertexAttrib_Vertices, vertices.data(), vertices.size(), 3);
    mAxesMesh.createVertexBufferData(VertexAttrib_Color, colors.data(), colors.size(), 4);
    mAxesMesh.createIndexBufferData(indices.data(), indices.size(), 3);
}

Axes::~Axes()
{
}

void Axes::renderInner(
    const Matrix4 iProj,
    const Matrix4 iEye,
    const Matrix4 &iView,
    const Matrix4 &iWorldPose,
    bool iUseColor,
    const float iColor[4])
{
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDept = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    Matrix4 mat = iProj * iEye * iView * iWorldPose;
    mShaders[DrawMode_General]->useProgram();

    glUniformMatrix4fv(mMatrixLocations[DrawMode_General], 1, false, mat.get());
    glUniform1i(mUseColorLocations[DrawMode_General], (iUseColor == true ? 1 : 0));
    glUniform4fv(mColorLocations[DrawMode_General], 1, iColor);
    mAxesMesh.draw();

    mShaders[DrawMode_General]->unuseProgram();

    if (oldCullingFace == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
    if (oldBlend == GL_TRUE)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (oldDept == GL_TRUE)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void Axes::renderInner(
    const Matrix4 pose,
    bool iUseColor,
    const float iColor[4])
{
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDept = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.3f);

    mShaders[DrawMode_General]->useProgram();

    glUniformMatrix4fv(mMatrixLocations[DrawMode_General], 1, false, pose.get());
    glUniform1i(mUseColorLocations[DrawMode_General], (iUseColor == true ? 1 : 0));
    glUniform4fv(mColorLocations[DrawMode_General], 1, iColor);
    mAxesMesh.draw();

    mShaders[DrawMode_General]->unuseProgram();
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (oldCullingFace == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
    if (oldBlend == GL_TRUE)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (oldDept == GL_TRUE)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void Axes::renderInner(
    std::vector<Matrix4> &pose,
    bool iUseColor,
    const float iColor[4])
{
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDept = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, 0.5f);

    mShaders[DrawMode_General]->useProgram();
    for (int a = 0; a < pose.size(); a++)
    {
        glUniformMatrix4fv(mMatrixLocations[DrawMode_General], 1, false, pose[a].get());
        glUniform1i(mUseColorLocations[DrawMode_General], (iUseColor == true ? 1 : 0));
        glUniform4fv(mColorLocations[DrawMode_General], 1, iColor);
        mAxesMesh.draw();
    }
    mShaders[DrawMode_General]->unuseProgram();

    glDisable(GL_POLYGON_OFFSET_FILL);
    if (oldCullingFace == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
    if (oldBlend == GL_TRUE)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (oldDept == GL_TRUE)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void Axes::release()
{
    mAxesMesh.releaseGLComp();

    for (uint32_t mode = 0; mode < DrawMode_MaxModeMumber; ++mode)
    {
        mShaders[mode].reset();
    }
}