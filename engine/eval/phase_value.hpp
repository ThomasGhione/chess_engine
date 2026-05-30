#pragma once

#include <cstdint>

namespace engine {

// Phase-dependent eval contribution. `mg` is the middlegame component
// (also the opening side of the curve), `eg` is the endgame component.
// A scalar phase weight w in [0, 1] (1 = opening/full material, 0 = bare
// endgame) blends the pair into a single score via blend().
struct PhaseValue {
    int32_t mg = 0;
    int32_t eg = 0;

    constexpr PhaseValue() = default;
    constexpr PhaseValue(int32_t m, int32_t e) noexcept : mg(m), eg(e) {}

    // Single-value constructor: same contribution in MG and EG. Lets existing
    // phase-neutral terms convert implicitly.
    constexpr PhaseValue(int32_t v) noexcept : mg(v), eg(v) {}

    constexpr PhaseValue operator+(const PhaseValue& o) const noexcept {
        return {mg + o.mg, eg + o.eg};
    }
    constexpr PhaseValue operator-(const PhaseValue& o) const noexcept {
        return {mg - o.mg, eg - o.eg};
    }
    constexpr PhaseValue operator-() const noexcept { return {-mg, -eg}; }

    constexpr PhaseValue& operator+=(const PhaseValue& o) noexcept {
        mg += o.mg; eg += o.eg; return *this;
    }
    constexpr PhaseValue& operator-=(const PhaseValue& o) noexcept {
        mg -= o.mg; eg -= o.eg; return *this;
    }

    constexpr PhaseValue operator*(int32_t k) const noexcept {
        return {mg * k, eg * k};
    }

    // Blend with phase weight w (1.0 = pure MG, 0.0 = pure EG).
    // Rounded to nearest int32 via fixed-point: scales by 1024 to avoid float.
    constexpr int32_t blend(int32_t wScaled1024) const noexcept {
        // wScaled1024 in [0, 1024]. Result = mg*w + eg*(1-w).
        return (mg * wScaled1024 + eg * (1024 - wScaled1024) + 512) >> 10;
    }
};

constexpr PhaseValue operator*(int32_t k, const PhaseValue& pv) noexcept {
    return pv * k;
}

} // namespace engine
