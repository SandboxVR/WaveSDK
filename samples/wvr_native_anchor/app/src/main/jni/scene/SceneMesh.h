#ifndef WVR_NATIVE_HELLOVR_SCENEMESH_H
#define WVR_NATIVE_HELLOVR_SCENEMESH_H
#include <Matrices.h>
#include <Vectors.h>
#include <vector>
#include <log.h>
#include <Shader.h>
#include <Context.h>
#include <Object.h>
#include <wvr/wvr_types.h>
#include <wvr/wvr_scene.h>

class sceneMeshData
{
public:
    uint64_t mLastMeshID;
    bool shouldPush2GPU;
    std::vector<std::vector<uint32_t>> mMeshIndex;
    std::vector<std::vector<WVR_Vector3f>> mMeshVertex;
    std::vector<std::vector<float>> mMeshOrder;
    std::vector<int> mVertexSize;
    void clearAllData();
    void getOrderMesh();
    Vector3 selectColor(int num);
    sceneMeshData();
    virtual ~sceneMeshData();
};

class sceneMeshData3D : public sceneMeshData
{
public:
    WVR_SceneMeshType mMeshType;
    sceneMeshData3D(WVR_SceneMeshType meshType);
    ~sceneMeshData3D();
};

class sceneMeshData2D : public sceneMeshData
{
public:
    sceneMeshData2D();
    ~sceneMeshData2D();
};

class SceneMesh : public Object
{
    int mMatrixLocation;

public:
    static bool show;
    sceneMeshData3D *mVisualMeshData;
    sceneMeshData3D *mColliderMeshData;
    sceneMeshData2D *mPlaneMeshData;
    SceneMesh();
    ~SceneMesh();
    void render(sceneMeshData *meshData, Matrix4 projection, Matrix4 eye, Matrix4 view);
    void changeRenderTarget();
};

#endif // WVR_NATIVE_HELLOVR_SCENEMESH_H
