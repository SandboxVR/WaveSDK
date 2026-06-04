// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#define LOG_TAG "HelloVR_Text"
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

Text::Text(float characterSize, bool headlock) : Object(), mIsHeadlock(headlock) {
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
    mTexture = Texture::loadTexture("textures/font.png");
    if (mTexture == NULL) {
        mHasError = true;
        return;
    }

    mVAO->bindVAO();
    mVAO->bindArrayBuffer();

    glBufferData(GL_ARRAY_BUFFER, sizeof(mVertices), mVertices, GL_DYNAMIC_DRAW);

    int stride = (2 + 3) * sizeof(float);
    GLuint offset = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (const void*) offset);

    offset += sizeof(float) * 3;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (const void*) offset);

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

Text::~Text() {
}

// A--C
// |  |
// B--D
// Vertices should be: A-B-C, B-D-C
// Texture coord is in this order: B-A-D, A-C-D
void Text::genCharCoord(GLfloat * texCoord, char character, int stride) {
    if (character < ' ')
        character = '?';
    if (character > '~')
        character = '?';
    int v = character - ' ';
    //LOGD("v %d, step x %f, step y %f", v, FONT_X_STEP, FONT_Y_STEP);

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

/*
    // Texture coord is in this order: A-B-C, B-D-C
    // A
    texCoord[0] = x;
    texCoord[1] = y + FONT_Y_STEP;
    // B
    texCoord[stride] = x;
    texCoord[stride + 1] = y;
    // C
    texCoord[stride * 2] = x + FONT_X_STEP;
    texCoord[stride * 2 + 1] = y + FONT_Y_STEP;
    // B
    texCoord[stride * 3] = x;
    texCoord[stride * 3 + 1] = y;
    // D
    texCoord[stride * 4] = x + FONT_X_STEP;
    texCoord[stride * 4 + 1] = y;
    // C
    texCoord[stride * 5] = x + FONT_X_STEP;
    texCoord[stride * 5 + 1] = y + FONT_Y_STEP;
*/
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
void Text::genCharVertices(GLfloat * vertices, float textStartX,
                                float textStartY, float textStartZ,
                                int charIndex, int stride) {
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
void Text::addOneLineText(const char* text, bool overwrite, bool append, float x, float y, float z) {
    if (text == NULL)
        return;

    if (overwrite) {
        mLines = 0;
        mCurrentSize = 0;
        memset(mVertices, 0, mArraySize);
    }

    int length = strnlen(text, MAX_TEXT_LENGTH);
    if (mCurrentSize + length > MAX_TEXT_LENGTH) {
        length = MAX_TEXT_LENGTH - mCurrentSize;
        if (length == 0) {
            LOGE("Too many characters...");
            return;
        }
    }

    mTextStartX = !append ? x : mTextStartX;
    mTextStartY = !append ? y : mTextStartY  - (mLines > 0 ? mCharacterSizeY : 0);
    mTextStartZ = !append ? z : mTextStartZ;

    // Make vertex array
    for (int i = 0; i < length; i++) {
        genCharVertices(mVertices + ((mCurrentSize + i) * 6 * mStride),
                        mTextStartX, mTextStartY, mTextStartZ, i, mStride);
        genCharCoord(mVertices + ((mCurrentSize + i) * 6 * mStride) + 3, text[i], mStride);
    }


    mVAO->bindArrayBuffer();
    glBufferData(GL_ARRAY_BUFFER, sizeof(mVertices), mVertices, GL_DYNAMIC_DRAW);
    mVAO->unbindArrayBuffer();

    mLines ++;
    mCurrentSize += length;

    // Log
    if (FULL_LOG) {
        char text_[MAX_TEXT_LENGTH + 1] = {0};
        strncpy(text_, text, length);
        LOGI("OneLineText:%s added in %f, %f, %f. Current size = %d, lines = %d",
             text_, mTextStartX, mTextStartY, mTextStartZ, mCurrentSize, mLines);
    }
}

void Text::dumpVertices() {
    LOGI("dumpVertices");
    for (int i = 0; i < mCurrentSize; i++) {
        for (int j = 0; j < 6; j++) {
            int vertexIndex = (i * 6 + j) * mStride;
            LOGI("vertex %d: %.3f, %.3f, %.3f   %.3f, %.3f", i, mVertices[vertexIndex], mVertices[vertexIndex + 1],
                mVertices[vertexIndex + 2], mVertices[vertexIndex + 3], mVertices[vertexIndex + 4]);
        }
        LOGI("");
    }
}

void Text::draw(const Matrix4& projection, const Matrix4& eye, const Matrix4& view, const Vector4& lightDir) {
    if (!mEnable)
        return;

    if (mCurrentSize == 0)
        return;

    Matrix4 matrix = projection;
    if (!mIsHeadlock) {
        matrix = projection * eye * view;
    }

    mShader->useProgram();
    glUniformMatrix4fv(mMatrixLocation, 1, false, matrix.get());

    glActiveTexture(GL_TEXTURE0);
    mTexture->bindTexture();

    mVAO->bindVAO();

    GLboolean blend = glIsEnabled(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glDrawArrays(GL_TRIANGLES, 0, mCurrentSize * 6);

    if (!blend)
        glDisable(GL_BLEND);
    mShader->unuseProgram();
    mVAO->unbindVAO();

}

void Text::drawMultiview(const Matrix4 projection[2], const Matrix4 eye[2], const Matrix4& view, const Vector4& lightDir) {
    if (!mEnable)
        return;

    if (mCurrentSize == 0)
        return;

    Matrix4 matrix[2];
    matrix[0] = projection[0];
    matrix[1] = projection[1];
    if (!mIsHeadlock) {
        matrix[0] = projection[0] * eye[0] * view;
        matrix[1] = projection[1] * eye[1] * view;
    }

    GLfloat float_array[32];
    memcpy(float_array, matrix[0].get(), 16*sizeof(GLfloat));
    memcpy(float_array+16, matrix[1].get(), 16*sizeof(GLfloat));

    mMultiviewShader->useProgram();
    glUniformMatrix4fv(mMultiviewMatrixLocation, 2, false, float_array);

    glActiveTexture(GL_TEXTURE0);
    mTexture->bindTexture();

    mVAO->bindVAO();
    GLboolean blend = glIsEnabled(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glDrawArrays(GL_TRIANGLES, 0, mCurrentSize * 6);

    if (!blend)
        glDisable(GL_BLEND);
    mMultiviewShader->unuseProgram();
    mVAO->unbindVAO();
}

void Text::setCharacterSize(float x, float y) {
    mCharacterSizeX = x;
    mCharacterSizeY = (y == 0.0f) ? (x * FONT_HEIGHT / FONT_WIDTH) : y;
}
