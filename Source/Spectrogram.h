#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// Analysis is performed off the audio thread. This viewer uses FFT magnitudes
// from the imported recording, not a trained AI model.
class LevelSpectrogram : private juce::Thread
{
public:
    LevelSpectrogram() : juce::Thread("LEVEL Spectrogram FFT") {}
    ~LevelSpectrogram() override { stopThread(-1); }

    void analyse(const juce::File& input)
    {
        stopThread(-1);
        {
            const juce::ScopedLock lock(imageGuard);
            rendered = {};
        }
        source = input;
        startThread();
    }
    juce::Image getImage() const
    {
        const juce::ScopedLock lock(imageGuard);
        return rendered;
    }
private:
    void run() override
    {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(source));
        if (!reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) return;
        constexpr int fftSize = 2048, imageWidth = 160, imageHeight = 96;
        juce::dsp::FFT fft(11);
        juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> fftData(fftSize * 2, 0.f);
        juce::AudioBuffer<float> chunk(1, fftSize);
        juce::Image img(juce::Image::RGB, imageWidth, imageHeight, true);
        const float maxFreq = (float)juce::jmin(16000.0, reader->sampleRate * 0.49);
        if (maxFreq <= 200.f) return;
        for (int x = 0; x < imageWidth && !threadShouldExit(); ++x)
        {
            const auto remaining = juce::jmax((juce::int64)0, reader->lengthInSamples - fftSize);
            const auto offset = (juce::int64)((double)x / juce::jmax(1, imageWidth - 1) * (double)remaining);
            chunk.clear();
            if (!reader->read(&chunk, 0, fftSize, offset, true, false)) continue;
            std::fill(fftData.begin(), fftData.end(), 0.f);
            std::copy_n(chunk.getReadPointer(0), fftSize, fftData.begin());
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData.data(), true);
            for (int y = 0; y < imageHeight; ++y)
            {
                const float t = 1.f - (float)y / (float)(imageHeight - 1);
                const float freq = 200.f * std::pow(maxFreq / 200.f, t);
                const int bin = juce::jlimit(1, fftSize / 2 - 1,
                    (int)std::round(freq * (float)fftSize / (float)reader->sampleRate));
                const float amplitude = fftData[(size_t)bin] / (float)fftSize;
                const float db = juce::Decibels::gainToDecibels(juce::jmax(1.0e-8f, amplitude), -100.f);
                const float power = juce::jlimit(0.f, 1.f, (db + 83.f) / 72.f);
                const float red = juce::jlimit(0.f, 1.f, power * 1.8f);
                const float green = juce::jlimit(0.f, 1.f, (power - .28f) * 1.5f);
                const float blue = juce::jlimit(0.f, 1.f, power * .12f);
                img.setPixelAt(x, y, juce::Colour::fromFloatRGBA(red, green, blue, 1.f));
            }
        }
        if (!threadShouldExit())
        {
            const juce::ScopedLock lock(imageGuard);
            rendered = img;
        }
    }

    juce::File source;
    mutable juce::CriticalSection imageGuard; // UI/analysis only; never in processBlock.
    juce::Image rendered;
};
