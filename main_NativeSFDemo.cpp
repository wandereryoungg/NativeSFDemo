/*
 * Copyright (C) 2021 The Android Open Source Project
 */

#define LOG_TAG "NativeSFDemo"

#include <GLES/gl.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <hardware/gralloc.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>

#include "NativeSurfaceWrapper.h"

using namespace android;

bool mQuit = false;

int drawNativeSurface(sp<NativeSurfaceWrapper> nativeSurface) {
    status_t err = NO_ERROR;
    EGLDisplay display;
    EGLint majorVersion;
    EGLint minorVersion;
    EGLint w, h;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLint numConfigs;
    EGLConfig config;
    EGLContext eglContext = EGL_NO_CONTEXT;
    EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
                      EGL_OPENGL_ES2_BIT, EGL_NONE};
    ANativeWindow* nativeWindow = nativeSurface->getSurface().get();
    /*
    // 1. connect the ANativeWindow as a CPU client. Buffers will be queued
    after being filled using the CPU err =
    native_window_api_connect(nativeWindow, NATIVE_WINDOW_API_CPU); if (err !=
    NO_ERROR) { ALOGE("ERROR: unable to native_window_api_connect\n"); return
    EXIT_FAILURE;
    }
    */
    // 2. set the ANativeWindow dimensions
    err = native_window_set_buffers_user_dimensions(
        nativeWindow, nativeSurface->width(), nativeSurface->height());
    if (err != NO_ERROR) {
        ALOGE("ERROR: unable to native_window_set_buffers_user_dimensions\n");
        return EXIT_FAILURE;
    }

    // 3. set the ANativeWindow format
    err =
        native_window_set_buffers_format(nativeWindow, PIXEL_FORMAT_RGBX_8888);
    if (err != NO_ERROR) {
        ALOGE("ERROR: unable to native_window_set_buffers_format\n");
        return EXIT_FAILURE;
    }

    // 4. set the ANativeWindow usage
    err = native_window_set_usage(nativeWindow, GRALLOC_USAGE_SW_WRITE_OFTEN);
    if (err != NO_ERROR) {
        ALOGE("native_window_set_usage failed: %s (%d)", strerror(-err), -err);
        return err;
    }

    // 5. set the ANativeWindow scale mode
    err = native_window_set_scaling_mode(
        nativeWindow, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err != NO_ERROR) {
        ALOGE("native_window_set_scaling_mode failed: %s (%d)", strerror(-err),
              -err);
        return err;
    }

    // 6. set the ANativeWindow permission to allocte new buffer, default is
    // true
    static_cast<Surface*>(nativeWindow)
        ->getIGraphicBufferProducer()
        ->allowAllocation(true);

    // 7. set the ANativeWindow buffer count
    int numBufs = 0;
    int minUndequeuedBufs = 0;

    err = nativeWindow->query(
        nativeWindow, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
    if (err != NO_ERROR) {
        ALOGE(
            "error: MIN_UNDEQUEUED_BUFFERS query "
            "failed: %s (%d)",
            strerror(-err), -err);
        goto handle_error;
    }

    numBufs = minUndequeuedBufs + 1;
    err = native_window_set_buffer_count(nativeWindow, numBufs);
    if (err != NO_ERROR) {
        ALOGE("error: set_buffer_count failed: %s (%d)", strerror(-err), -err);
        goto handle_error;
    }

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, &majorVersion, &minorVersion);
    ALOGI("%s   majorVersion: %d minorVersion: %d", __func__, majorVersion,
          minorVersion);
    eglChooseConfig(display, attrs, &config, 1, &numConfigs);
    ALOGI("%s   numConfigs: %d", __func__, numConfigs);

    eglSurface = eglCreateWindowSurface(
        display, config, static_cast<Surface*>(nativeWindow), NULL);
    if (eglSurface == EGL_NO_SURFACE) {
        ALOGE("error eglCreateWindowSurface failed: %s",
              strerror(eglGetError()));
        goto handle_error;
    }

    eglContext = eglCreateContext(display, config, NULL, NULL);
    if (eglContext == EGL_NO_CONTEXT) {
        ALOGE("error eglCreateContext failed: %s", strerror(eglGetError()));
        goto handle_error;
    }

    eglQuerySurface(display, eglSurface, EGL_WIDTH, &w);
    eglQuerySurface(display, eglSurface, EGL_HEIGHT, &h);
    ALOGI("%s   w: %d h: %d", __func__, w, h);
    if (eglMakeCurrent(display, eglSurface, eglSurface, eglContext) ==
        EGL_FALSE)
        return NO_INIT;

    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);

    while (!mQuit) {
        glClearColor(0, 255, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(display, eglSurface);

        sleep(1);
    }

handle_error:

    if (eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(display, eglSurface);
    }
    if (eglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(display, eglContext);
    }

    // 15. Clean up after success or error.
    err = native_window_api_disconnect(nativeWindow, NATIVE_WINDOW_API_CPU);
    if (err != NO_ERROR) {
        ALOGE("error: api_disconnect failed: %s (%d)", strerror(-err), -err);
    }

    return err;
}

void sighandler(int num) {
    if (num == SIGINT) {
        printf("\nSIGINT: Force to stop !\n");
        mQuit = true;
    }
}

int main() {
    signal(SIGINT, sighandler);

    sp<NativeSurfaceWrapper> nativeSurface(
        new NativeSurfaceWrapper(String8("NativeSFDemo")));
    drawNativeSurface(nativeSurface);
    return 0;
}