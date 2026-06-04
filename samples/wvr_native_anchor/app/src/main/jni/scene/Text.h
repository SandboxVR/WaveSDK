// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."
#ifndef WVR_NATIVE_MARKER_TEXT_H
#define WVR_NATIVE_MARKER_TEXT_H
#include <Object.h>

struct textObject
{
    bool mIsHeadlock;
    std::vector<std::string> text;
    Vector3 extraShift;
    Vector3 extraRotate;
    Matrix4 pose;
};

class Text : public Object
{
private:
    int mMatrixLocation = 0;
    int mMultiviewMatrixLocation = 0;

    const static int MAX_TEXT_LENGTH = 255;
    const static int mStride = 5;
    const static int mArraySize = MAX_TEXT_LENGTH * mStride * 6;
    float mVertices[mArraySize];
    int mCurrentSize = 0;
    int mLines = 0;
    float mTextStartX = -0.5f;
    float mTextStartY = -0.5f;
    float mTextStartZ = -1.0f;

    float mCharacterSizeX = 0.1f;
    float mCharacterSizeY;

    float mTextCentertX = -1.0;

public:
    Text(float characterSize, bool headlock = true);
    ~Text();
    void genCharCoord(GLfloat *texCoord, char character, int stride);
    void genCharVertices(GLfloat *vertices, float textStartX,
                         float textStartY, float textStartZ,
                         int charIndex, int stride);
    void addOneLineText(const char *text, bool overwrite, bool append,
                        float x, float y, float z);
    void addSequenceText(textObject textData);
    void dumpVertices();
    void clearTexts();
    virtual void drawSequence(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view, const Vector4 &lightDir);

private:
    std::vector<textObject> texts;
    void setCharacterSize(float x, float y = 0.0f);
};
#endif // WVR_NATIVE_MARKER_TEXT_H