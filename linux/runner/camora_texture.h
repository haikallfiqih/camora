#pragma once

#include <flutter_linux/flutter_linux.h>

#include "../../native/capture_engine.h"

G_BEGIN_DECLS

#define CAMORA_TYPE_TEXTURE camora_texture_get_type()

G_DECLARE_FINAL_TYPE(
    CamoraTexture,
    camora_texture,
    CAMORA,
    TEXTURE,
    FlPixelBufferTexture
)

CamoraTexture* camora_texture_new(
    CaptureEngine* capture
);

void camora_texture_shutdown(CamoraTexture* texture);

G_END_DECLS
