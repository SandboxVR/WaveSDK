#include <Cube.h>

CubeManager::CubeManager() : Object(), actionStatue(noProcess)
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

        // LogD(mName, "%s\n%s\n", vpath, vstr);
        // LogD(mName, "%s\n%s\n", fpath, fstr);

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
    Vector3 A = Vector3(-0.5, 0, -0.5);
    Vector3 B = Vector3(0.5, 0, -0.5);
    Vector3 C = Vector3(0.5, 1, -0.5);
    Vector3 D = Vector3(-0.5, 1, -0.5);
    Vector3 E = Vector3(-0.5, 0, 0.5);
    Vector3 F = Vector3(0.5, 0, 0.5);
    Vector3 G = Vector3(0.5, 1, 0.5);
    Vector3 H = Vector3(-0.5, 1, 0.5);

    /*
         D-----------C
        /|          /|
       H-+---------G |
       | |         | |
       | A---------+-B
       |/     /-x  |/
       E-----z-----F
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

void CubeManager::addCubeFromAnchor(int anchorIndex, Matrix4 pose, Vector3 scale, bool isLost)
{
    Cube *cube = new Cube();
    cube->mPose = pose;
    cube->mScale = scale;
    cube->status = stand;
    cube->anchorIndex = anchorIndex;
    cube->anchorLost = isLost;
    cubeVector.push_back(cube);
}

void CubeManager::addUndefineCube()
{
    for (int a = 0; a < rightTouchVector.size(); a++)
    {
        undefineCubes.push_back(*cubeVector[rightTouchVector[a].vectorIndex]);
    }
}

bool CubeManager::cubesCanDelete()
{
    if (leftTouchVector.size() > 0)
        return true;
    else
        return false;
}

bool CubeManager::cubesCanChange()
{
    if (rightTouchVector.size() > 0)
        return true;
    else
        return false;
}

void CubeManager::updateUndefineCubePosition(Matrix4 rightHandPosition)
{
    for (int a = 0; a < undefineCubes.size(); a++)
    {
        undefineCubes[a].mPose[12] = rightHandPosition[12];
        undefineCubes[a].mPose[13] = rightHandPosition[13];
        undefineCubes[a].mPose[14] = rightHandPosition[14];
    }
}

void CubeManager::updateUndefineCubePose(Matrix4 rightHandPosition)
{
    for (int a = 0; a < undefineCubes.size(); a++)
    {
        undefineCubes[a].mPose = rightHandPosition;
    }
}

void CubeManager::updateUndefineCubeScale(Matrix4 rightCursorPose)
{
    Matrix4 inv;
    Matrix4 cursorPose;
    Vector3 temp;
    Vector3 out;

    for (int a = 0; a < undefineCubes.size(); a++)
    {
        inv = undefineCubes[a].mPose;
        cursorPose = inv.invert() * rightCursorPose;
        temp = Vector3(cursorPose[12], cursorPose[13], cursorPose[14]);

        if (abs(temp.x / (cubeMinWH / 2)) > 1)
            out.x = temp.x / 0.5;
        else
            out.x = cubeMinWH;
        if (abs(temp.y / cubeMinWH) > 1)
            out.y = temp.y;
        else
            out.y = cubeMinWH;
        if (abs(temp.z / (cubeMinWH / 2)) > 1)
            out.z = temp.z / 0.5;
        else
            out.z = cubeMinWH;

        undefineCubes[a].mScale = out;
    }
}

bool CubeManager::updateCubesStatus(Matrix4 rightHandPosition, Matrix4 leftHandPosition)
{
    rightTouchVector.clear();
    leftTouchVector.clear();

    if (actionStatue == noProcess)
    {
        checkInteract(rightHandPosition, leftHandPosition);
    }
    else if (actionStatue == moving)
    {
        //updateUndefineCubePosition(rightHandPosition);
        updateUndefineCubePose(rightHandPosition);
    }
    else if (actionStatue == scaling)
    {
        updateUndefineCubeScale(rightHandPosition);
    }

    if (rightTouchVector.size() != 0 || leftTouchVector.size() != 0)
        return true;
    else
        return false;
}

void CubeManager::clearUndefineCubes()
{
    undefineCubes.clear();
}

void CubeManager::clearCubes()
{
    for (int a = 0; a < cubeVector.size(); a++)
    {
        delete cubeVector[a];
    }
    cubeVector.clear();
}

bool CubeManager::checkInteract(Matrix4 rightCursorPose, Matrix4 leftCursorPose)
{
    Matrix4 inv;
    Matrix4 leftCursorPose_cube;
    Matrix4 rightCursorPose_cube;
    float x;
    float y;
    float z;

    for (int a = 0; a < cubeVector.size(); a++)
    {
        if(cubeVector[a]->anchorLost==false){
            inv = cubeVector[a]->mPose;
            inv = inv.invert();
            leftCursorPose_cube = inv * leftCursorPose;
            rightCursorPose_cube = inv * rightCursorPose;

            // check right hand
            x = rightCursorPose_cube[12];
            y = rightCursorPose_cube[13];
            z = rightCursorPose_cube[14];

            if (abs(x) <= abs(cubeVector[a]->mScale.x * 0.5) &&
                y * (cubeVector[a]->mScale.y) > 0 && abs(y) <= abs(cubeVector[a]->mScale.y) &&
                abs(z) <= abs(cubeVector[a]->mScale.z * 0.5))
            {
                rightTouchVector.push_back(MyIndex{a, cubeVector[a]->anchorIndex});
                cubeVector[a]->status = canChange;
            }
            else
            {
                cubeVector[a]->status = stand;
            }

            // check left hand
            x = leftCursorPose_cube[12];
            y = leftCursorPose_cube[13];
            z = leftCursorPose_cube[14];

            if (abs(x) <= abs(cubeVector[a]->mScale.x * 0.5) &&
                y * (cubeVector[a]->mScale.y) > 0 && abs(y) <= abs(cubeVector[a]->mScale.y) &&
                abs(z) <= abs(cubeVector[a]->mScale.z * 0.5))
            {
                leftTouchVector.push_back(MyIndex{a, cubeVector[a]->anchorIndex}); // delete candidate
                cubeVector[a]->status = canDelete;
            }
        }
    }
    return true;
}

void CubeManager::renderCubes(Matrix4 projection, Matrix4 eye, Matrix4 view)
{
    renderFinishCubes(projection, eye, view);
    renderUndefineCubes(projection, eye, view);
}

void CubeManager::renderFinishCubes(Matrix4 projection, Matrix4 eye, Matrix4 view)
{
    if (!mEnable || mHasError)
        return;
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    mShader->useProgram();
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 0.8f);
    for (int a = 0; a < cubeVector.size(); a++)
    {
        // skip lost tracking anchor
        if (cubeVector[a]->anchorLost == true)
            continue;

        Matrix4 matrix = projection * eye * view * cubeVector[a]->mPose;

        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        switch (cubeVector[a]->status)
        {
        case stand: // normal
            glUniform1i(mUseInput, 0);
            break;
        case canDelete: // delete candidate
            glUniform1i(mUseInput, 1);
            glUniform4f(mCubeColor, 1.0f, 0.0f, 0.0f, 1.0f);
            break;
        case canChange: // changing candidate
            glUniform1i(mUseInput, 1);
            glUniform4f(mCubeColor, 0.0f, 1.0f, 0.0f, 1.0f);
            break;
        default:
            break;
        }
        glUniform3f(mScale, cubeVector[a]->mScale.x, cubeVector[a]->mScale.y, cubeVector[a]->mScale.z);

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

void CubeManager::renderUndefineCubes(Matrix4 projection, Matrix4 eye, Matrix4 view)
{
    if (!mEnable || mHasError)
        return;
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    mShader->useProgram();
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 2.0f);
    for (int a = 0; a < undefineCubes.size(); a++)
    {
        Matrix4 matrix = projection * eye * view * undefineCubes[a].mPose;
        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        glUniform1i(mUseInput, 1);
        glUniform4f(mCubeColor, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform3f(mScale, undefineCubes[a].mScale.x, undefineCubes[a].mScale.y, undefineCubes[a].mScale.z);

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
