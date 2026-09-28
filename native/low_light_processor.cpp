#include "low_light_processor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

void LowLightProcessor::setEnabled(bool enabled) {
    if (enabled_ && !enabled) {
        resetTemporalState();
    }

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

void LowLightProcessor::resetTemporalState() {
    temporalFrame_.clear();
    temporalWidth_ = 0;
    temporalHeight_ = 0;
}

void LowLightProcessor::temporalDenoise(
    uint8_t* rgba,
    int width,
    int height,
    float amount
) {
    if (!rgba || width <= 0 || height <= 0) {
        return;
    }

    const size_t frameSize =
        static_cast<size_t>(width) *
        static_cast<size_t>(height) * 4;

    if (temporalFrame_.size() != frameSize ||
        temporalWidth_ != width ||
        temporalHeight_ != height) {

        temporalFrame_.resize(frameSize);

        std::memcpy(
            temporalFrame_.data(),
            rgba,
            frameSize
        );

        temporalWidth_ = width;
        temporalHeight_ = height;

        return;
    }

    const size_t pixelCount =
        static_cast<size_t>(width) *
        static_cast<size_t>(height);

    // Integer temporal filter.
    //
    // No float blending in the hot pixel path.
    // Bright pixels bypass temporal filtering completely.
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t* current =
            rgba + i * 4;

        uint8_t* history =
            temporalFrame_.data() + i * 4;

        // Cheap approximate luminance.
        //
        // (R*54 + G*183 + B*19) / 256
        const int currentY =
            (
                54 * current[0] +
                183 * current[1] +
                19 * current[2]
            ) >> 8;

        // Bright areas generally don't need low-light denoise.
        // Refresh history and move on.
        if (currentY >= 145) {
            history[0] = current[0];
            history[1] = current[1];
            history[2] = current[2];
            history[3] = current[3];
            continue;
        }

        const int historyY =
            (
                54 * history[0] +
                183 * history[1] +
                19 * history[2]
            ) >> 8;

        const int difference =
            std::abs(currentY - historyY);

        // Motion / large scene change:
        // trust current frame and reset history here.
        if (difference >= 18) {
            history[0] = current[0];
            history[1] = current[1];
            history[2] = current[2];
            history[3] = current[3];
            continue;
        }

        // Determine history weight using integer fractions.
        //
        // Deep shadows get the strongest filtering.
        // Midtones remain conservative to protect faces/details.
        int historyWeight = 0;

        if (currentY < 55) {
            if (difference <= 3) {
                historyWeight = 5; // 5/8
            } else if (difference <= 8) {
                historyWeight = 4; // 4/8
            } else {
                historyWeight = 2; // 2/8
            }
        } else if (currentY < 100) {
            if (difference <= 3) {
                historyWeight = 4; // 4/8
            } else if (difference <= 8) {
                historyWeight = 3; // 3/8
            } else {
                historyWeight = 1; // 1/8
            }
        } else {
            if (difference <= 3) {
                historyWeight = 2; // 2/8
            } else {
                historyWeight = 0;
            }
        }

        // At lower enhancement strength, reduce temporal filtering.
        if (amount < 0.45f && historyWeight > 0) {
            --historyWeight;
        }

        if (historyWeight <= 0) {
            history[0] = current[0];
            history[1] = current[1];
            history[2] = current[2];
            history[3] = current[3];
            continue;
        }

        const int currentWeight =
            8 - historyWeight;

        // Rounded integer blend.
        const uint8_t r =
            static_cast<uint8_t>(
                (
                    current[0] * currentWeight +
                    history[0] * historyWeight +
                    4
                ) >> 3
            );

        const uint8_t g =
            static_cast<uint8_t>(
                (
                    current[1] * currentWeight +
                    history[1] * historyWeight +
                    4
                ) >> 3
            );

        const uint8_t b =
            static_cast<uint8_t>(
                (
                    current[2] * currentWeight +
                    history[2] * historyWeight +
                    4
                ) >> 3
            );

        current[0] = r;
        current[1] = g;
        current[2] = b;

        // Filtered output becomes next temporal reference.
        history[0] = r;
        history[1] = g;
        history[2] = b;
        history[3] = current[3];
    }
}

void LowLightProcessor::process(
    uint8_t* rgba,
    int width,
    int height
) {
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
        resetTemporalState();
        return;
    }

    // Clean sensor noise before lifting shadows/midtones.
    temporalDenoise(
        rgba,
        width,
        height,
        amount
    );

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
