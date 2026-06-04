#ifndef WVR_NATIVE_MARKER_PLANE_H
#define WVR_NATIVE_MARKER_PLANE_H
#include <Object.h>
#include <vector>
#include <array>
#include <Context.h>
#include "Axes.h"
#include <wvr/wvr_scene.h>
#include <wvr/wvr_marker.h>

class PlaneManager : public Object
{
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;

    // opengl ID
    int mMatrixLocation;
    int mScale;
    int mColor;

    Axes *mAxes;

public:
    std::vector<int> leftSelectCandidate;
    int nearestLeftSelect;
    std::vector<int> rightSelectCandidate;
    int nearestRightSelect;
    PlaneManager();
    ~PlaneManager();
    void addSinglePlaneVertex(const Vector3 &v, std::vector<float> &vertdata);
    void setupShareVertex(std::vector<float> &vertdata, std::vector<uint32_t> &indexdata);
    void render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view, std::vector<WVR_ArucoMarker> &markerData);
    void checkEachInteract(Matrix4 rightPose, Matrix4 leftPose, std::vector<WVR_ArucoMarker> &markerData);
    Vector4 expressViaPlaneCoordinate(Vector4 worldCoord, Matrix4 planePose);
    void renderAxes(std::vector<Matrix4> &pose);
};
#endif // WVR_NATIVE_MARKER_PLANE_H
