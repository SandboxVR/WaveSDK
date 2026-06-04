#ifndef WVR_NATIVE_ANCHOR_CUBE_H
#define WVR_NATIVE_ANCHOR_CUBE_H
#include <Matrices.h>
#include <Vectors.h>
#include <vector>
#include <log.h>
#include <Shader.h>
#include <Context.h>
#include <Object.h>
#include <VertexArrayObject.h>
#include <Texture.h>
static const float cubeMinWH = 0.01;
static const Vector3 cubeDefaultScale(0.05, 0.05, 0.05); // cube: 5cm^3

typedef enum
{
    spatial = 0,
    cached = 1,
    persisted = 2,
} anchorType;

typedef enum
{
    noProcess = 0,
    moving = 1,
    scaling = 2,
    deleted = 3,
} processingState;

typedef enum
{
    stand = 0,
    canDelete = 1,
    canChange = 2,
} cubeState;

struct Cube
{
    Matrix4 mPose;
    Vector3 mScale;
    int status;
    int anchorIndex;
    bool anchorLost;
};

struct MyIndex
{
    int vectorIndex;
    int anchorIndex;
};

class CubeManager : public Object
{
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;
    int mMatrixLocation;
    int mScale;
    int mCubeColor;
    int mUseInput;
    std::vector<Cube *> cubeVectorSpatial;   // spatial anchor
    std::vector<Cube *> cubeVectorCached;    // spatial anchor from cached anchor
    std::vector<Cube *> cubeVectorPersisted; // spatial anchor from persisted anchor

public:
    processingState actionStatue;

    std::vector<Cube *> *targetCubeVector;
    std::vector<Cube> undefineCubes;
    std::vector<MyIndex> rightTouchVector;
    std::vector<MyIndex> leftTouchVector;

    CubeManager();
    ~CubeManager();
    bool cubesCanDelete();
    bool cubesCanChange();
    void addUndefineCube();
    void initCubes();
    void initTexture();
    void setupShareVertex(std::vector<float> &vertdata, std::vector<uint32_t> &indexdata);
    void addSingleCubeVertex(const Vector3 &v, float t0, float t1, std::vector<float> &vertdata);
    void renderCubes(Matrix4 projection, Matrix4 eye, Matrix4 view);
    void renderFinishCubes(Matrix4 projection, Matrix4 eye, Matrix4 view);
    void renderUndefineCubes(Matrix4 projection, Matrix4 eye, Matrix4 view);
    void addCubeFromAnchor(anchorType type, int anchorIndex, Matrix4 mPose, Vector3 scale, bool isLost);
    void updateUndefineCubeScale(Matrix4 rightHandPose);
    void updateUndefineCubePosition(Matrix4 rightHandPose);
    void updateUndefineCubePose(Matrix4 rightHandPose);
    bool updateCubesStatus(Matrix4 rightHandPosition, Matrix4 leftHandPosition);
    bool checkInteract(Matrix4 rightHandPosition, Matrix4 leftHandPosition);
    void clearTargetCubes();
    void clearAllTargetCubes();
    void clearUndefineCubes();
    void clearInteractCubes();
    void changeTargetType(anchorType type);
};

#endif // WVR_NATIVE_ANCHOR_CUBE_H
