#include <gtest/gtest.h>

#include "../include/synthesizer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int SampleRate = 44100;
constexpr int InputSamples = 512;
constexpr int ChannelCount = 8;

void setupSynthesizer(Synthesizer &synth) {
    Synthesizer::Parameters params;
    params.audioBufferSize = 8192;
    params.inputBufferSize = 2048;
    params.audioSampleRate = SampleRate;
    params.inputSampleRate = SampleRate;
    params.inputChannelCount = ChannelCount;
    params.initialAudioParameters.airNoise = 0.0f;
    params.initialAudioParameters.inputSampleNoise = 0.0f;
    params.initialAudioParameters.levelerMaxGain = 1.0f;
    params.initialAudioParameters.levelerMinGain = 1.0f;
    params.initialAudioParameters.dF_F_mix = 0.0f;
    synth.initialize(params);

    // Production initializes one impulse response per exhaust channel as well.
    const int16_t impulse[] = { INT16_MAX };
    for (int channel = 0; channel < ChannelCount; ++channel) {
        synth.initializeImpulseResponse(impulse, 1, 1.0f, channel);
    }
}

std::vector<int16_t> renderSignal(bool threaded, double amplitude) {
    Synthesizer synth;
    setupSynthesizer(synth);
    for (int i = 0; i < InputSamples; ++i) {
        const double value = amplitude * std::sin(2.0 * 3.141592653589793 * 440.0 * i / SampleRate);
        double data[ChannelCount];
        std::fill_n(data, ChannelCount, value);
        synth.writeInput(data);
    }

    // Equal input/output rates produce one initial interpolation sample plus
    // one sample per input. Stay below the renderer's 2000-sample batch limit.
    if (threaded) synth.startAudioRenderingThread();
    synth.endInputBlock();
    if (threaded) {
        synth.waitProcessed();
        // Join before inspecting output; waitProcessed alone reports input consumption.
        synth.endAudioRenderingThread();
    }
    else {
        synth.renderAudio();
    }

    constexpr int RequestedSamples = InputSamples + 8;
    constexpr int16_t Guard = 12345;
    std::vector<int16_t> output(RequestedSamples + 2, Guard);
    const int returned = synth.readAudioOutput(RequestedSamples, output.data() + 1);
    EXPECT_EQ(returned, InputSamples + 1);
    EXPECT_EQ(output.front(), Guard);
    EXPECT_EQ(output.back(), Guard);
    for (int i = returned; i < RequestedSamples; ++i) EXPECT_EQ(output[i + 1], 0);
    synth.destroy();
    return std::vector<int16_t>(output.begin() + 1, output.begin() + 1 + std::max(0, returned));
}
}

TEST(SynthesizerTests, SynthesizerSanityCheck) {
    Synthesizer synth;
    setupSynthesizer(synth);
    synth.destroy();
}

TEST(SynthesizerTests, SynthesizerSystemTestSingleThread) {
    const auto silence = renderSignal(false, 0.0);
    EXPECT_TRUE(std::all_of(silence.begin(), silence.end(), [](int16_t v) { return v == 0; }));
    const auto signal = renderSignal(false, 1000.0);
    ASSERT_FALSE(signal.empty());
    // A real input must yield both polarities without clipping at this amplitude.
    EXPECT_GT(*std::max_element(signal.begin(), signal.end()), 100);
    EXPECT_LT(*std::min_element(signal.begin(), signal.end()), -100);
    EXPECT_TRUE(std::all_of(signal.begin(), signal.end(), [](int16_t v) { return v > INT16_MIN && v < INT16_MAX; }));
}

TEST(SynthesizerTests, SynthesizerSystemTest) {
    // With noise disabled, background rendering must match synchronous PCM output.
    const auto expected = renderSignal(false, 1000.0);
    EXPECT_EQ(renderSignal(true, 1000.0), expected);
}
