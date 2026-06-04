#include <ScanLine.h>
#include <VertexArrayObject.h>
ScanLine::ScanLine() : scanLine_y(0.8)
{
    // init opengl stuff
    const char *shaderNames = "Scan";
    const char *vpaths = "shader/vertex/scanLine_vertex.glsl";
    const char *fpaths = "shader/fragment/scanLine_fragment.glsl";

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
            LOGE("Compile scanLine shader error!!!");
        }
        else
        {
            Shader::putShader(mShader);
        }
    }
    mShader->useProgram();
    mStartLocation = mShader->getUniformLocation("startYPosition");
    mScale = mShader->getUniformLocation("scale");

    mVAO = new VertexArrayObject(true, true);
    initVertex();
    mShader->unuseProgram();
}

ScanLine::~ScanLine()
{
}

void ScanLine::initVertex()
{
    setupShareVertex(singleVertexArray, singleIndexArray);

    // populate the vertex buffer
    mVAO->bindVAO();
    mVAO->bindArrayBuffer();
    int stride = 3 * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * singleVertexArray.size(), &singleVertexArray[0], GL_STATIC_DRAW);
    if (hasGLError())
    {
        LOGI(" scan glBufferData GL_ARRAY_BUFFER error!!!");
        return;
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, 0);

    // populate the index buffer
    mVAO->bindElementArrayBuffer();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * singleIndexArray.size(), &singleIndexArray[0], GL_STATIC_DRAW);

    mHasError = false;
    mVAO->unbindVAO();
    glDisableVertexAttribArray(0);
}

void ScanLine::addSingleVertex(const Vector3 &v, std::vector<float> &vertdata)
{
    vertdata.push_back(v.x);
    vertdata.push_back(v.y);
    vertdata.push_back(v.z);
}

void ScanLine::setupShareVertex(std::vector<float> &vertdata, std::vector<u_int32_t> &indexdata)
{
    Vector3 A = Vector3(-1, 1, 0);
    Vector3 B = Vector3(-1, 0, 0);
    Vector3 C = Vector3(1, 0, 0);
    Vector3 D = Vector3(1, 1, 0);
    /*
    A(0)-------------------D(3)
    |                      |
    |                      |
    B(1)-------------------C(2)
    */
    addSingleVertex(A, vertdata);
    addSingleVertex(B, vertdata);
    addSingleVertex(C, vertdata);
    addSingleVertex(D, vertdata);
    int indexes[] = {0, 1, 2, 0, 2, 3};
    const size_t N = sizeof(indexes) / sizeof(int);
    for (int i = 0; i < N; i++)
        indexdata.push_back(indexes[i]);
}

void ScanLine::render()
{
    scanLine_y = scanLine_y - 0.002;
    if (scanLine_y <= -0.8)
        scanLine_y = 0.8;

    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    mShader->useProgram();
    glDisable(GL_CULL_FACE);

    glUniform1f(mStartLocation, scanLine_y);
    glUniform1f(mScale, 0.005);

    mVAO->bindVAO();
    glDrawElements(GL_TRIANGLES, singleIndexArray.size(), GL_UNSIGNED_INT, 0);
    mVAO->unbindVAO();
    mShader->unuseProgram();

    if (oldCullingFace == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
    if (hasGLError())
    {
        LOGI("render error");
        return;
    }
}
