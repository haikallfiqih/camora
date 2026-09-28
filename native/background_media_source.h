#pragma once

#include <atomic>
#include <string>
#include <thread>

class CaptureEngine;
typedef struct _GdkPixbuf GdkPixbuf;

class BackgroundMediaSource {
public:
    explicit BackgroundMediaSource(CaptureEngine& capture);
    ~BackgroundMediaSource();

    bool start(
        const std::string& path,
        int targetWidth,
        int targetHeight,
        std::string& error
    );
    void stop();

private:
    void runPixbuf(std::string path, int targetWidth, int targetHeight);
    void runVideo(std::string path, int targetWidth, int targetHeight);
    bool publishPixbuf(GdkPixbuf* source, int targetWidth, int targetHeight);
    bool waitForStop(int milliseconds);

    CaptureEngine& capture_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};
