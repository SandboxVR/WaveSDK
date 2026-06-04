#ifndef WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#define WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#include <Plane.h>
#include "Cube.h"
#include <wvr/wvr_anchor.h>
#include <wvr/wvr_scene.h>
#include <wvr/wvr_device.h>
class anchorManager
{
    WVR_SpatialAnchor *mAnchor;
    WVR_ScenePlane *mPlane;

    uint32_t anchorCount;
    uint32_t planeCount;
    bool perceptionOn = false;

public:
    anchorManager();
    ~anchorManager();
    // anchor
    bool init(CubeManager *cm, PlaneManager *pm);
    void deinit();
    bool getExistedPlanes(PlaneManager *pm);
    bool createCubeAnchor(Matrix4 pose, Vector3 scale);
    bool getExistedAnchor(CubeManager *cm);
    bool updateExistedAnchor(PlaneManager *pm, CubeManager *cm);
    bool addCubeAnchor(PlaneManager *pm, Matrix4 controllerPose, Vector3 scale = Vector3(0.05, 0.05, 0.05));
    bool addUndefineCubeAnchor(CubeManager *cm);
    bool deleteChosenAnchor2changing(CubeManager *cm);
    bool deleteChosenAnchor(CubeManager *cm);
    Vector3 getScaleFromName(std::string str);
    bool clearAnchor();
    // utility
    Matrix4 convertToMatrix4(WVR_Pose_t pose);
    WVR_Pose_t convertToWVRPose(Matrix4 pose);
};
#endif // WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
