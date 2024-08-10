/*
 * Copyright (C) 2021 The Android Open Source Project
 */

#define LOG_TAG "NativeSFDemo"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <hardware/gralloc.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>
#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkTypeface.h>
#include <SkFontStyle.h>
#include <SkFont.h>
#include <SkStream.h>
#include <SkEncodedImageFormat.h>
#include <SkImageEncoder.h>

#include "NativeSurfaceWrapper.h"

using namespace android;

bool mQuit = false;

void saveBitmapToFile(const SkBitmap& bitmap, const char* filename) {
    SkFILEWStream stream(filename);
    if (!stream.isValid()) {
        ALOGE("%s   Failed to create output file %s", __func__, filename);
        return;
    }

    SkPixmap pixmap;
    if (!bitmap.peekPixels(&pixmap)) {
        ALOGE("%s   Failed to get bitmap pixels", __func__);
        return;
    }

    bool ret = SkEncodeImage(&stream, pixmap, SkEncodedImageFormat::kPNG, 100);
    if (!ret) {
        ALOGE("%s   Failed to encode bitmap", __func__);
    }
}

int drawNativeSurface(sp<NativeSurfaceWrapper> nativeSurface) {
    status_t err = NO_ERROR;
    int countFrame = 0;
    ANativeWindow* nativeWindow = nativeSurface->getSurface().get();

    // 1. connect the ANativeWindow as a CPU client. Buffers will be queued
    // after being filled using the CPU
    err = native_window_api_connect(nativeWindow, NATIVE_WINDOW_API_CPU);
    if (err != NO_ERROR) {
        ALOGE("ERROR: unable to native_window_api_connect\n");
        return EXIT_FAILURE;
    }

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

    // 8. draw the ANativeWindow
    while (!mQuit) {
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(nativeWindow, &buffer, NULL) == 0) {

            SkImageInfo info = SkImageInfo::Make(buffer.width, buffer.height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            ssize_t bytesPerLine = buffer.stride * bytesPerPixel(buffer.format);
            ALOGI("%s   bytesPerLine: %zu", __func__, bytesPerLine);
            SkBitmap bitmap;
            bitmap.installPixels(info, buffer.bits, bytesPerLine);
            // bitmap.allocPixels(info);

            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setColor(SK_ColorRED);

            SkCanvas canvas(bitmap);

            std::string tmp = "hello " + std::to_string(countFrame);
            SkString drawStr(tmp);

            SkFont font(SkTypeface::MakeFromName("monospace", SkFontStyle()), 20);

            canvas.clear(SK_ColorTRANSPARENT);
            canvas.drawColor(SK_ColorWHITE);

            canvas.drawRect(SkRect::MakeLTRB(10, 10, 200, 200), paint);

            canvas.drawSimpleText(drawStr.c_str(), drawStr.size(), SkTextEncoding::kUTF8, buffer.width / 2, buffer.height / 2, font, paint);
            ALOGI("%s   draw Text: %s len: %zu", __func__, drawStr.c_str(), drawStr.size());

            /*
            static bool debug = true;
            if (debug) {
                saveBitmapToFile(bitmap, "/data/camera/young.png");
                debug = false;
            }
            */

            ANativeWindow_unlockAndPost(nativeWindow);
            countFrame++;
        } else {
            ALOGE("%s   lock failed: %s", __func__, strerror(errno));
            break;
        }
        sleep(1);
    }

handle_error:
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