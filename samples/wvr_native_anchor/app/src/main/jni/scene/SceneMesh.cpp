#include <scene/SceneMesh.h>
#include <VertexArrayObject.h>

bool SceneMesh::show = true;

sceneMeshData::sceneMeshData() : mLastMeshID(0) {}
sceneMeshData::~sceneMeshData()
{
    mMeshIndex.clear();
    mMeshVertex.clear();
    mMeshOrder.clear();
    mVertexPose.clear();
}

void sceneMeshData::clearAllData()
{
    mMeshIndex.clear();
    mMeshVertex.clear();
    mMeshOrder.clear();
    mVertexPose.clear();
    mLastMeshID = -1;
}

// Use this function to sort vertex. Can also neglect this function and use glDrawElement with vertex/index data
void sceneMeshData::getOrderMesh()
{
    int count;
    Vector3 color;
    mMeshOrder.clear();
    std::vector<float> buffer;
    for (int a = 0; a < mMeshIndex.size(); a++)
    {
        count = 0;
        buffer.clear();
        for (int b = 0; b < mMeshIndex[a].size(); b = b + 3)
        {
            count = count % 10 + 1;
            color = selectColor(count);

            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b]].v[0]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b]].v[1]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b]].v[2]);
            buffer.push_back(color.x);
            buffer.push_back(color.y);
            buffer.push_back(color.z);

            buffer.push_back(mMeshVertex[a][mMeshIndex[0][b + 1]].v[0]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[0][b + 1]].v[1]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[0][b + 1]].v[2]);
            buffer.push_back(color.x);
            buffer.push_back(color.y);
            buffer.push_back(color.z);

            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b + 2]].v[0]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b + 2]].v[1]);
            buffer.push_back(mMeshVertex[a][mMeshIndex[a][b + 2]].v[2]);
            buffer.push_back(color.x);
            buffer.push_back(color.y);
            buffer.push_back(color.z);
        }
        mMeshOrder.push_back(buffer);
    }
    mMeshVertex.clear();
    mMeshIndex.clear();
}

Vector3 sceneMeshData::selectColor(int num)
{
    Vector3 color;
    switch (num)
    {
    case 1:
        color.x = 238.0 / 255;
        color.y = 132.0 / 255;
        color.z = 117.0 / 255;
        break;
    case 2:
        color.x = 117.0 / 255;
        color.y = 238.0 / 255;
        color.z = 181.0 / 255;
        break;
    case 3:
        color.x = 238.0 / 255;
        color.y = 189.0 / 255;
        color.z = 117.0 / 255;
        break;
    case 4:
        color.x = 117.0 / 255;
        color.y = 228.0 / 255;
        color.z = 238.0 / 255;
        break;
    case 5:
        color.x = 230.0 / 255;
        color.y = 239.0 / 255;
        color.z = 116.0 / 255;
        break;
    case 6:
        color.x = 117.0 / 255;
        color.y = 171.0 / 255;
        color.z = 238.0 / 255;
        break;
    case 7:
        color.x = 177.0 / 255;
        color.y = 238.0 / 255;
        color.z = 117.0 / 255;
        break;
    case 8:
        color.x = 120.0 / 255;
        color.y = 117.0 / 255;
        color.z = 238.0 / 255;
        break;
    case 9:
        color.x = 123.0 / 255;
        color.y = 238.0 / 255;
        color.z = 117.0 / 255;
        break;
    case 10:
        color.x = 157.0 / 255;
        color.y = 28.0 / 255;
        color.z = 200.0 / 255;
        break;
    }
    return color;
}

sceneMeshData2D::sceneMeshData2D()
{
}
sceneMeshData2D::~sceneMeshData2D()
{
}

sceneMeshData3D::sceneMeshData3D(WVR_SceneMeshType meshType)
{
    mMeshType = meshType;
}

sceneMeshData3D::sceneMeshData3D()
{
}

sceneMeshData3D::~sceneMeshData3D()
{
}

SceneMesh::SceneMesh()
{
    // Different type of mesh data
    mVisualMeshData = new sceneMeshData3D(WVR_SceneMeshType_VisualMesh);
    mColliderMeshData = new sceneMeshData3D(WVR_SceneMeshType_ColliderMesh);
    mPlaneMeshData = new sceneMeshData2D();
    mObjectMeshData = new sceneMeshData3D();

    // init opengl stuff
    const char *shaderNames = "Scene";
    const char *vpaths = "shader/vertex/scene_vertex.glsl";
    const char *fpaths = "shader/fragment/scene_fragment.glsl";

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
            LOGE("Compile sceneMesh shader error!!!");
        }
        else
        {
            Shader::putShader(mShader);
        }
    }
    mShader->useProgram();
    mMatrixLocation = mShader->getUniformLocation("matrix");
    mVAO = new VertexArrayObject(true, true);
    mShader->unuseProgram();
}

SceneMesh::~SceneMesh()
{
    delete mVisualMeshData;
    delete mColliderMeshData;
    delete mPlaneMeshData;
    delete mObjectMeshData;
}

void SceneMesh::changeRenderTarget()
{
    mVisualMeshData->shouldPush2GPU = true;
    mColliderMeshData->shouldPush2GPU = true;
    mPlaneMeshData->shouldPush2GPU = true;
    mObjectMeshData->shouldPush2GPU = true;
}

void SceneMesh::render(sceneMeshData *meshData, Matrix4 projection, Matrix4 eye, Matrix4 view)
{
    mShader->useProgram();

    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    // update GPU vertex when needed
    if (meshData->shouldPush2GPU == true || meshData->mMeshOrder.size() > 1)
    {
        meshData->mVertexSize.clear();
        for (int a = 0; a < meshData->mMeshOrder.size(); a++)
        {
            mVAO->bindVAO();
            // populate the vertex data to buffer
            mVAO->bindArrayBuffer();
            int stride = 6 * sizeof(float);
            GLuint offset = 0;
            float *ptr = (float *)meshData->mMeshOrder[a].data();
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * meshData->mMeshOrder[a].size(), ptr, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void *)offset);
            // populate the color data to buffer
            offset += sizeof(float) * 3;
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (const void *)offset);

            mVAO->unbindVAO();
            mVAO->unbindArrayBuffer();
            meshData->mVertexSize.push_back(meshData->mMeshOrder[a].size() / 6);
            LOGI("render %d vertex (upload vertex to gpu)", meshData->mVertexSize[a]);
            Matrix4 matrix = projection * eye * view * meshData->mVertexPose[a];
            glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
            mVAO->bindVAO();
            glDrawArrays(GL_TRIANGLES, 0, meshData->mVertexSize[a]);
            mVAO->unbindVAO();
        }
        meshData->shouldPush2GPU = false;
    }
    else if (meshData->mVertexSize.size() > 0)
    {
        LOGI("render %d vertex", meshData->mVertexSize[0]);
        Matrix4 matrix = projection * eye * view * meshData->mVertexPose[0];
        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        mVAO->bindVAO();
        glDrawArrays(GL_TRIANGLES, 0, meshData->mVertexSize[0]);
        mVAO->unbindVAO();
    }

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
    mShader->unuseProgram();
}