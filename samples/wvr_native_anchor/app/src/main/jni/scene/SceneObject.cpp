#define LOG_TAG "SceneObj"
#include <SceneObject.h>
#include <log.h>
#include <Shader.h>
#include <Texture.h>
#include <Object.h>
#include <VertexArrayObject.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

SceneObjectManager::SceneObjectManager() : Object()
{

    const char *shaderNames = "SceneObjs";
    const char *vpaths = "shader/vertex/sceneObject_vertex.glsl";
    const char *fpaths = "shader/fragment/sceneObject_fragment.glsl";

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
            LOGE("Compile scene object shader error!!!");
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
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void *)offset);

    // populate the index buffer
    mVAO->bindElementArrayBuffer();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * singleIndexArray.size(), &singleIndexArray[0], GL_STATIC_DRAW);

    mHasError = false;
    mVAO->unbindVAO();
    glDisableVertexAttribArray(0);

    if (hasGLError())
    {
        LOGI("OpenGL GL_ELEMENT_ARRAY_BUFFER error!!!");
        return;
    }
}

SceneObjectManager::~SceneObjectManager()
{
}

void SceneObjectManager::clearSceneObjects()
{
    objectsVector.clear();
}

void SceneObjectManager::addSingleObjectVertex(const Vector3 &v, std::vector<float> &vertdata)
{
    // v is model position; t is texture coordinate
    vertdata.push_back(v.x);
    vertdata.push_back(v.y);
    vertdata.push_back(v.z);
}

void SceneObjectManager::setupShareVertex(std::vector<float> &vertdata, std::vector<u_int32_t> &indexdata)
{

    Vector3 A = Vector3(-0.5, 0.5, -0.5);
    Vector3 B = Vector3(0.5, 0.5, -0.5);
    Vector3 C = Vector3(0.5, 0.5, 0.5);
    Vector3 D = Vector3(-0.5, 0.5, 0.5);
    Vector3 E = Vector3(-0.5, -0.5, -0.5);
    Vector3 F = Vector3(0.5, -0.5, -0.5);
    Vector3 G = Vector3(0.5, -0.5, 0.5);
    Vector3 H = Vector3(-0.5, -0.5, 0.5);

    /*
     D-------- --C
    /|          /|
   H-+------ --G |
   | |    /y   | |
   | |   -x    | |
   | A---------+-B
   |/          |/
   E----- -----F
    */

    // Left
    addSingleObjectVertex(A, vertdata); // 0
    addSingleObjectVertex(D, vertdata); // 1
    // Right
    addSingleObjectVertex(B, vertdata); // 2
    addSingleObjectVertex(C, vertdata); // 3
    // Front
    addSingleObjectVertex(E, vertdata); // 4
    addSingleObjectVertex(F, vertdata); // 5
    addSingleObjectVertex(G, vertdata); // 6
    addSingleObjectVertex(H, vertdata); // 7
    // Top
    addSingleObjectVertex(C, vertdata); // 8
    addSingleObjectVertex(D, vertdata); // 9
    // Bottom
    addSingleObjectVertex(A, vertdata); // 10
    addSingleObjectVertex(B, vertdata); // 11
    // Back
    addSingleObjectVertex(A, vertdata); // 12
    addSingleObjectVertex(D, vertdata); // 13

    int indexes[] = {
        0, 7, 1, 0, 4, 7,    // left
        10, 5, 4, 10, 11, 5, // bottom
        12, 3, 2, 12, 13, 3, // back
        6, 9, 7, 6, 8, 9,    // top
        6, 4, 5, 6, 7, 4,    // front
        6, 2, 3, 6, 5, 2     // right
    };
    const size_t N = sizeof(indexes) / sizeof(int);
    for (int i = 0; i < N; i++)
        indexdata.push_back(indexes[i]);
}

void SceneObjectManager::addSceneObject(int index, Matrix4 pose, Vector3 scale)
{
    objectsVector.push_back(SceneObject{pose, scale, index});
}

std::vector<SceneObject> SceneObjectManager::getSceneObjects()
{
    return objectsVector;
}

void SceneObjectManager::render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view)
{

    if (!mEnable || mHasError)
        return;
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    mShader->useProgram();
    glDisable(GL_CULL_FACE);
    for (int a = 0; a < objectsVector.size(); a++)
    {
        Matrix4 matrix = projection * eye * view * objectsVector[a].mPose;
        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        glUniform4f(mColor, 0.3f, 0.3f, 0.6f, 0.3f);
        glUniform3f(mScale, objectsVector[a].mScale.x, objectsVector[a].mScale.y, objectsVector[a].mScale.z);

        mVAO->bindVAO();
        glDrawElements(GL_TRIANGLES, singleIndexArray.size(), GL_UNSIGNED_INT, 0);
        mVAO->unbindVAO();
    }
    mShader->unuseProgram();
    if (oldCullingFace == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
}