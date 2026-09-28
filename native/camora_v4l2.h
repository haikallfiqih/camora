#pragma once

#include <cstdint>

extern "C" {

const char* camora_list_cameras();

const char* camora_list_controls(
    const char* device_path
);

int camora_set_control(
    const char* device_path,
    uint32_t control_id,
    int32_t value
);

const char* camora_list_formats(
    const char* device
);

}
