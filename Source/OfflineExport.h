#pragma once
#include "PluginProcessor.h"
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
               const juce::MemoryBlock& processorState, int sampleBits = 24)
    {
        if (isThreadRunning() || !input.existsAsFile() || input == destination)
            return false;
        if (sampleBits != 16 && sampleBits != 24)
            return false;
        in = input;
        out = destination;
        stateData = processorState;
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
        if (out.existsAsFile() && !out.deleteFile()) {
            status.store(Status::Failed); return;
        }
        auto stream = out.createOutputStream();
        if (!stream || !stream->openedOk()) {
            status.store(Status::Failed); return;
        }
        juce::WavAudioFormat wav;
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wav.createWriterFor(stream.get(), reader->sampleRate,
                                reader->numChannels, bitDepth, {}, 0));
        if (!writer) { status.store(Status::Failed); return; }
        stream.release();  // Writer now owns the output stream.
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
            if (!writer->writeFromAudioSampleBuffer(segment, 0, count)) { ok = false; break; }
            offset += count;
        }
        writer.reset();
        engine.releaseResources();
        if (!ok || threadShouldExit())
        {
            out.deleteFile();
            status.store(Status::Failed);
        }
        else status.store(Status::Complete);
    }
    juce::File in, out;
    juce::MemoryBlock stateData;
    int bitDepth = 24;
    std::atomic<Status> status{Status::Idle};
};
