#ifndef WVR_NATIVE_MARKER_CUBE_H
#define WVR_NATIVE_MARKER_CUBE_H
#include <Matrices.h>
#include <Vectors.h>
#include <vector>
#include <log.h>
#include <Shader.h>
#include <Context.h>
#include <Object.h>
#include <VertexArrayObject.h>
#include <Texture.h>
#include <wvr/wvr_scene.h>
#include <wvr/wvr_marker.h>

typedef struct mTrackableArucoData;

class CubeManager : public Object
{
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;
    int mMatrixLocation;
    int mScale;
    int mCubeColor;
    int mUseInput;

public:
    std::vector<int> leftSelectCandidate;
    int nearestLeftSelect;
    CubeManager();
    ~CubeManager();
    void initCubes();
    void initTexture();
    void setupShareVertex(std::vector<float> &vertdata, std::vector<uint32_t> &indexdata);
    void addSingleCubeVertex(const Vector3 &v, float t0, float t1, std::vector<float> &vertdata);
    void renderCubes(Matrix4 projection, Matrix4 eye, Matrix4 view, std::vector<mTrackableArucoData> &markerData);
    void checkDeleteInteract(Matrix4 cursorPose, std::vector<mTrackableArucoData> &markerData);
};

#endif // WVR_NATIVE_MARKER_CUBE_H
