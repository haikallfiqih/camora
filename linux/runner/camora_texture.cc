#include "camora_texture.h"

#include <mutex>
#include <vector>

struct _CamoraTexture {
    FlPixelBufferTexture parent_instance;

    CaptureEngine* capture;

    std::mutex mutex;
    std::vector<uint8_t> pixels;
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
    CamoraTexture* self =
        CAMORA_TEXTURE(texture);

    if (!self->capture ||
        !self->capture->running()) {
        return FALSE;
    }

    const int w =
        self->capture->width();

    const int h =
        self->capture->height();

    const int size =
        self->capture->frameSize();

    if (w <= 0 || h <= 0 || size <= 0) {
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(
        self->mutex
    );

    if (
        static_cast<int>(
            self->pixels.size()
        ) != size
    ) {
        self->pixels.resize(size);
    }

    if (!self->capture->copyLatestFrame(
            self->pixels.data(),
            size)) {
        return FALSE;
    }

    // Flutter's Linux texture preview is an RGB presentation surface for
    // Camora. When processed RGBA contains transparency, visualize it over
    // a checkerboard while keeping CaptureEngine's actual frame untouched.
    //
    // Opaque frames (normal, blur, image replacement) are unchanged.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t* pixel =
                self->pixels.data() +
                static_cast<size_t>(y * w + x) * 4;

            const int alpha = pixel[3];

            if (alpha >= 255) {
                continue;
            }

            constexpr int checkerSize = 18;

            const bool alternate =
                ((x / checkerSize) + (y / checkerSize)) % 2;

            const int checker =
                alternate ? 72 : 48;

            const int inverseAlpha = 255 - alpha;

            pixel[0] = static_cast<uint8_t>(
                (pixel[0] * alpha +
                 checker * inverseAlpha + 127) / 255);

            pixel[1] = static_cast<uint8_t>(
                (pixel[1] * alpha +
                 checker * inverseAlpha + 127) / 255);

            pixel[2] = static_cast<uint8_t>(
                (pixel[2] * alpha +
                 checker * inverseAlpha + 127) / 255);

            // Preview itself is now flattened.
            pixel[3] = 255;
        }
    }

    *out_buffer =
        self->pixels.data();

    *width =
        static_cast<uint32_t>(w);

    *height =
        static_cast<uint32_t>(h);

    return TRUE;
}

static void camora_texture_class_init(
    CamoraTextureClass* klass
) {
    auto* texture_class =
        FL_PIXEL_BUFFER_TEXTURE_CLASS(
            klass
        );

    texture_class->copy_pixels =
        camora_texture_copy_pixels;
}

static void camora_texture_init(
    CamoraTexture* self
) {
    self->capture = nullptr;
}

CamoraTexture* camora_texture_new(
    CaptureEngine* capture
) {
    auto* texture =
        CAMORA_TEXTURE(
            g_object_new(
                CAMORA_TYPE_TEXTURE,
                nullptr
            )
        );

    texture->capture = capture;

    return texture;
}
