#pragma once

#include <cstdint>

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
    ) const;

private:
    bool enabled_ = false;
    int strength_ = 50;
};
