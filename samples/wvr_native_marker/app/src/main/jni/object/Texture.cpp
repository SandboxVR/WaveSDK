// "WaveVR SDK 
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."

#define LOG_TAG "Texture"
#include <Texture.h>
#include <Context.h>
#include <log.h>
#include <android/bitmap.h>
#include <GLES2/gl2.h>

Texture::Texture() :
    mTexture(0),
    mBitmap(NULL),
    mWidth(0),
    mHeight(0),
    mStride(0),
    mSize(0) {
}

Texture::~Texture() {
    clear();
}

void Texture::clear() {
    if (mTexture != 0) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
    }
    cleanBitmap();
}

void Texture::setTexture(int texture) {
    clear();
    mTexture = texture;
}

Texture * Texture::genTexture() {
    GLuint texture = 0;
    glGenTextures(1, &texture);

    Texture * m = new Texture();
    m->setTexture(texture);
    return m;
}

Texture * Texture::loadTextureFromBitmapWithoutCached(WVR_CtrlerTexBitmap_t &wvrBitmap)
{
    Texture *texture = nullptr;
    if (wvrBitmap.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        texture = genTexture();
        (*texture).mBitmap = nullptr;
        (*texture).mWidth = wvrBitmap.width;
        (*texture).mHeight = wvrBitmap.height;
        (*texture).mStride = wvrBitmap.stride;
        (*texture).mSize = wvrBitmap.stride * wvrBitmap.height;
        (*texture).mType = GL_UNSIGNED_BYTE;
        (*texture).mFormat = GL_RGBA;
        //
        glBindTexture(GL_TEXTURE_2D, (*texture).mTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); //only one leve1.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (*texture).mWidth, (*texture).mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, wvrBitmap.bitmap);

    } else {
        LOGW("We only support RGBA8888.format(%d)", wvrBitmap.format);
    }

    return texture;
}

Texture * Texture::loadTexture(const char * assetFile) {
    Context * context = Context::getInstance();
    EnvWrapper ew = context->getEnv();
    JNIEnv * env = ew.get();

    AssetFile textureFile(context->getAssetManager(), assetFile);
    if (!textureFile.open())
        return NULL;

    const void * data = textureFile.getBuffer();
    size_t length = textureFile.getLength();
    BitmapFactory * bf = context->getBitmapFactory();

    AndroidBitmapInfo info;
    uint8_t * bmp = bf->decodeByteArray(env, data, length, info);
    if (bmp == NULL)
        return NULL;

    Texture * texture = genTexture();
    texture->mBitmap = bmp;
    texture->mWidth = info.width;
    texture->mHeight = info.height;
    texture->mStride = info.stride;
    texture->mSize = info.stride * info.height;
    switch (info.format) {
    case ANDROID_BITMAP_FORMAT_RGB_565:
        texture->mType = GL_UNSIGNED_SHORT_5_6_5;
        texture->mFormat = GL_RGB565;
        break;
    case ANDROID_BITMAP_FORMAT_RGBA_4444:
        texture->mType = GL_UNSIGNED_SHORT_4_4_4_4;
        texture->mFormat = GL_RGBA4;
        break;
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
        texture->mType = GL_UNSIGNED_BYTE;
        texture->mFormat = GL_RGBA;
        break;
    case ANDROID_BITMAP_FORMAT_A_8:
        texture->mType = GL_UNSIGNED_BYTE;
        texture->mFormat = GL_RED;
        break;
    default:
        texture->mType = GL_UNSIGNED_BYTE;
        texture->mFormat = GL_RGBA;
        break;
    }

    return texture;
}