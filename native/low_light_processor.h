#pragma once

#include <cstdint>
#include <vector>

class LowLightProcessor {
public:
    void setEnabled(bool enabled);
    void setStrength(int strength);

    bool enabled() const;
    int strength() const;

    void process(
        uint8_t* rgba,
        int width,
        int height
    );

private:
    void resetTemporalState();

    void temporalDenoise(
        uint8_t* rgba,
        int width,
        int height,
        float amount
    );

    bool enabled_ = false;
    int strength_ = 50;

    std::vector<uint8_t> temporalFrame_;
    int temporalWidth_ = 0;
    int temporalHeight_ = 0;
};
