// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#include <Plane.h>
#include <log.h>
#include <Shader.h>
#include <Texture.h>
#include <Object.h>
#include <VertexArrayObject.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

PlaneManager::PlaneManager() : Object(), nearestLeftSelect(-1), nearestRightSelect(-1)
{

    const char *shaderNames = "Planes";
    const char *vpaths = "shader/vertex/plane_vertex.glsl";
    const char *fpaths = "shader/fragment/plane_fragment.glsl";

    mShader = Shader::findShader(vpaths, fpaths);
    if (mShader != nullptr)
    {
        LOGI("Shader find!!!");
    }
    else
    {
        Context *context = Context::getInstance();
        EnvWrapper ew = context->getEnv();
        JNIEnv *env = ew.get();

        AssetFile vfile(context->getAssetManager(), vpaths);
        AssetFile ffile(context->getAssetManager(), fpaths);
        if (!vfile.open() || !ffile.open())
        {
            LOGE("Unable to read shader files!!!");
            return;
        }

        char *vstr = vfile.toString();
        char *fstr = ffile.toString();

        mShader = std::make_shared<Shader>(shaderNames, vpaths, vstr, fpaths, fstr);
        bool ret = mShader->compile();

        delete[] vstr;
        delete[] fstr;
        if (ret == false)
        {
            LOGE("Compile plane shader error!!!");
        }
        else
        {
            Shader::putShader(mShader);
        }
    }

    mMatrixLocation = mShader->getUniformLocation("matrix");
    mScale = mShader->getUniformLocation("scale");
    mColor = mShader->getUniformLocation("v2fColor");

    mVAO = new VertexArrayObject(true, true);

    setupShareVertex(singleVertexArray, singleIndexArray);
    mVAO->bindVAO();
    // populate the vertex buffer
    mVAO->bindArrayBuffer();
    GLuint offset = 0;
    int stride = (3) * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * singleVertexArray.size(), &singleVertexArray[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // transition
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void *)offset);

    // populate the index buffer
    mVAO->bindElementArrayBuffer();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * singleIndexArray.size(), &singleIndexArray[0], GL_STATIC_DRAW);

    mHasError = false;
    mVAO->unbindVAO();
    glDisableVertexAttribArray(0);

    mAxes = new Axes();

    if (hasGLError())
    {
        LOGI("OpenGL GL_ELEMENT_ARRAY_BUFFER error!!!");
        return;
    }
}
PlaneManager::~PlaneManager()
{
    delete mAxes;
}

void PlaneManager::setupShareVertex(std::vector<float> &vertdata, std::vector<u_int32_t> &indexdata)
{

    Vector3 A = Vector3(-0.5, 0.5, 0);
    Vector3 B = Vector3(0.5, 0.5, 0);
    Vector3 C = Vector3(-0.5, -0.5, 0);
    Vector3 D = Vector3(0.5, -0.5, 0);
    /*
       A(0)----B(1)
        /   y    /
       /   /-x  /
      /        /
    C(2)----D(3)
    */

    // Left
    addSinglePlaneVertex(A, vertdata); // 0
    addSinglePlaneVertex(B, vertdata); // 1
    addSinglePlaneVertex(C, vertdata); // 2
    addSinglePlaneVertex(D, vertdata); // 3

    int indexes[] = {0, 3, 2, 0, 1, 3};
    const size_t N = sizeof(indexes) / sizeof(int);
    for (int i = 0; i < N; i++)
        indexdata.push_back(indexes[i]);
}

void PlaneManager::addSinglePlaneVertex(const Vector3 &v, std::vector<float> &vertdata)
{
    vertdata.push_back(v.x);
    vertdata.push_back(v.y);
    vertdata.push_back(v.z);
}

void PlaneManager::checkEachInteract(Matrix4 rightPose, Matrix4 leftPose, std::vector<WVR_ArucoMarker> &markerData)
{
    float dis_R = std::numeric_limits<float>::max();
    float dis_L = std::numeric_limits<float>::max();
    leftSelectCandidate.clear();
    rightSelectCandidate.clear();
    nearestLeftSelect = -1;
    nearestRightSelect = -1;
    Vector4 c0_R(rightPose[12], rightPose[13], rightPose[14], 1);
    Vector4 c0_L(leftPose[12], leftPose[13], leftPose[14], 1);

    for (int a = 0; a < markerData.size(); a++)
    {
        float t;
        // intersection point with ray
        Vector4 pt_R;
        Vector4 pt_L;
        // plane normal vector
        Vector4 pn(0, 0, 1, 1);
        // controller ray normal vector
        Vector4 cn_R(0, 0, -1, 1);
        Vector4 cn_L(0, 0, -1, 1);

        Matrix4 mPose = convertToMatrix4(markerData[a].pose);
        Vector4 p0(mPose[12], mPose[13], mPose[14], 1);

        // right
        cn_R = rightPose * cn_R;
        cn_R.x = cn_R.x - rightPose[12];
        cn_R.y = cn_R.y - rightPose[13];
        cn_R.z = cn_R.z - rightPose[14];
        // left
        cn_L = leftPose * cn_L;
        cn_L.x = cn_L.x - leftPose[12];
        cn_L.y = cn_L.y - leftPose[13];
        cn_L.z = cn_L.z - leftPose[14];

        pn = mPose * pn;
        pn.x = pn.x - mPose[12];
        pn.y = pn.y - mPose[13];
        pn.z = pn.z - mPose[14];

        // only choose Detected marker to add as trackable marker
        if (markerData[a].state == WVR_MarkerTrackingState_Detected)
        {
            // if right controller ray & plane not parallel
            if ((cn_R.x * pn.x + cn_R.y * pn.y + cn_R.z * pn.z) != 0)
            {
                t = ((pn.x * p0.x + pn.y * p0.y + pn.z * p0.z) - (pn.x * c0_R.x + pn.y * c0_R.y + pn.z * c0_R.z)) / (pn.x * cn_R.x + pn.y * cn_R.y + pn.z * cn_R.z);
                if (t >= 0)
                {
                    Vector4 worldCoord = Vector4(c0_R.x + cn_R.x * t, c0_R.y + cn_R.y * t, c0_R.z + cn_R.z * t, 1);
                    pt_R = expressViaPlaneCoordinate(worldCoord, mPose);
                    // check if intersection in plane
                    if (abs(pt_R.x) <= markerData[a].size / 2 && abs(pt_R.y) <= markerData[a].size / 2)
                    {
                        Vector3 cp(rightPose[12], rightPose[13], rightPose[14]);
                        float tempDis = cp.distance(Vector3(worldCoord.x, worldCoord.y, worldCoord.z));
                        if (tempDis < dis_R)
                        {
                            dis_R = tempDis;
                            nearestRightSelect = a;
                        }
                        rightSelectCandidate.push_back(a);
                    }
                }
            }
        }
        else if (markerData[a].state == WVR_MarkerTrackingState_Stopped) // only choose stopped marker to destroy
        {
            // if left controller ray & plane not parallel
            if ((cn_L.x * pn.x + cn_L.y * pn.y + cn_L.z * pn.z) != 0)
            {
                t = ((pn.x * p0.x + pn.y * p0.y + pn.z * p0.z) - (pn.x * c0_L.x + pn.y * c0_L.y + pn.z * c0_L.z)) / (pn.x * cn_L.x + pn.y * cn_L.y + pn.z * cn_L.z);
                if (t >= 0)
                {
                    Vector4 worldCoord = Vector4(c0_L.x + cn_L.x * t, c0_L.y + cn_L.y * t, c0_L.z + cn_L.z * t, 1);
                    pt_L = expressViaPlaneCoordinate(worldCoord, mPose);
                    // check if intersection in plane
                    if (abs(pt_L.x) <= markerData[a].size / 2 && abs(pt_L.y) <= markerData[a].size / 2)
                    {
                        Vector3 cp(leftPose[12], leftPose[13], leftPose[14]);
                        float tempDis = cp.distance(Vector3(worldCoord.x, worldCoord.y, worldCoord.z));
                        if (tempDis < dis_L)
                        {
                            dis_L = tempDis;
                            nearestLeftSelect = a;
                        }
                        leftSelectCandidate.push_back(a);
                    }
                }
            }
        }
    }
}

Vector4 PlaneManager::expressViaPlaneCoordinate(Vector4 worldCoord, Matrix4 planePose)
{
    Matrix4 inv = planePose;
    inv = inv.invert();
    return inv * worldCoord;
}

void PlaneManager::renderAxes(std::vector<Matrix4> &pose)
{
    mAxes->renderInner(pose);
}

void PlaneManager::render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view, std::vector<WVR_ArucoMarker> &markerData)
{
    if (!mEnable || mHasError)
        return;
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDept = glIsEnabled(GL_DEPTH_TEST);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 0.8f);

    std::vector<Matrix4> matrix;
    matrix.clear();

    mShader->useProgram();
    for (int a = 0; a < markerData.size(); a++)
    {
        if (markerData[a].state == WVR_MarkerTrackingState_Detected)
        {
            if (a == nearestRightSelect)
            {
                // green (may be created as trackable marker)
                glUniform4f(mColor, 0.3f, 0.9f, 0.3f, 0.7f);
            }
            else
            {
                // white (detected)
                glUniform4f(mColor, 0.9f, 0.9f, 0.9f, 0.7f);
            }
        }
        else if (markerData[a].state == WVR_MarkerTrackingState_Stopped)
        {
            LOGI("markerData [a]=WVR_MarkerTrackingState_Stopped", a);
            if (a == nearestLeftSelect)
            {
                // red (may be removed from trackable marker)
                glUniform4f(mColor, 0.9f, 0.3f, 0.3f, 0.7f);
            }
            else
            {
                // green (already created as trackable marker)
                glUniform4f(mColor, 0.3f, 0.9f, 0.3f, 0.7f);
            }
        }
        else
        {
            // unexpected status
            continue;
        }
        matrix.push_back(projection * eye * view * convertToMatrix4(markerData[a].pose));
        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix[a].get());
        glUniform3f(mScale, markerData[a].size, markerData[a].size, markerData[a].size);
        Matrix4 mmt = convertToMatrix4(markerData[a].pose);

        mVAO->bindVAO();
        glDrawElements(GL_TRIANGLES, singleIndexArray.size(), GL_UNSIGNED_INT, 0);
        mVAO->unbindVAO();
    }
    mShader->unuseProgram();

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

    renderAxes(matrix);
}