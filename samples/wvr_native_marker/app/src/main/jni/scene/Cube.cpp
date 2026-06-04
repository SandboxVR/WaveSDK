#include <Cube.h>
#include <markerManager.h>

CubeManager::CubeManager() : Object(), nearestLeftSelect(-1)
{
    const char *shaderNames = "Cubes";
    const char *vpaths = "shader/vertex/cube_vertex.glsl";
    const char *fpaths = "shader/fragment/cube_fragment.glsl";

    mShader = Shader::findShader(vpaths, fpaths);
    if (mShader != nullptr)
    {
        LOGI("Shader find!!!");
    }
    else
    {
        Context *context = Context::getInstance();
        EnvWrapper ew = context->getEnv();

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
            LOGE("Compile cube shader error!!!");
        }
        else
        {
            Shader::putShader(mShader);
        }
    }

    mMatrixLocation = mShader->getUniformLocation("matrix");
    mScale = mShader->getUniformLocation("scale");
    mCubeColor = mShader->getUniformLocation("v2fColor");
    mUseInput = mShader->getUniformLocation("fUseInputColor");

    mVAO = new VertexArrayObject(true, true);

    initCubes();
    initTexture();
}

CubeManager::~CubeManager()
{
}

void CubeManager::initCubes()
{

    setupShareVertex(singleVertexArray, singleIndexArray);

    mVAO->bindVAO();

    // populate the vertex buffer
    mVAO->bindArrayBuffer();
    GLuint offset = 0;
    int stride = (3 + 2) * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * singleVertexArray.size(), &singleVertexArray[0], GL_STATIC_DRAW);
    if (hasGLError())
    {
        LOGE("glBufferData GL_ARRAY_BUFFER error!!!");
        return;
    }

    glEnableVertexAttribArray(0); // transition
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void *)offset);

    offset += 3 * sizeof(float);
    glEnableVertexAttribArray(1); // coordinate
    glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (const void *)offset);

    // populate the index buffer
    mVAO->bindElementArrayBuffer();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * singleIndexArray.size(), &singleIndexArray[0], GL_STATIC_DRAW);
    if (hasGLError())
    {
        LOGE("glBufferData GL_ELEMENT_ARRAY_BUFFER error!!!");
        return;
    }
    mHasError = false;
    mVAO->unbindVAO();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

void CubeManager::initTexture()
{
    mTexture = Texture::loadTexture("textures/cube_texture.jpg");
    if (mTexture == NULL)
    {
        LOGI("texture lost!!!");
        mHasError = true;
        return;
    }

    mTexture->bindTexture();
    mTexture->bindBitmap(GL_RGB5_A1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    mTexture->unbindTexture();
    mTexture->cleanBitmap();
}

void CubeManager::setupShareVertex(std::vector<float> &vertdata, std::vector<u_int32_t> &indexdata)
{

    Vector3 A = Vector3(-0.5, 0.5, 0.0);
    Vector3 B = Vector3(0.5, 0.5, 0.0);
    Vector3 C = Vector3(0.5, 0.5, 1.0);
    Vector3 D = Vector3(-0.5, 0.5, 1.0);
    Vector3 E = Vector3(-0.5, -0.5, 0.0);
    Vector3 F = Vector3(0.5, -0.5, 0.0);
    Vector3 G = Vector3(0.5, -0.5, 1.0);
    Vector3 H = Vector3(-0.5, -0.5, 1.0);

    /*
     D-----------C
    /|          /|
   H-+---------G |
   | |         | |
   | A-----y---+-B
   |/     /-x  |/
   E-----------F
    */

    // Left
    addSingleCubeVertex(A, 0, 2 / 3.0f, vertdata); // 0
    addSingleCubeVertex(D, 0, 1 / 3.0f, vertdata); // 1
    // Right
    addSingleCubeVertex(B, 3 / 4.0f, 2 / 3.0f, vertdata); // 2
    addSingleCubeVertex(C, 3 / 4.0f, 1 / 3.0f, vertdata); // 3
    // Front
    addSingleCubeVertex(E, 1 / 4.0f, 2 / 3.0f, vertdata); // 4
    addSingleCubeVertex(F, 2 / 4.0f, 2 / 3.0f, vertdata); // 5
    addSingleCubeVertex(G, 2 / 4.0f, 1 / 3.0f, vertdata); // 6
    addSingleCubeVertex(H, 1 / 4.0f, 1 / 3.0f, vertdata); // 7
    // Top
    addSingleCubeVertex(C, 2 / 4.0f, 0, vertdata); // 8
    addSingleCubeVertex(D, 1 / 4.0f, 0, vertdata); // 9
    // Bottom
    addSingleCubeVertex(A, 1 / 4.0f, 1, vertdata); // 10
    addSingleCubeVertex(B, 2 / 4.0f, 1, vertdata); // 11
    // Back
    addSingleCubeVertex(A, 1, 2 / 3.0f, vertdata); // 12
    addSingleCubeVertex(D, 1, 1 / 3.0f, vertdata); // 13

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

void CubeManager::addSingleCubeVertex(const Vector3 &v, float t0, float t1, std::vector<float> &vertdata)
{
    // v is model position; t is texture coordinate
    vertdata.push_back(v.x);
    vertdata.push_back(v.y);
    vertdata.push_back(v.z);
    vertdata.push_back(t0);
    vertdata.push_back(t1);
}

void CubeManager::checkDeleteInteract(Matrix4 cursorPose, std::vector<mTrackableArucoData> &markerData)
{
    float dis = std::numeric_limits<float>::max();
    leftSelectCandidate.clear();
    nearestLeftSelect = -1;
    Vector4 c0(cursorPose[12], cursorPose[13], cursorPose[14], 1);
    for (int a = 0; a < markerData.size(); a++)
    {
        if (markerData[a].markerState.state == WVR_MarkerTrackingState_Tracked)
        {
            float t;
            // intersection point with ray
            Vector4 pt;
            // cube normal vector
            Vector4 pn(0, 0, 1, 1);
            // controller normal vector
            Vector4 cn(0, 0, -1, 1);
            Matrix4 mPose = convertToMatrix4(markerData[a].markerState.pose);
            Vector4 p0(mPose[12], mPose[13], mPose[14], 1);

            cn = cursorPose * cn;
            cn.x = cn.x - cursorPose[12];
            cn.y = cn.y - cursorPose[13];
            cn.z = cn.z - cursorPose[14];

            pn = mPose * pn;
            pn.x = pn.x - mPose[12];
            pn.y = pn.y - mPose[13];
            pn.z = pn.z - mPose[14];

            // if controller ray & plane not parallel
            if ((cn.x * pn.x + cn.y * pn.y + cn.z * pn.z) != 0)
            {
                t = ((pn.x * p0.x + pn.y * p0.y + pn.z * p0.z) - (pn.x * c0.x + pn.y * c0.y + pn.z * c0.z)) / (pn.x * cn.x + pn.y * cn.y + pn.z * cn.z);
                if (t >= 0)
                {
                    Vector4 worldCoord = Vector4(c0.x + cn.x * t, c0.y + cn.y * t, c0.z + cn.z * t, 1);

                    // convert pt to cube coordinate
                    Matrix4 inv = mPose;
                    inv = inv.invert();
                    pt = inv * worldCoord;

                    // check if intersection in cube
                    if (abs(pt.x) <= markerData[a].arucoData.size / 2 && abs(pt.y) <= markerData[a].arucoData.size / 2)
                    {
                        Vector3 cp(cursorPose[12], cursorPose[13], cursorPose[14]);
                        float tempDis = cp.distance(Vector3(worldCoord.x, worldCoord.y, worldCoord.z));
                        if (tempDis < dis)
                        {
                            dis = tempDis;
                            nearestLeftSelect = a;
                        }
                        leftSelectCandidate.push_back(a);
                    }
                }
            }
        }
    }
}

void CubeManager::renderCubes(Matrix4 projection, Matrix4 eye, Matrix4 view, std::vector<mTrackableArucoData> &markerData)
{
    if (!mEnable || mHasError)
        return;
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    mShader->useProgram();
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 0.8f);
    for (int a = 0; a < markerData.size(); a++)
    {
        // skip lost tracking anchor
        if (markerData[a].markerState.state != WVR_MarkerTrackingState_Tracked)
        {
            LOGI("markerData %d is lost", a);
            continue;
        }

        Matrix4 matrix = projection * eye * view * convertToMatrix4(markerData[a].markerState.pose);

        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        if (leftSelectCandidate.size() > 0 && nearestLeftSelect == a)
        {
            glUniform1i(mUseInput, 1);
            glUniform4f(mCubeColor, 1.0f, 0.0f, 0.0f, 0.4f);
        }
        else
        {
            glUniform1i(mUseInput, 0);
        }
        glUniform3f(mScale, markerData[a].arucoData.size, markerData[a].arucoData.size, markerData[a].arucoData.size);

        mVAO->bindVAO();
        glActiveTexture(GL_TEXTURE0);
        mTexture->bindTexture();
        glDrawElements(GL_TRIANGLES, singleIndexArray.size(), GL_UNSIGNED_INT, 0);
        mTexture->unbindTexture();
        mVAO->unbindVAO();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
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
