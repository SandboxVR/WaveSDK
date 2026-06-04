#ifndef WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#define WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
#include <Plane.h>
#include "Cube.h"
#include "SceneMesh.h"
#include "SceneObject.h"
#include <wvr/wvr_anchor.h>
#include <wvr/wvr_scene.h>
#include <wvr/wvr_device.h>
#include <thread>

typedef enum
{
    IDEAL = 0,
    IMPORT_ANCHOR = 1,
    EXPORT_ANCHOR = 2
} TaskType;

struct Task
{
    WVR_SpatialAnchorName persistedName;
    TaskType workType;
    uint32_t persistDataSize;
    char *persistData;
};
typedef struct Task Task;

class BGWork
{
    std::thread bgThread;
    Task *taskPtr;

public:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_condv;
    Task task;
    BGWork();
    ~BGWork();
    bool exportPersisted(WVR_SpatialAnchorName anchorName);
    bool importPersisted();
    bool checkStatus();
};

class anchorManager
{
    WVR_SpatialAnchor *allSpatialAnchor;
    uint32_t allSpatialAnchorCount;
    uint32_t cachedAnchorCount;
    uint32_t persistedAnchorCount;

    WVR_ScenePlane *mPlane;
    uint32_t mPlaneCount;
    bool shouldUpdatePlaneVertex;

    WVR_SceneObject *mObject;
    uint32_t mObjectCount;
    bool shouldUpdateObjectVertex;

public:
    // export and import persisted anchor on back ground thread
    BGWork *backGroundThread;

    anchorManager();
    ~anchorManager();
    bool init(CubeManager *cm, PlaneManager *pm, SceneObjectManager *om);
    void startScenePerception();
    void deinit();
    bool getExistedPlanes(PlaneManager *pm);
    bool planeChanged(uint32_t newCount, WVR_ScenePlane* newScenePlane);
    bool createCubeAnchor(anchorType type, Matrix4 pose, Vector3 scale);
    bool getExistedSpatialAnchor(CubeManager *cm);
    bool getExistedCachedAnchor();
    bool getExistedPersistedAnchor();
    bool getExistedObjects(SceneObjectManager *om);
    bool objectChanged(uint32_t newCount, WVR_SceneObject* newScenePlane);
    bool updateExistedAnchor(CubeManager *cm);
    bool addCubeAnchor(anchorType type, PlaneManager *pm, Matrix4 controllerPose, Vector3 scale = Vector3(0.05, 0.05, 0.05));
    bool addUndefineCubeAnchor(anchorType type, CubeManager *cm);
    bool deleteChosenAnchor2changing(anchorType type, CubeManager *cm);
    bool deleteChosenAnchor(anchorType type, CubeManager *cm);
    bool exportPersistCubeAnchor(anchorType type, CubeManager *cm);
    bool importPersistCubeAnchor(anchorType type, CubeManager *cm);
    Vector3 getScaleFromName(std::string str);
    bool clearTypeAnchor(anchorType type, CubeManager *cm);
    //  utility
    Matrix4 convertToMatrix4(WVR_Pose_t pose);
    WVR_Pose_t convertToWVRPose(Matrix4 pose);
    // mesh
    void getMesh_plane(PlaneManager *pm, sceneMeshData2D *sceneMesh2D);
    void getMesh_scene(sceneMeshData3D *sceneMesh3D); // visual mesh and collider mesh
    void getMesh_object(SceneObjectManager *om, sceneMeshData3D *sceneMesh3D);
};

#endif // WVR_NATIVE_ANCHOR_ANCHORMANAGER_H
