#include "capture_engine.h"
#include <cmath>
#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

static std::string escape_json(const std::string& input) {
    std::string output;

    for (char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '"':  output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }

    return output;
}

static std::string control_type_name(uint32_t type) {
    switch (type) {
        case V4L2_CTRL_TYPE_INTEGER: return "integer";
        case V4L2_CTRL_TYPE_BOOLEAN: return "boolean";
        case V4L2_CTRL_TYPE_MENU: return "menu";
        case V4L2_CTRL_TYPE_INTEGER_MENU: return "integer_menu";
        case V4L2_CTRL_TYPE_BUTTON: return "button";
        case V4L2_CTRL_TYPE_INTEGER64: return "integer64";
        default: return "unknown";
    }
}


static CaptureEngine g_capture;

extern "C" {


const char* camora_list_cameras() {
    static std::string result;
    result.clear();

    DIR* dir = opendir("/dev");

    if (!dir) {
        result = "[]";
        return result.c_str();
    }

    std::vector<std::string> devices;

    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            devices.emplace_back("/dev/" + std::string(entry->d_name));
        }
    }

    closedir(dir);

    std::sort(devices.begin(), devices.end());

    std::ostringstream json;
    json << "[";

    bool first = true;

    for (const auto& path : devices) {
        int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);

        if (fd < 0) continue;

        v4l2_capability cap{};

        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            close(fd);
            continue;
        }

        uint32_t deviceCaps =
            (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                ? cap.device_caps
                : cap.capabilities;

        bool capture =
            (deviceCaps & V4L2_CAP_VIDEO_CAPTURE) ||
            (deviceCaps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);

        bool streaming =
            deviceCaps & V4L2_CAP_STREAMING;

        if (!capture || !streaming) {
            close(fd);
            continue;
        }

        if (!first) json << ",";
        first = false;

        json
            << "{"
            << "\"path\":\"" << escape_json(path) << "\","
            << "\"name\":\""
            << escape_json(reinterpret_cast<const char*>(cap.card))
            << "\","
            << "\"driver\":\""
            << escape_json(reinterpret_cast<const char*>(cap.driver))
            << "\","
            << "\"bus\":\""
            << escape_json(reinterpret_cast<const char*>(cap.bus_info))
            << "\""
            << "}";

        close(fd);
    }

    json << "]";

    result = json.str();

    return result.c_str();
}

const char* camora_list_controls(const char* device_path) {
    static std::string result;
    result.clear();

    if (!device_path) {
        result = "[]";
        return result.c_str();
    }

    int fd = open(device_path, O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        result = "[]";
        return result.c_str();
    }

    std::ostringstream json;
    json << "[";

    bool firstControl = true;

    v4l2_query_ext_ctrl query{};
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL |
               V4L2_CTRL_FLAG_NEXT_COMPOUND;

    while (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) == 0) {
        const bool disabled =
            query.flags & V4L2_CTRL_FLAG_DISABLED;

        const bool controlClass =
            query.type == V4L2_CTRL_TYPE_CTRL_CLASS;

        if (!disabled && !controlClass) {
            v4l2_control control{};
            control.id = query.id;

            int64_t current = query.default_value;

            if (ioctl(fd, VIDIOC_G_CTRL, &control) == 0) {
                current = control.value;
            }

            if (!firstControl) json << ",";
            firstControl = false;

            json
                << "{"
                << "\"id\":" << query.id << ","
                << "\"name\":\""
                << escape_json(
                    reinterpret_cast<const char*>(query.name)
                )
                << "\","
                << "\"type\":\""
                << control_type_name(query.type)
                << "\","
                << "\"min\":" << query.minimum << ","
                << "\"max\":" << query.maximum << ","
                << "\"step\":" << query.step << ","
                << "\"default\":" << query.default_value << ","
                << "\"value\":" << current << ","
                << "\"inactive\":"
                << (
                    (query.flags & V4L2_CTRL_FLAG_INACTIVE)
                        ? "true"
                        : "false"
                );

            if (
                query.type == V4L2_CTRL_TYPE_MENU ||
                query.type == V4L2_CTRL_TYPE_INTEGER_MENU
            ) {
                json << ",\"options\":[";

                bool firstOption = true;

                for (
                    int64_t index = query.minimum;
                    index <= query.maximum;
                    index++
                ) {
                    v4l2_querymenu menu{};
                    menu.id = query.id;
                    menu.index = static_cast<uint32_t>(index);

                    if (ioctl(fd, VIDIOC_QUERYMENU, &menu) != 0) {
                        continue;
                    }

                    if (!firstOption) json << ",";
                    firstOption = false;

                    json
                        << "{"
                        << "\"value\":" << index << ","
                        << "\"label\":\"";

                    if (query.type == V4L2_CTRL_TYPE_MENU) {
                        json << escape_json(
                            reinterpret_cast<const char*>(menu.name)
                        );
                    } else {
                        json << menu.value;
                    }

                    json << "\"}";
                }

                json << "]";
            }

            json << "}";
        }

        query.id |=
            V4L2_CTRL_FLAG_NEXT_CTRL |
            V4L2_CTRL_FLAG_NEXT_COMPOUND;
    }

    json << "]";

    close(fd);

    result = json.str();

    return result.c_str();
}

int camora_set_control(
    const char* device_path,
    uint32_t control_id,
    int32_t value
) {
    if (!device_path) return -1;

    int fd = open(device_path, O_RDWR | O_NONBLOCK);

    if (fd < 0) return -2;

    v4l2_control control{};
    control.id = control_id;
    control.value = value;

    const int result =
        ioctl(fd, VIDIOC_S_CTRL, &control);

    close(fd);

    return result == 0 ? 0 : -3;
}



int camora_capture_start(
    const char* device,
    int width,
    int height,
    int fps
) {
    if (!device) return -1;

    return g_capture.start(
        device,
        width,
        height,
        fps
    ) ? 0 : -2;
}

void camora_capture_stop() {
    g_capture.stop();
}

int camora_capture_width() {
    return g_capture.width();
}

int camora_capture_height() {
    return g_capture.height();
}

int camora_capture_frame_size() {
    return g_capture.frameSize();
}

int camora_capture_copy_frame(
    unsigned char* destination,
    int destination_size
) {
    return g_capture.copyLatestFrame(
        destination,
        destination_size
    ) ? 1 : 0;
}

}

// -----------------------------------------------------------------------------
// Camera format enumeration
// -----------------------------------------------------------------------------

extern "C" const char* camora_list_formats(const char* device) {
    static thread_local std::string result;
    result.clear();

    if (!device) {
        result = "[]";
        return result.c_str();
    }

    int fd = open(device, O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        result = "[]";
        return result.c_str();
    }

    std::ostringstream json;
    json << "[";

    bool first = true;

    v4l2_fmtdesc fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmt.index = 0;
         ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0;
         ++fmt.index) {

        v4l2_frmsizeenum size{};
        size.pixel_format = fmt.pixelformat;

        for (size.index = 0;
             ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0;
             ++size.index) {

            if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
                continue;
            }

            v4l2_frmivalenum interval{};
            interval.pixel_format = fmt.pixelformat;
            interval.width =
                size.discrete.width;
            interval.height =
                size.discrete.height;

            for (interval.index = 0;
                 ioctl(
                     fd,
                     VIDIOC_ENUM_FRAMEINTERVALS,
                     &interval) == 0;
                 ++interval.index) {

                if (interval.type !=
                    V4L2_FRMIVAL_TYPE_DISCRETE) {
                    continue;
                }

                const auto numerator =
                    interval.discrete.numerator;

                const auto denominator =
                    interval.discrete.denominator;

                if (numerator == 0) {
                    continue;
                }

                const double fps =
                    static_cast<double>(
                        denominator) /
                    static_cast<double>(
                        numerator);

                char fourcc[5] = {
                    static_cast<char>(
                        fmt.pixelformat & 0xFF),
                    static_cast<char>(
                        (fmt.pixelformat >> 8) & 0xFF),
                    static_cast<char>(
                        (fmt.pixelformat >> 16) & 0xFF),
                    static_cast<char>(
                        (fmt.pixelformat >> 24) & 0xFF),
                    '\0',
                };

                if (!first) {
                    json << ",";
                }

                first = false;

                json
                    << "{"
                    << "\"pixelFormat\":\""
                    << fourcc
                    << "\","
                    << "\"width\":"
                    << size.discrete.width
                    << ","
                    << "\"height\":"
                    << size.discrete.height
                    << ","
                    << "\"fps\":"
                    << static_cast<int>(
                           std::round(fps))
                    << "}";
            }
        }
    }

    json << "]";

    close(fd);

    result = json.str();

    return result.c_str();
}
