
#ifndef WVR_NATIVE_ANCHOR_SCENEOBJECT_H
#define WVR_NATIVE_ANCHOR_SCENEOBJECT_H
#include <Object.h>
#include <vector>
#include <array>
#include <Context.h>
#include "Axes.h"

struct SceneObject
{
    Matrix4 mPose;
    Vector3 mScale;
    int index;
};

class SceneObjectManager : public Object
{
    std::vector<SceneObject> objectsVector;
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;

    // opengl ID
    int mMatrixLocation;
    int mScale;
    int mColor;

public:
    SceneObjectManager();
    ~SceneObjectManager();
    void addSceneObject(int index, Matrix4 pose, Vector3 scale);
    void clearSceneObjects();
    void addSingleObjectVertex(const Vector3 &v, std::vector<float> &vertdata);
    void setupShareVertex(std::vector<float> &vertdata, std::vector<uint32_t> &indexdata);
    std::vector<SceneObject> getSceneObjects();
    void render(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view);
};

#endif // WVR_NATIVE_ANCHOR_SCENEOBJECT_H
