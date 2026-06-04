#ifndef WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#define WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#include <Plane.h>
#include "Cube.h"
#include "SceneMesh.h"
#include <wvr/wvr_anchor.h>
#include <wvr/wvr_scene.h>
#include <wvr/wvr_device.h>
class anchorManager
{
    WVR_SpatialAnchor *mAnchor;
    WVR_ScenePlane *mPlane;

    uint32_t anchorCount;
    uint32_t planeCount;

public:
    anchorManager();
    ~anchorManager();
    bool init(CubeManager *cm, PlaneManager *pm);
    void startScenePerception();
    void deinit();
    bool getExistedPlanes(PlaneManager *pm);
    bool createCubeAnchor(Matrix4 pose, Vector3 scale);
    bool getExistedAnchor(CubeManager *cm);
    bool updateExistedAnchor(CubeManager *cm);
    bool addCubeAnchor(PlaneManager *pm, Matrix4 controllerPose, Vector3 scale = Vector3(0.05, 0.05, 0.05));
    bool addUndefineCubeAnchor(CubeManager *cm);
    bool deleteChosenAnchor2changing(CubeManager *cm);
    bool deleteChosenAnchor(CubeManager *cm);
    Vector3 getScaleFromName(std::string str);
    bool clearAnchor();
    // utility
    Matrix4 convertToMatrix4(WVR_Pose_t pose);
    WVR_Pose_t convertToWVRPose(Matrix4 pose);
    // mesh
    void getMesh_2D(PlaneManager* pm, sceneMeshData2D* sceneMesh2D);
    void getMesh_3D(sceneMeshData3D* sceneMesh3D);//visual mesh and collider mesh
};
#endif // WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
