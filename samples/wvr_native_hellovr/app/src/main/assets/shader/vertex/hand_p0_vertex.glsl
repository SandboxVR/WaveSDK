#version 300 es

#define APPLY_BONE

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 worldMat;
uniform mat3 normalMat; //vs
uniform mat4 boneMats[48]; //Os

layout(location = 0) in vec3 v3Position;
layout(location = 1) in vec3 v3Normal;
layout(location = 2) in vec2 v2Coord1;
layout(location = 3) in vec2 v2Coord2;
layout(location = 5) in ivec4 v4BoneID;
layout(location = 6) in vec4 v4BoneWeight;

void main() {
    vec4 localVertex;

    //1. calculate vertex data.
#ifdef APPLY_BONE
    mat4 localPose = mat4(1.0);

    localPose = boneMats[v4BoneID[0]] * v4BoneWeight[0];
    localPose += boneMats[v4BoneID[1]] * v4BoneWeight[1];
    localPose += boneMats[v4BoneID[2]] * v4BoneWeight[2];
    localPose += boneMats[v4BoneID[3]] * v4BoneWeight[3];

    localVertex = localPose * vec4(v3Position.xyz, 1.0);
#else
    localVertex = vec4(v3Position.xyz, 1.0);
#endif

    gl_Position = projMat * viewMat * worldMat * localVertex;
}