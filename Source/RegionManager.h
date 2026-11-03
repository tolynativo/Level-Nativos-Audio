#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace level {
enum class RegionType : std::uint8_t {
    Sibilance, Breath, Plosive, MouthNoise, Pause
};

struct AudioRegion {
    std::uint64_t id = 0;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    RegionType type = RegionType::Breath;
    float confidence = 1.0f;
    float gainDb = -6.0f;
    bool enabled = true;
    bool userEdited = false;
};

// UI/analysis-thread owned only; never mutate or iterate this vector in processBlock.
// A future real-time engine must receive precomputed immutable region snapshots.
class RegionManager {
public:
    const std::vector<AudioRegion>& all() const noexcept { return regions; }

    std::uint64_t add(double start, double end, RegionType type, float confidence = 1.f) {
        if (!(start >= 0.0) || !(end > start) || !(confidence >= 0.f && confidence <= 1.f))
            return 0;
        AudioRegion r;
        r.id = nextId++;
        r.startSeconds = start;
        r.endSeconds = end;
        r.type = type;
        r.confidence = confidence;
        regions.push_back(r);
        sort();
        return r.id;
    }
    bool erase(std::uint64_t id) {
        auto it = std::find_if(regions.begin(), regions.end(), [id](const auto& r) { return r.id == id; });
        if (it == regions.end()) return false;
        regions.erase(it);
        return true;
    }
    bool resize(std::uint64_t id, double start, double end) {
        if (!(start >= 0.0) || !(end > start)) return false;
        for (auto& r : regions) if (r.id == id) {
            r.startSeconds = start; r.endSeconds = end; r.userEdited = true; sort(); return true;
        }
        return false;
    }
    bool setGain(std::uint64_t id, float gainDb) {
        if (!(gainDb >= -72.f && gainDb <= 24.f)) return false;
        for (auto& r : regions) if (r.id == id) {
            r.gainDb = gainDb; r.userEdited = true; return true;
        }
        return false;
    }
    bool setEnabled(std::uint64_t id, bool enabled) {
        for (auto& r : regions) if (r.id == id) {
            r.enabled = enabled; r.userEdited = true; return true;
        }
        return false;
    }
    void clear() noexcept { regions.clear(); }
private:
    void sort() {
        std::stable_sort(regions.begin(), regions.end(), [](const auto& a, const auto& b) {
            return a.startSeconds < b.startSeconds;
        });
    }
    std::uint64_t nextId = 1;
    std::vector<AudioRegion> regions;
};
} // namespace level
