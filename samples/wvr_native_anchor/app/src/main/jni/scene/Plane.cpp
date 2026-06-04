// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#define LOG_TAG "Plane"
#include <Plane.h>
#include <log.h>
#include <Shader.h>
#include <Texture.h>
#include <Object.h>
#include <VertexArrayObject.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

PlaneManager::PlaneManager() : Object()
{

    const char *shaderNames = "Cubes";
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

void PlaneManager::addPlane(int index, Matrix4 pose, Vector3 scale)
{
    planesVector.push_back(Plane{pose, scale, noRay, index});
}

void PlaneManager::clearPlanes()
{
    planesVector.clear();
}

void PlaneManager::updatePlanesStatus(Matrix4 rightCursorPose)
{
    float shortestDistance = std::numeric_limits<float>::max();
    Vector3 p0(rightCursorPose[12], rightCursorPose[13], rightCursorPose[14]);
    Matrix4 shortestPose;
    Matrix4 interactPose;
    int shortestIndex;
    intersectionPose_world.clear();
    for (int a = 0; a < planesVector.size(); a++)
    {
        planesVector[a].status = noRay;
        if (checkEachInteract(rightCursorPose, planesVector[a], interactPose))
        {
            float dis = p0.distance(Vector3(interactPose[12], interactPose[13], interactPose[14]));
            if (shortestDistance > dis)
            {
                shortestPose = interactPose;
                shortestDistance = dis;
                shortestIndex = a;
            }
        }
    }
    if (shortestDistance != std::numeric_limits<float>::max())
    {
        planesVector[shortestIndex].status = rightHandSelect;
        intersectionPose_world.push_back(shortestPose);
    }
}

Matrix4 PlaneManager::mappingPoseOnPlane(Matrix4 cursorPose, Matrix4 planePose)
{
    Vector4 cursorPosition(cursorPose[12], cursorPose[13], cursorPose[14], 1);
    Vector4 cursorPosition_x = cursorPose * Vector4(1, 0, 0, 0);
    cursorPosition = expressViaPlaneCoordinate(cursorPosition, planePose);
    cursorPosition_x = expressViaPlaneCoordinate(cursorPosition_x, planePose);
    Vector3 c(cursorPosition_x.x - cursorPosition.x, cursorPosition_x.y - cursorPosition.y, 0);
    Vector3 p(1, 0, 0);
    float thita = acos(p.dot(c) / (c.length() * p.length()));
    if (c.y < 0)
        thita = thita * -1;
    Matrix4 out;

    out.rotateZ(thita * 180 / 3.1415926);
    float dd = thita * 180 / 3.1415926;
    return out;
}

bool PlaneManager::checkEachInteract(Matrix4 cursorPose, Plane plane, Matrix4 &interactPose)
{
    float t;
    Matrix4 temp_world;
    // intersection point (plane & ray)
    Vector4 pt;
    // plane normal vector
    Vector4 pn(0, 0, 1, 1);
    // cube normal vector
    Vector4 cn(0, 0, -1, 1);
    Vector4 c0(cursorPose[12], cursorPose[13], cursorPose[14], 1);
    Vector4 p0(plane.mPose[12], plane.mPose[13], plane.mPose[14], 1);

    cn = cursorPose * cn;
    cn.x = cn.x - cursorPose[12];
    cn.y = cn.y - cursorPose[13];
    cn.z = cn.z - cursorPose[14];

    pn = plane.mPose * pn;
    pn.x = pn.x - plane.mPose[12];
    pn.y = pn.y - plane.mPose[13];
    pn.z = pn.z - plane.mPose[14];

    temp_world = plane.mPose;

    // if controller ray & plane not parallel
    if ((cn.x * pn.x + cn.y * pn.y + cn.z * pn.z) != 0)
    {
        t = ((pn.x * p0.x + pn.y * p0.y + pn.z * p0.z) - (pn.x * c0.x + pn.y * c0.y + pn.z * c0.z)) / (pn.x * cn.x + pn.y * cn.y + pn.z * cn.z);
        if (t >= 0)
        {
            pt = Vector4(c0.x + cn.x * t, c0.y + cn.y * t, c0.z + cn.z * t, 1);
            temp_world[12] = pt.x;
            temp_world[13] = pt.y;
            temp_world[14] = pt.z;
            pt = expressViaPlaneCoordinate(pt, plane.mPose);
            // check if intersection in plane
            if (abs(pt.x) <= plane.mScale.x / 2 && abs(pt.y) <= plane.mScale.y / 2)
            {
                interactPose = temp_world;
                return true;
            }
        }
    }
    return false; // false:no interact
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

std::vector<Plane> PlaneManager::getPlanes()
{
    return planesVector;
}

void PlaneManager::render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view)
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

    mShader->useProgram();
    for (int a = 0; a < planesVector.size(); a++)
    {
        if (planesVector[a].status == rightHandSelect)
        {
            glUniform4f(mColor, 0.3f, 0.8f, 0.3f, 1.0f);
        }
        else if (planesVector[a].status == leftHandSelect)
        {
            glUniform4f(mColor, 0.9f, 0.3f, 0.3f, 1.0f);
        }
        else
        {
            glUniform4f(mColor, 0.8f, 0.8f, 0.8f, 1.0f);
        }

        matrix.push_back(projection * eye * view * planesVector[a].mPose);
        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix[a].get());
        glUniform3f(mScale, planesVector[a].mScale.x, planesVector[a].mScale.y, planesVector[a].mScale.z);
        mVAO->bindVAO();
        glDrawElements(GL_TRIANGLES, singleIndexArray.size(), GL_UNSIGNED_INT, 0);
        mVAO->unbindVAO();
    }
    mShader->unuseProgram();

    renderAxes(matrix);

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