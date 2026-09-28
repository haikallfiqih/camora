#include "background_media_source.h"

#include "capture_engine.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iostream>
#include <vector>

namespace {

std::string lowercaseExtension(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return std::tolower(value); });
    return extension;
}

bool isVideoExtension(const std::string& extension) {
    return extension == "mp4" || extension == "m4v" ||
           extension == "mov" || extension == "webm";
}

}  // namespace

BackgroundMediaSource::BackgroundMediaSource(CaptureEngine& capture)
    : capture_(capture) {}

BackgroundMediaSource::~BackgroundMediaSource() {
    stop();
}

bool BackgroundMediaSource::start(
    const std::string& path,
    int targetWidth,
    int targetHeight,
    std::string& error
) {
    stop();
    if (path.empty() || targetWidth <= 0 || targetHeight <= 0) {
        error = "Choose a valid background file.";
        return false;
    }
    if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
        error = "The selected background file is unavailable.";
        return false;
    }

    const std::string extension = lowercaseExtension(path);
    const bool video = isVideoExtension(extension);
    const bool pixbuf = extension == "jpg" || extension == "jpeg" ||
        extension == "png" || extension == "webp" || extension == "gif" ||
        extension == "svg";
    if (!video && !pixbuf) {
        error = "Unsupported background format.";
        return false;
    }

    stopRequested_ = false;
    worker_ = std::thread(
        video ? &BackgroundMediaSource::runVideo
              : &BackgroundMediaSource::runPixbuf,
        this, path, targetWidth, targetHeight);
    return true;
}

void BackgroundMediaSource::stop() {
    stopRequested_ = true;
    if (worker_.joinable()) worker_.join();
}

bool BackgroundMediaSource::waitForStop(int milliseconds) {
    int remaining = std::max(1, milliseconds);
    while (remaining > 0 && !stopRequested_) {
        const int chunk = std::min(remaining, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        remaining -= chunk;
    }
    return stopRequested_;
}

bool BackgroundMediaSource::publishPixbuf(
    GdkPixbuf* source,
    int targetWidth,
    int targetHeight
) {
    if (!source || stopRequested_) return false;

    const int sourceWidth = gdk_pixbuf_get_width(source);
    const int sourceHeight = gdk_pixbuf_get_height(source);
    if (sourceWidth <= 0 || sourceHeight <= 0) return false;

    const double scale = std::max(
        static_cast<double>(targetWidth) / sourceWidth,
        static_cast<double>(targetHeight) / sourceHeight);
    const int scaledWidth = std::max(
        targetWidth, static_cast<int>(std::ceil(sourceWidth * scale)));
    const int scaledHeight = std::max(
        targetHeight, static_cast<int>(std::ceil(sourceHeight * scale)));

    g_autoptr(GdkPixbuf) scaled = gdk_pixbuf_scale_simple(
        source, scaledWidth, scaledHeight, GDK_INTERP_BILINEAR);
    if (!scaled) return false;

    const int cropX = (scaledWidth - targetWidth) / 2;
    const int cropY = (scaledHeight - targetHeight) / 2;
    g_autoptr(GdkPixbuf) image = gdk_pixbuf_new_subpixbuf(
        scaled, cropX, cropY, targetWidth, targetHeight);
    if (!image) return false;

    const int channels = gdk_pixbuf_get_n_channels(image);
    const int rowStride = gdk_pixbuf_get_rowstride(image);
    const guchar* inputPixels = gdk_pixbuf_read_pixels(image);
    if (!inputPixels || (channels != 3 && channels != 4)) return false;

    std::vector<uint8_t> output(
        static_cast<size_t>(targetWidth) * targetHeight * 4);
    for (int y = 0; y < targetHeight; ++y) {
        const guchar* row = inputPixels + y * rowStride;
        for (int x = 0; x < targetWidth; ++x) {
            const guchar* input = row + x * channels;
            uint8_t* pixel = output.data() +
                static_cast<size_t>(y * targetWidth + x) * 4;
            pixel[0] = input[0];
            pixel[1] = input[1];
            pixel[2] = input[2];
            pixel[3] = channels == 4 ? input[3] : 255;
        }
    }

    if (stopRequested_) return false;
    capture_.setBackgroundReplacement(
        true, std::move(output), targetWidth, targetHeight);
    return true;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
void BackgroundMediaSource::runPixbuf(
    std::string path,
    int targetWidth,
    int targetHeight
) {
    GError* error = nullptr;
    GdkPixbufAnimation* animation =
        gdk_pixbuf_animation_new_from_file(path.c_str(), &error);
    if (!animation) {
        std::cerr << "Background media error: "
                  << (error ? error->message : "image could not be decoded")
                  << std::endl;
        if (error) g_error_free(error);
        return;
    }

    GdkPixbufAnimationIter* iterator =
        gdk_pixbuf_animation_get_iter(animation, nullptr);
    if (!iterator) {
        g_object_unref(animation);
        return;
    }

    const bool isStatic = gdk_pixbuf_animation_is_static_image(animation);
    while (!stopRequested_) {
        GdkPixbuf* frame = gdk_pixbuf_animation_iter_get_pixbuf(iterator);
        if (!publishPixbuf(frame, targetWidth, targetHeight)) break;
        if (isStatic) break;

        const int delay = std::max(
            10, gdk_pixbuf_animation_iter_get_delay_time(iterator));
        if (waitForStop(delay)) break;
        gdk_pixbuf_animation_iter_advance(iterator, nullptr);
    }

    g_object_unref(iterator);
    g_object_unref(animation);
}

G_GNUC_END_IGNORE_DEPRECATIONS

void BackgroundMediaSource::runVideo(
    std::string path,
    int targetWidth,
    int targetHeight
) {
    gchar* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
    if (!uri) return;

    const std::string pipelineDescription =
        "uridecodebin uri=\"" + std::string(uri) +
        "\" ! videoconvert ! video/x-raw,format=RGBA ! "
        "appsink name=background-sink max-buffers=1 drop=true sync=true";
    g_free(uri);

    GError* error = nullptr;
    GstElement* pipeline =
        gst_parse_launch(pipelineDescription.c_str(), &error);
    if (!pipeline) {
        std::cerr << "Background video error: "
                  << (error ? error->message : "pipeline could not be created")
                  << std::endl;
        if (error) g_error_free(error);
        return;
    }

    GstElement* sink = gst_bin_get_by_name(
        GST_BIN(pipeline), "background-sink");
    GstBus* bus = gst_element_get_bus(pipeline);
    if (!sink || !bus) {
        if (sink) gst_object_unref(sink);
        if (bus) gst_object_unref(bus);
        gst_object_unref(pipeline);
        return;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    while (!stopRequested_) {
        GstSample* sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(sink), 100 * GST_MSECOND);
        if (sample) {
            GstCaps* caps = gst_sample_get_caps(sample);
            GstVideoInfo info;
            gst_video_info_init(&info);
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo map{};
            if (caps && gst_video_info_from_caps(&info, caps) && buffer &&
                GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_RGBA &&
                gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                const int width = GST_VIDEO_INFO_WIDTH(&info);
                const int height = GST_VIDEO_INFO_HEIGHT(&info);
                const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
                if (width > 0 && height > 0 && stride >= width * 4 &&
                    map.size >= static_cast<size_t>(stride) * height) {
                    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
                        map.data, GDK_COLORSPACE_RGB, TRUE, 8,
                        width, height, stride, nullptr, nullptr);
                    if (pixbuf) {
                        publishPixbuf(pixbuf, targetWidth, targetHeight);
                        g_object_unref(pixbuf);
                    }
                }
                gst_buffer_unmap(buffer, &map);
            }
            gst_sample_unref(sample);
        }

        GstMessage* message = gst_bus_pop_filtered(
            bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR |
                                             GST_MESSAGE_EOS));
        if (!message) continue;
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
            gst_element_seek_simple(
                pipeline, GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                          GST_SEEK_FLAG_KEY_UNIT),
                0);
        } else {
            GError* streamError = nullptr;
            gchar* details = nullptr;
            gst_message_parse_error(message, &streamError, &details);
            std::cerr << "Background video error: "
                      << (streamError ? streamError->message : "decode failed")
                      << std::endl;
            if (streamError) g_error_free(streamError);
            g_free(details);
            gst_message_unref(message);
            break;
        }
        gst_message_unref(message);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
}
