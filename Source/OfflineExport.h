#pragma once
#include "PluginProcessor.h"
#include "RegionManager.h"
#include <atomic>
#include <memory>

// Batch WAV exporter. A separate processor instance processes the audio in a worker
// so the host audio callback and editor are never blocked by disk I/O.
class LevelOfflineExport : private juce::Thread
{
public:
    enum class Status { Idle, Working, Complete, Failed };
    LevelOfflineExport() : juce::Thread("LEVEL Offline Bounce") {}
    ~LevelOfflineExport() override { stopThread(-1); }
    bool begin(const juce::File& input, const juce::File& destination,
               const juce::MemoryBlock& processorState,
               std::vector<level::AudioRegion> manualRegions, int sampleBits = 24)
    {
        if (isThreadRunning() || !input.existsAsFile() || input == destination)
            return false;
        if (sampleBits != 16 && sampleBits != 24 && sampleBits != 32)
            return false;
        in = input;
        out = destination;
        stateData = processorState;
        regionEdits = std::move(manualRegions);
        bitDepth = sampleBits;
        status.store(Status::Working);
        startThread();
        return true;
    }
    Status currentStatus() const noexcept { return status.load(); }

private:
    void run() override
    {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(in));
        if (!reader || reader->lengthInSamples < 0 || reader->sampleRate <= 0.0 ||
            reader->numChannels < 1 || reader->numChannels > 2)
        {
            status.store(Status::Failed);
            return;
        }

        // Never modify the original; an output file must be a separate path.
        juce::TemporaryFile temporary(out);
        auto fileStream = temporary.getFile().createOutputStream();
        if (!fileStream || !fileStream->openedOk()) {
            status.store(Status::Failed); return;
        }
        std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);
        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriter::Options{}
            .withSampleRate(reader->sampleRate)
            .withNumChannels((int)reader->numChannels)
            .withBitsPerSample(bitDepth)
            .withSampleFormat(bitDepth == 32
                ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                : juce::AudioFormatWriterOptions::SampleFormat::integral);
        auto writer = wav.createWriterFor(stream, options);
        if (!writer) { status.store(Status::Failed); return; }
        // Successful creation transfers ownership of stream to the writer.
        constexpr int blockSize = 2048;
        juce::AudioBuffer<float> buffer((int)reader->numChannels, blockSize);
        juce::MidiBuffer midi;
        LevelProcessor engine;
        engine.setStateInformation(stateData.getData(), (int)stateData.getSize());
        engine.prepareToPlay(reader->sampleRate, blockSize);
        juce::int64 offset = 0;
        bool ok = true;
        while (offset < reader->lengthInSamples && !threadShouldExit())
        {
            const int count = (int)juce::jmin((juce::int64)blockSize, reader->lengthInSamples - offset);
            buffer.clear();
            if (!reader->read(&buffer, 0, count, offset, true, true)) { ok = false; break; }
            // Restrict the buffer to valid samples, especially for the final block.
            juce::AudioBuffer<float> segment(buffer.getArrayOfWritePointers(), (int)reader->numChannels, count);
            engine.processBlock(segment, midi);
            // Manually selected regions are rendered non-destructively. Soft 15ms
            // boundaries reduce clicks. No modifications are made to source audio.
            for (const auto& region : regionEdits)
            {
                if (!region.enabled) continue;
                const auto first = (juce::int64)(region.startSeconds * reader->sampleRate);
                const auto last = (juce::int64)(region.endSeconds * reader->sampleRate);
                if (last <= offset || first >= offset + count) continue;
                const int start = (int)juce::jmax((juce::int64)0, first - offset);
                const int stop = (int)juce::jmin((juce::int64)count, last - offset);
                const float targetGain = juce::Decibels::decibelsToGain(region.gainDb);
                const double fadeSamples = reader->sampleRate * 0.015;
                for (int i = start; i < stop; ++i)
                {
                    const double position = (double)(offset + i);
                    const float fadeIn = (float)juce::jlimit(0.0, 1.0, (position - (double)first) / fadeSamples);
                    const float fadeOut = (float)juce::jlimit(0.0, 1.0, ((double)last - position) / fadeSamples);
                    const float blend = juce::jmin(fadeIn, fadeOut);
                    const float gain = 1.f + (targetGain - 1.f) * blend;
                    for (int channel = 0; channel < segment.getNumChannels(); ++channel)
                        segment.getWritePointer(channel)[i] *= gain;
                }
            }
            if (!writer->writeFromAudioSampleBuffer(segment, 0, count)) { ok = false; break; }
            offset += count;
        }
        writer.reset();
        engine.releaseResources();
        if (!ok || threadShouldExit())
        {
            status.store(Status::Failed);
        }
        else
        {
            status.store(temporary.overwriteTargetFileWithTemporary() ? Status::Complete : Status::Failed);
        }
    }
    juce::File in, out;
    juce::MemoryBlock stateData;
    std::vector<level::AudioRegion> regionEdits;
    int bitDepth = 24;
    std::atomic<Status> status{Status::Idle};
};
