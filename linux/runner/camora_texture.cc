#include "camora_texture.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <vector>

struct CamoraTextureState {
    CamoraTextureState(CaptureEngine* capture_engine, int frame_width,
                       int frame_height, size_t frame_bytes)
        : capture(capture_engine),
          width(frame_width),
          height(frame_height),
          required_bytes(frame_bytes),
          pixels(frame_bytes) {
        for (size_t pixel = 0; pixel < frame_bytes; pixel += 4) {
            pixels[pixel + 0] = 32;
            pixels[pixel + 1] = 32;
            pixels[pixel + 2] = 32;
            pixels[pixel + 3] = 255;
        }
    }

    std::mutex mutex;
    CaptureEngine* capture;
    const int width;
    const int height;
    const size_t required_bytes;
    std::vector<uint8_t> pixels;
    uint64_t callback_count = 0;
    bool shutdown = false;
#ifndef NDEBUG
    std::chrono::steady_clock::time_point metrics_start =
        std::chrono::steady_clock::now();
    std::chrono::nanoseconds copy_time{0};
    uint64_t copy_count = 0;
#endif
};

struct _CamoraTexture {
    FlPixelBufferTexture parent_instance;
    CamoraTextureState* state;
};

G_DEFINE_TYPE(
    CamoraTexture,
    camora_texture,
    fl_pixel_buffer_texture_get_type()
)

static gboolean camora_texture_copy_pixels(
    FlPixelBufferTexture* texture,
    const uint8_t** out_buffer,
    uint32_t* width,
    uint32_t* height,
    GError** error
) {
    CamoraTexture* self = CAMORA_TEXTURE(texture);
    CamoraTextureState* state = self->state;
    if (!state) return FALSE;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->shutdown) return FALSE;

    const auto frame = state->capture && state->capture->running()
        ? state->capture->latestFrame()
        : std::shared_ptr<const VideoFrame>{};
    const bool matching_frame = frame &&
        frame->width == state->width &&
        frame->height == state->height &&
        frame->rgba.size() == state->required_bytes;

    assert(state->width > 0);
    assert(state->height > 0);
    assert(!state->pixels.empty());
    assert(state->pixels.size() == state->required_bytes);

    if (matching_frame) {
#ifndef NDEBUG
    using MetricsClock = std::chrono::steady_clock;
    const auto copy_start = MetricsClock::now();
#endif
    std::memcpy(
        state->pixels.data(),
        frame->rgba.data(),
        state->required_bytes
    );
#ifndef NDEBUG
    const auto copy_end = MetricsClock::now();
    state->copy_time +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            copy_end - copy_start);
    ++state->copy_count;
    if (copy_end - state->metrics_start >= std::chrono::seconds(1)) {
        const double average_ms =
            std::chrono::duration<double, std::milli>(
                state->copy_time).count() / state->copy_count;
        std::clog << "[Camora preview] copies=" << state->copy_count
                  << " copy_avg=" << average_ms << "ms" << std::endl;
        state->metrics_start = copy_end;
        state->copy_time = std::chrono::nanoseconds::zero();
        state->copy_count = 0;
    }
#endif

    // Flutter's preview is opaque. Preserve the immutable published RGBA
    // frame and flatten transparent pixels only in this texture-owned buffer.
    for (int y = 0; y < state->height; ++y) {
        for (int x = 0; x < state->width; ++x) {
            uint8_t* pixel = state->pixels.data() +
                static_cast<size_t>(y * state->width + x) * 4;
            const int alpha = pixel[3];
            if (alpha >= 255) continue;

            constexpr int checker_size = 18;
            const bool alternate =
                ((x / checker_size) + (y / checker_size)) % 2;
            const int checker = alternate ? 72 : 48;
            const int inverse_alpha = 255 - alpha;

            pixel[0] = static_cast<uint8_t>(
                (pixel[0] * alpha + checker * inverse_alpha + 127) / 255);
            pixel[1] = static_cast<uint8_t>(
                (pixel[1] * alpha + checker * inverse_alpha + 127) / 255);
            pixel[2] = static_cast<uint8_t>(
                (pixel[2] * alpha + checker * inverse_alpha + 127) / 255);
            pixel[3] = 255;
        }
    }

    }

    // This allocation never resizes. It remains alive until after Flutter
    // unregisters the texture and the GObject is finalized.

    ++state->callback_count;
    if (state->callback_count <= 10) {
        std::clog << "[Camora texture debug] callback="
                  << state->callback_count
                  << " texture=" << self
                  << " state=" << state
                  << " frame=" << frame.get()
                  << " src=" << (frame ? frame->rgba.data() : nullptr)
                  << " dst=" << static_cast<void*>(state->pixels.data())
                  << " frame=" << (frame ? frame->width : 0) << "x"
                  << (frame ? frame->height : 0)
                  << " texture=" << state->width << "x" << state->height
                  << " bytes=" << (frame ? frame->rgba.size() : 0)
                  << " required=" << state->required_bytes
                  << " copied=" << matching_frame
                  << std::endl;
    }

    *out_buffer = state->pixels.data();
    *width = static_cast<uint32_t>(state->width);
    *height = static_cast<uint32_t>(state->height);
    return TRUE;
}

static void camora_texture_finalize(GObject* object) {
    CamoraTexture* self = CAMORA_TEXTURE(object);
    delete self->state;
    self->state = nullptr;
    G_OBJECT_CLASS(camora_texture_parent_class)->finalize(object);
}

static void camora_texture_class_init(CamoraTextureClass* klass) {
    auto* texture_class = FL_PIXEL_BUFFER_TEXTURE_CLASS(klass);
    texture_class->copy_pixels = camora_texture_copy_pixels;

    auto* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = camora_texture_finalize;
}

static void camora_texture_init(CamoraTexture* self) {
    self->state = nullptr;
}

CamoraTexture* camora_texture_new(CaptureEngine* capture) {
    if (!capture || capture->width() <= 0 || capture->height() <= 0) {
        return nullptr;
    }

    const size_t width = static_cast<size_t>(capture->width());
    const size_t height = static_cast<size_t>(capture->height());
    if (width > std::numeric_limits<size_t>::max() / height / 4) {
        return nullptr;
    }
    const size_t required = width * height * 4;

    auto* texture = CAMORA_TEXTURE(
        g_object_new(CAMORA_TYPE_TEXTURE, nullptr));
    texture->state = new CamoraTextureState(
        capture, capture->width(), capture->height(), required);
    return texture;
}

void camora_texture_shutdown(CamoraTexture* texture) {
    if (!texture || !texture->state) return;
    std::lock_guard<std::mutex> lock(texture->state->mutex);
    texture->state->shutdown = true;
    texture->state->capture = nullptr;
}
