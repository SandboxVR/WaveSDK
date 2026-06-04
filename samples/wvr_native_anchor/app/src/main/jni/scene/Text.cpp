// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#include <Object.h>
#include <Context.h>
#include <Shader.h>
#include <Texture.h>
#include <VertexArrayObject.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>
#include <Text.h>
#include <log.h>

#define FULL_LOG false

const GLfloat FONT_WIDTH = 120.0f;
const GLfloat FONT_HEIGHT = 227.0f;
const GLfloat FONT_IMAGE_WIDTH = 2048.0f;
const GLfloat FONT_IMAGE_HEIGHT = 2048.0f;
const GLfloat FONT_X_STEP = FONT_WIDTH / FONT_IMAGE_WIDTH;
const GLfloat FONT_Y_STEP = FONT_HEIGHT / FONT_IMAGE_HEIGHT;

Text::Text(float characterSize, bool headlock) : Object()
{
    mName = LOG_TAG;
    mEnable = false;
    memset(mVertices, 0, mArraySize);
    setCharacterSize(characterSize);

    loadShaderFromAsset("shader/vertex/vt_vertex.glsl", "shader/fragment/t_fragment.glsl");
    if (mHasError)
        return;
    mMatrixLocation = mShader->getUniformLocation("matrix");

    loadMultiviewShaderFromAsset("shader/vertex/vtm_vertex.glsl", "shader/fragment/t_fragment.glsl");
    if (mHasError)
        return;
    mMultiviewMatrixLocation = mMultiviewShader->getUniformLocation("matrix");

    mVAO = new VertexArrayObject(true, false);
    mTexture = Texture::loadTexture("textures/fontColor.png");
    if (mTexture == NULL)
    {
        mHasError = true;
        return;
    }

    mVAO->bindVAO();
    mVAO->bindArrayBuffer();

    glBufferData(GL_ARRAY_BUFFER, sizeof(mVertices), mVertices, GL_DYNAMIC_DRAW);

    int stride = (2 + 3) * sizeof(float);
    GLuint offset = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void *)offset);

    offset += sizeof(float) * 3;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (const void *)offset);

    mVAO->unbindVAO();
    mVAO->unbindArrayBuffer();

    mTexture->bindTexture();
    mTexture->bindBitmap(GL_RGBA4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    mTexture->unbindTexture();
    mTexture->cleanBitmap();
}

Text::~Text()
{
}

// A--C
// |  |
// B--D
// Vertices should be: A-B-C, B-D-C
// Texture coord is in this order: B-A-D, A-C-D
void Text::genCharCoord(GLfloat *texCoord, char character, int stride)
{
    if (character < ' ')
        character = '?';
    if (character > '~')
        character = '?';
    int v = character - ' ';
    // LOGD("v %d, step x %f, step y %f", v, FONT_X_STEP, FONT_Y_STEP);

    float x = ((v % 16) * FONT_WIDTH) / FONT_IMAGE_WIDTH;
    float y = ((int)(v / 16)) * FONT_HEIGHT / FONT_IMAGE_HEIGHT;

    // Texture coord is in this order: B-A-D, A-C-D
    // B
    texCoord[0] = x;
    texCoord[1] = y;
    // A
    texCoord[stride] = x;
    texCoord[stride + 1] = y + FONT_Y_STEP;
    // D
    texCoord[stride * 2] = x + FONT_X_STEP;
    texCoord[stride * 2 + 1] = y;
    // A
    texCoord[stride * 3] = x;
    texCoord[stride * 3 + 1] = y + FONT_Y_STEP;
    // C
    texCoord[stride * 4] = x + FONT_X_STEP;
    texCoord[stride * 4 + 1] = y + FONT_Y_STEP;
    // D
    texCoord[stride * 5] = x + FONT_X_STEP;
    texCoord[stride * 5 + 1] = y;
}

// A--C
// |  |
// B--D
// Vertices should be: A-B-C, B-D-C
// Texture coord is in this order: B-A-D, A-C-D
//
// verteices : the out vertex array
// textStartX, textStarY, textStartZ: the start point of the text in GL world
// charIndex : the character index in the text
// stride : the size of one vertex
void Text::genCharVertices(GLfloat *vertices, float textStartX,
                           float textStartY, float textStartZ,
                           int charIndex, int stride)
{
    // A
    vertices[0] = textStartX + (charIndex * mCharacterSizeX);
    vertices[1] = textStartY;
    vertices[2] = textStartZ;

    // B
    vertices[stride] = textStartX + (charIndex * mCharacterSizeX);
    vertices[stride + 1] = textStartY - mCharacterSizeY;
    vertices[stride + 2] = textStartZ;

    // C
    vertices[stride * 2] = textStartX + (charIndex * mCharacterSizeX) + mCharacterSizeX;
    vertices[stride * 2 + 1] = textStartY;
    vertices[stride * 2 + 2] = textStartZ;

    // B
    vertices[stride * 3] = textStartX + (charIndex * mCharacterSizeX);
    vertices[stride * 3 + 1] = textStartY - mCharacterSizeY;
    vertices[stride * 3 + 2] = textStartZ;

    // D
    vertices[stride * 4] = textStartX + (charIndex * mCharacterSizeX) + mCharacterSizeX;
    vertices[stride * 4 + 1] = textStartY - mCharacterSizeY;
    vertices[stride * 4 + 2] = textStartZ;

    // C
    vertices[stride * 5] = textStartX + (charIndex * mCharacterSizeX) + mCharacterSizeX;
    vertices[stride * 5 + 1] = textStartY;
    vertices[stride * 5 + 2] = textStartZ;
}

// text: The text string
// overwrite: Overwrite the existed text
// append: false - Place this line in the specified position.
//         true - Append this line to the existed text.
// x, y, z: Indicate the new position. Ignored if append is true.
void Text::addOneLineText(const char *text, bool overwrite, bool append, float x, float y, float z)
{
    if (text == NULL)
        return;

    if (overwrite)
    {
        mLines = 0;
        mCurrentSize = 0;
        memset(mVertices, 0, mArraySize);
    }

    int length = strnlen(text, MAX_TEXT_LENGTH);
    if (mCurrentSize + length > MAX_TEXT_LENGTH)
    {
        length = MAX_TEXT_LENGTH - mCurrentSize;
        if (length == 0)
        {
            LOGE("Too many characters...");
            return;
        }
    }

    mTextStartX = !append ? x : mTextStartX;
    mTextStartY = !append ? y : mTextStartY - (mLines > 0 ? mCharacterSizeY : 0);
    mTextStartZ = !append ? z : mTextStartZ;

    // Make vertex array
    mTextCentertX = (mTextStartX + ((length - 1) * mCharacterSizeX)) / 2;
    for (int i = 0; i < length; i++)
    {
        genCharVertices(mVertices + ((mCurrentSize + i) * 6 * mStride),
                        mTextStartX, mTextStartY, mTextStartZ, i, mStride);
        genCharCoord(mVertices + ((mCurrentSize + i) * 6 * mStride) + 3, text[i], mStride);
    }

    mVAO->bindArrayBuffer();
    glBufferData(GL_ARRAY_BUFFER, sizeof(mVertices), mVertices, GL_DYNAMIC_DRAW);
    mVAO->unbindArrayBuffer();

    mLines++;
    mCurrentSize += length;

    // Log
    if (FULL_LOG)
    {
        char text_[MAX_TEXT_LENGTH + 1] = {0};
        strncpy(text_, text, length);
        LOGI("OneLineText:%s added in %f, %f, %f. Current size = %d, lines = %d",
             text_, mTextStartX, mTextStartY, mTextStartZ, mCurrentSize, mLines);
    }
}

void Text::addSequenceText(textObject textData)
{
    texts.push_back(textData);
}

void Text::dumpVertices()
{
    LOGI("dumpVertices");
    for (int i = 0; i < mCurrentSize; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            int vertexIndex = (i * 6 + j) * mStride;
            LOGI("vertex %d: %.3f, %.3f, %.3f   %.3f, %.3f", i, mVertices[vertexIndex], mVertices[vertexIndex + 1],
                 mVertices[vertexIndex + 2], mVertices[vertexIndex + 3], mVertices[vertexIndex + 4]);
        }
        LOGI("");
    }
}

void Text::clearTexts()
{
    for (int a = 0; a < texts.size(); a++)
    {
        texts[a].text.clear();
    }
    texts.clear();
}

void Text::drawSequence(const Matrix4 &projection, const Matrix4 &eye, const Matrix4 &view, const Vector4 &lightDir)
{
    GLboolean oldCullingFace = glIsEnabled(GL_CULL_FACE);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    mShader->useProgram();

    for (int a = 0; a < texts.size(); a++)
    {
        addOneLineText(texts[a].text[0].c_str(), true, false, texts[a].extraShift.x, texts[a].extraShift.y + mCharacterSizeY * (texts[a].text.size() - 1), texts[a].extraShift.z);
        for (int b = 1; b < texts[a].text.size(); b++)
        {
            addOneLineText(texts[a].text[b].c_str(), false, true, 0.0, 0.0, 0.0);
        }

        if (!mEnable)
            return;
        if (mCurrentSize == 0)
            return;

        Matrix4 matrix;
        Matrix4 extraRotate;
        if (!texts[a].mIsHeadlock)
        {
            Matrix4 translate2Center;
            translate2Center.translate(-mTextCentertX, 0, 0);
            extraRotate = extraRotate.rotateX(texts[a].extraRotate.x);
            extraRotate = extraRotate.rotateX(texts[a].extraRotate.y);
            extraRotate = extraRotate.rotateX(texts[a].extraRotate.z);

            matrix = projection * eye * view * texts[a].pose * translate2Center * extraRotate;
        }
        else
        {
            Matrix4 translate2Center;
            translate2Center.translate(-mTextCentertX, 0, 0);
            matrix = projection * translate2Center;
        }

        glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());
        glActiveTexture(GL_TEXTURE0);
        mTexture->bindTexture();
        mVAO->bindVAO();
        glDrawArrays(GL_TRIANGLES, 0, mCurrentSize * 6);
        mVAO->unbindVAO();
    }

    if (!oldBlend)
        glDisable(GL_BLEND);
    if (oldCullingFace)
        glEnable(GL_CULL_FACE);

    mShader->unuseProgram();
}

void Text::setCharacterSize(float x, float y)
{
    mCharacterSizeX = x;
    mCharacterSizeY = (y == 0.0f) ? (x * FONT_HEIGHT / FONT_WIDTH) : y;
}
