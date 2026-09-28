#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <cstdint>
#include <vector>

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

        // Camora only exposes actual video-capture devices.
        // Metadata nodes such as /dev/video3 are ignored.
        bool capture =
            (deviceCaps & V4L2_CAP_VIDEO_CAPTURE) ||
            (deviceCaps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);

        bool streaming = deviceCaps & V4L2_CAP_STREAMING;

        if (!capture || !streaming) {
            close(fd);
            continue;
        }

        if (!first) json << ",";
        first = false;

        auto escape = [](const std::string& input) {
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
        };

        json
            << "{"
            << "\"path\":\"" << escape(path) << "\","
            << "\"name\":\""
            << escape(reinterpret_cast<const char*>(cap.card))
            << "\","
            << "\"driver\":\""
            << escape(reinterpret_cast<const char*>(cap.driver))
            << "\","
            << "\"bus\":\""
            << escape(reinterpret_cast<const char*>(cap.bus_info))
            << "\""
            << "}";

        close(fd);
    }

    json << "]";
    result = json.str();

    return result.c_str();
}

}
