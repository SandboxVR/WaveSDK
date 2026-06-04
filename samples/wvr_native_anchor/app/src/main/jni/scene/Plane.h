#ifndef WVR_NATIVE_ANCHOR_PLANE_H
#define WVR_NATIVE_ANCHOR_PLANE_H
#include <Object.h>
#include <vector>
#include <array>
#include <Context.h>
#include "Axes.h"

typedef enum
{
    noRay = 0,           // no ray touch plane
    rightHandSelect = 1, // right hand ray touch
    leftHandSelect = 2,  // left hand ray touch
} planeState;

struct Plane
{
    Matrix4 mPose;
    Vector3 mScale;
    planeState status;
};

class PlaneManager : public Object
{
    std::vector<Plane> planesVector;
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;

    // opengl ID
    int mMatrixLocation;
    int mScale;
    int mColor;

    Axes *mAxes;

public:
    std::vector<Matrix4> intersectionPose_world;

    PlaneManager();
    ~PlaneManager();
    void addPlane(Matrix4 pose, Vector3 scale);
    void clearPlanes();
    void addSinglePlaneVertex(const Vector3 &v, std::vector<float> &vertdata);
    void setupShareVertex(std::vector<float> &vertdata, std::vector<uint32_t> &indexdata);
    void render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view);
    void updatePlanesStatus(Matrix4 rightCursorPose);
    bool checkEachInteract(Matrix4 cursorPose, Plane plane, Matrix4 &interactPose);
    Vector4 expressViaPlaneCoordinate(Vector4 worldCoord, Matrix4 planePose);
    Matrix4 mapingPoseOnPlane(Matrix4 cursorPose, Matrix4 planePose);
    void renderAxes(std::vector<Matrix4> &pose);
};
#endif // WVR_NATIVE_ANCHOR_PLANE_H
