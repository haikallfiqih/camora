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

}
