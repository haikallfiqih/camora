#include "low_light_processor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

void LowLightProcessor::setEnabled(bool enabled) {
    enabled_ = enabled;
}

void LowLightProcessor::setStrength(int strength) {
    strength_ = std::clamp(strength, 0, 100);
}

bool LowLightProcessor::enabled() const {
    return enabled_;
}

int LowLightProcessor::strength() const {
    return strength_;
}

void LowLightProcessor::process(
    uint8_t* rgba,
    int width,
    int height
) const {
    if (!enabled_ || !rgba || strength_ <= 0 ||
        width <= 0 || height <= 0) {
        return;
    }

    const int pixelCount = width * height;

    // ---------------------------------------------------------
    // 1. Estimate scene luminance.
    // ---------------------------------------------------------

    constexpr int targetSamples = 10000;

    const int sampleStep =
        std::max(
            1,
            pixelCount / targetSamples
        );

    double luminanceSum = 0.0;
    int samples = 0;

    for (int i = 0; i < pixelCount; i += sampleStep) {
        const uint8_t* pixel =
            rgba + i * 4;

        const float r =
            pixel[0] / 255.0f;

        const float g =
            pixel[1] / 255.0f;

        const float b =
            pixel[2] / 255.0f;

        const float y =
            0.2126f * r +
            0.7152f * g +
            0.0722f * b;

        if (y < 0.88f) {
            luminanceSum += y;
            ++samples;
        }
    }

    if (samples == 0) {
        return;
    }

    const float sceneLuminance =
        static_cast<float>(
            luminanceSum / samples
        );

    const float userAmount =
        static_cast<float>(strength_) /
        100.0f;

    const float darkness =
        std::clamp(
            (0.55f - sceneLuminance) / 0.42f,
            0.0f,
            1.0f
        );

    const float amount =
        darkness * userAmount;

    if (amount < 0.01f) {
        return;
    }

    // ---------------------------------------------------------
    // 2. Build luminance LUT.
    // ---------------------------------------------------------

    std::array<float, 256> luminanceScale{};

    const float gamma =
        1.0f - (0.34f * amount);

    for (int value = 0; value < 256; ++value) {
        const float y =
            static_cast<float>(value) /
            255.0f;

        if (y <= 0.001f) {
            luminanceScale[value] = 1.0f;
            continue;
        }

        float curved =
            std::pow(y, gamma);

        // Midtone illumination / virtual fill-light.
        const float midtone =
            std::exp(
                -std::pow(
                    (y - 0.38f) / 0.24f,
                    2.0f
                )
            );

        const float midtoneLift =
            0.12f *
            amount *
            midtone;

        curved +=
            (1.0f - curved) *
            midtoneLift;

        // Near-black protection.
        const float blackProtection =
            std::clamp(
                y / 0.12f,
                0.0f,
                1.0f
            );

        curved =
            y +
            (curved - y) *
            blackProtection;

        // Highlight protection.
        const float highlightProtection =
            1.0f -
            std::clamp(
                (y - 0.58f) / 0.32f,
                0.0f,
                1.0f
            );

        curved =
            y +
            (curved - y) *
            highlightProtection;

        float scale =
            curved / y;

        const float maxScale =
            1.0f +
            (0.95f * amount);

        luminanceScale[value] =
            std::clamp(
                scale,
                1.0f,
                maxScale
            );
    }

    // ---------------------------------------------------------
    // 3. Apply illumination LUT.
    // ---------------------------------------------------------

    for (int i = 0; i < pixelCount; ++i) {
        uint8_t* pixel =
            rgba + i * 4;

        const float r =
            pixel[0] / 255.0f;

        const float g =
            pixel[1] / 255.0f;

        const float b =
            pixel[2] / 255.0f;

        const float y =
            0.2126f * r +
            0.7152f * g +
            0.0722f * b;

        const int yIndex =
            std::clamp(
                static_cast<int>(
                    y * 255.0f
                ),
                0,
                255
            );

        const float scale =
            luminanceScale[yIndex];

        float outR = r * scale;
        float outG = g * scale;
        float outB = b * scale;

        const float newY =
            0.2126f * outR +
            0.7152f * outG +
            0.0722f * outB;

        // Mild chroma-noise protection.
        const float deepShadow =
            1.0f -
            std::clamp(
                newY / 0.22f,
                0.0f,
                1.0f
            );

        const float saturation =
            1.0f -
            (
                0.14f *
                amount *
                deepShadow
            );

        outR =
            newY +
            (outR - newY) *
            saturation;

        outG =
            newY +
            (outG - newY) *
            saturation;

        outB =
            newY +
            (outB - newY) *
            saturation;

        pixel[0] =
            static_cast<uint8_t>(
                std::clamp(
                    outR,
                    0.0f,
                    1.0f
                ) * 255.0f
            );

        pixel[1] =
            static_cast<uint8_t>(
                std::clamp(
                    outG,
                    0.0f,
                    1.0f
                ) * 255.0f
            );

        pixel[2] =
            static_cast<uint8_t>(
                std::clamp(
                    outB,
                    0.0f,
                    1.0f
                ) * 255.0f
            );
    }
}
