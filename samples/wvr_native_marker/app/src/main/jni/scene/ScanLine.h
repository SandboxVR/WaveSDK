#ifndef WVR_NATIVE_HELLOVR_SCANLINE_H
#define WVR_NATIVE_HELLOVR_SCANLINE_H
#include <Matrices.h>
#include <Vectors.h>
#include <vector>
#include <log.h>
#include <Shader.h>
#include <Context.h>
#include <Object.h>

class ScanLine : public Object
{
    float scanLine_y;
    int mStartLocation;
    int mScale;
    std::vector<float> singleVertexArray;
    std::vector<u_int32_t> singleIndexArray;

public:
    ScanLine();
    ~ScanLine();
    void render();

private:
    void initVertex();
    void setupShareVertex(std::vector<float> &vertdata, std::vector<u_int32_t> &indexdata);
    void addSingleVertex(const Vector3 &v, std::vector<float> &vertdata);
};

#endif // WVR_NATIVE_HELLOVR_SCANLINE_H
