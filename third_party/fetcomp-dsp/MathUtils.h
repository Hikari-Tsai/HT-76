#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace dsp_math
{
    // ════════════════════════════════════════════════════════════════════════
    //   F A S T   T R A N S C E N D E N T A L S
    // ════════════════════════════════════════════════════════════════════════

    // Bit-exact copy of chowdsp::math_approx::tan<7>
    // Uses tan_mhalfpi_halfpi via half-angle formula
    // ~3-4× faster than std::tan, well-behaved over [-π/2, π/2]
    inline float fastTan(float x) noexcept
    {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float half_pi = pi * 0.5f;
        constexpr float recip_pi = 1.0f / pi;

        // Range reduction: wrap into [-π/2, π/2]
        float shifted = x + half_pi;
        const float trunc = static_cast<float>(static_cast<int>(shifted * recip_pi));
        float mod = shifted - pi * trunc;
        if (shifted < 0.0f) mod += pi;
        const float wrapped = mod - half_pi;

        // Half-angle formula: tan(x) = 2·tan(x/2) / (1 - tan(x/2)²)
        // tan(x/2) computed via 7th-order polynomial on [-π/4, π/4]
        const float h = 0.5f * wrapped;
        const float h_sq = h * h;

        // Order 7 polynomial from chowdsp trig_approx.hpp
        const float x_5_7 = 0.116406244996f + 0.0944480566104f * h_sq;
        const float x_1_3 = 1.0f + 0.335216153138f * h_sq;
        const float x_1_3_5_7 = x_1_3 + x_5_7 * h_sq * h_sq;
        const float h_x = h * x_1_3_5_7;

        return 2.0f * h_x / (1.0f - h_x * h_x);
    }

    // ────────────────────────────────────────────────────────────────────────
    // tanh — bit-exact copy of chowdsp::math_approx::tanh<5>
    // Uses tanh(x) ≈ p(x) / sqrt(p(x)² + 1), where p(x) is a 5th-order
    // odd polynomial fit to minimize maximum relative error.
    // ~3-4× faster than std::tanh, max error ~0.005%, fully smooth.
    // ────────────────────────────────────────────────────────────────────────
    inline float fastTanh(float x) noexcept
    {
        const float x_sq = x * x;
        const float y_3_5 = 0.165326984031f + 0.00970240200826f * x_sq;
        const float y_1_3_5 = 1.0f + y_3_5 * x_sq;
        const float x_poly = x * y_1_3_5;
        return x_poly / std::sqrt(x_poly * x_poly + 1.0f);
    }

    // Double-precision version (for Acid303 ladder filter)
    inline double fastTanh(double x) noexcept
    {
        const double x_sq = x * x;
        const double y_3_5 = 0.165326984031 + 0.00970240200826 * x_sq;
        const double y_1_3_5 = 1.0 + y_3_5 * x_sq;
        const double x_poly = x * y_1_3_5;
        return x_poly / std::sqrt(x_poly * x_poly + 1.0);
    }

    // ────────────────────────────────────────────────────────────────────────
    // sin — bit-exact copy of chowdsp::math_approx::sin<5>
    // Uses Chebyshev polynomial via the (π²-x²)·p(x) product form
    // (https://mooooo.ooo/chebyshev-sine-approximation/) for guaranteed
    // periodicity without discontinuities.
    // ~3-4× faster than std::sin, max error ~10⁻⁴ over full range.
    // ────────────────────────────────────────────────────────────────────────
    inline float fastSin(float x) noexcept
    {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float two_pi = 2.0f * pi;
        constexpr float pi_sq = pi * pi;
        constexpr float recip_two_pi = 1.0f / two_pi;

        // Range reduction: wrap x into [-pi, pi]
        float shifted = x + pi;
        const float trunc = static_cast<float>(static_cast<int>(shifted * recip_two_pi));
        float mod = shifted - two_pi * trunc;
        if (shifted < 0.0f) mod += two_pi;
        const float wrapped = mod - pi;

        // 5th-order Chebyshev sin polynomial (chowdsp trig_approx.hpp)
        const float x_sq = wrapped * wrapped;
        const float x_3_5 = -0.00650096169550f + 0.000139899314103f * x_sq;
        const float x_1_3_5 = 0.101256629587f + x_3_5 * x_sq;
        const float x_poly = wrapped * x_1_3_5;

        return (pi_sq - x_sq) * x_poly;
    }

    // ────────────────────────────────────────────────────────────────────────

    // ────────────────────────────────────────────────────────────────────────
    // exp2 — bit-exact copy of chowdsp::math_approx::exp2<5>
    // Uses 2^x = 2^floor(x) * 2^frac(x), where the integer part is computed
    // via IEEE 754 bit manipulation and the fractional part via 5th-order
    // polynomial.
    // ~3-4× faster than std::exp2, max error ~10⁻⁶.
    // ────────────────────────────────────────────────────────────────────────
    inline float fastExp2(float x) noexcept
    {
        // Clamp to prevent denormals at extreme negative values
        if (x < -126.0f) x = -126.0f;

        const auto xi = static_cast<int32_t>(x);
        const auto l = x < 0.0f ? xi - 1 : xi;
        const float f = x - static_cast<float>(l);
        const int32_t vi = (l + 127) << 23;

        // pow2 polynomial, order 5 (chowdsp pow_approx.hpp)
        const float f_sq = f * f;
        const float x_4_5 = 0.00899009909264f + 0.00187839071291f * f;
        const float x_2_3 = 0.240156326598f + 0.0558229130202f * f;
        const float x_2_3_4_5 = x_2_3 + x_4_5 * f_sq;
        const float x_0_1 = 1.0f + 0.693152270576f * f;
        const float poly = x_0_1 + x_2_3_4_5 * f_sq;

        float two_to_l;
        std::memcpy(&two_to_l, &vi, sizeof(float));
        return two_to_l * poly;
    }

    // ────────────────────────────────────────────────────────────────────────
    // log2 — bit-exact copy of chowdsp::math_approx::log2<3>
    // Uses log2(x) = exponent(x) + log2(1 + mantissa(x))
    // Exponent extracted via IEEE 754 bit manipulation, mantissa via
    // 3rd-order polynomial.
    // ~3-4× faster than std::log2.
    // ────────────────────────────────────────────────────────────────────────
    inline float fastLog2(float x) noexcept
    {
        if (x <= 0.0f) return -126.0f;  // safe fallback

        int32_t vi;
        std::memcpy(&vi, &x, sizeof(float));

        const auto ex = vi & 0x7f800000;
        const auto e = (ex >> 23) - 127;
        const auto vfi = (vi - ex) | 0x3f800000;

        float vf;
        std::memcpy(&vf, &vfi, sizeof(float));

        // log2 order 3 polynomial (chowdsp log_approx.hpp)
        const float x_sq = vf * vf;
        const float x_2_3 = -1.05974531422f + 0.159220010975f * vf;
        const float x_0_1 = -2.16417056258f + 3.06469586582f * vf;
        return static_cast<float>(e) + (x_0_1 + x_2_3 * x_sq);
    }

    inline float fastPow(float x, float y) noexcept
    {
        if (x <= 0.0f) return 0.0f;
        return fastExp2(y * fastLog2(x));
    }

// One-pole DC-blocker feedback coefficient for a given cutoff frequency:
    //     y[n] = x[n] - x[n-1] + R * y[n-1]
    // Pass the rate the blocker actually RUNS at (e.g. the oversampled rate)
    // so the cutoff stays where it was designed, independent of host rate.
    inline float dcBlockerCoeff(float cutoffHz, double sampleRate) noexcept
    {
        return std::exp(-2.0f * 3.14159265f * cutoffHz / (float) sampleRate);
    }

    // ════════════════════════════════════════════════════════════════════════
    //   I N T E R P O L A T I O N   P R I M I T I V E S
    // ════════════════════════════════════════════════════════════════════════

    // Linear interpolation primitive.
    // frac in [0, 1] interpolates between y0 and y1.
    // Cost: 1 sub, 1 mul, 1 add. Cheapest possible.
    inline float lerp(float frac, float y0, float y1) noexcept
    {
        return y0 + frac * (y1 - y0);
    }

    // 4-point cubic Hermite (Catmull-Rom) interpolation.
    // Standard high-quality interpolator for real-time sample playback.
    //
    //   ym1 = sample at integer position posInt - 1
    //   y0  = sample at posInt
    //   y1  = sample at posInt + 1
    //   y2  = sample at posInt + 2
    //   frac = fractional part of read position, in [0, 1]
    //
    // Caller must ensure all four sample reads are valid — use the
    // readBufferHermite() helper below for a bounds-safe wrapper.
    //
    // Cost: ~10 muls + ~10 adds. About 3× the cost of lerp().
    inline float hermite4(float frac, float ym1, float y0, float y1, float y2) noexcept
    {
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 1.5f * (y0 - y1) + 0.5f * (y2 - ym1);
        return ((c3 * frac + c2) * frac + c1) * frac + y0;
    }

    // ════════════════════════════════════════════════════════════════════════
    //   B U F F E R   R E A D E R S   (single-channel, bounds-safe)
    // ════════════════════════════════════════════════════════════════════════
    //
    // Both readers take a raw `const float*` so the caller can fetch the
    // channel pointer once outside the audio loop:
    //
    //     const float* ch0 = buffer.getReadPointer(0);
    //     for (...) {
    //         float s = dsp_math::readBufferHermite(ch0, numSamples, pos);
    //     }
    //
    // For multi-channel buffers, call once per channel.
    //
    // Out-of-range reads are clamped to the buffer edges. NaN/Inf read
    // positions, null pointers, and empty buffers all return 0.0f safely.

    // Linear-interpolation buffer read.
    // Use for drums, percussion, lo-fi character — same quality tier as
    // SP-1200 / MPC60-style drum machines.
    inline float readBufferLinear(const float* data, int numSamples, float readPos) noexcept
    {
        if (data == nullptr || numSamples <= 0) return 0.0f;
        if (!std::isfinite(readPos)) return 0.0f;
        if (numSamples == 1) return data[0];

        // std::floor handles negative readPos correctly (unlike int truncation).
        const int posFloor = static_cast<int>(std::floor(readPos));
        const float frac = readPos - static_cast<float>(posFloor);

        const int last = numSamples - 1;
        const int i0 = (posFloor < 0) ? 0 : (posFloor > last) ? last : posFloor;
        const int i1 = (posFloor + 1 < 0) ? 0 : (posFloor + 1 > last) ? last : posFloor + 1;

        return data[i0] + frac * (data[i1] - data[i0]);
    }

    // 4-point Hermite buffer read.
    // Use for sliced loops, REX files, melodic samples, anything pitched far
    // from native rate. Quality tier comparable to Akai S1000/S1100,
    // Renoise "Cubic", Ableton Sampler "Best".
    //
    // Fast path: zero clamping when read position is safely inside the buffer.
    // Slow path: edge clamping near boundaries (first/last few samples).
    inline float readBufferHermite(const float* data, int numSamples, float readPos) noexcept
    {
        if (data == nullptr || numSamples <= 0) return 0.0f;
        if (!std::isfinite(readPos)) return 0.0f;
        if (numSamples == 1) return data[0];

        const int posFloor = static_cast<int>(std::floor(readPos));
        const float frac = readPos - static_cast<float>(posFloor);

        // FAST PATH: deep inside the buffer, no clamping needed.
        // Requires posFloor - 1 >= 0  AND  posFloor + 2 <= numSamples - 1.
        if (posFloor >= 1 && posFloor + 2 < numSamples)
        {
            return hermite4(frac,
                            data[posFloor - 1],
                            data[posFloor],
                            data[posFloor + 1],
                            data[posFloor + 2]);
        }

        // SLOW PATH: at or beyond the edges. Clamp every index independently.
        const int last = numSamples - 1;
        auto clampIdx = [last](int i) noexcept {
            return (i < 0) ? 0 : (i > last) ? last : i;
        };

        const float ym1 = data[clampIdx(posFloor - 1)];
        const float y0  = data[clampIdx(posFloor)];
        const float y1  = data[clampIdx(posFloor + 1)];
        const float y2  = data[clampIdx(posFloor + 2)];

        return hermite4(frac, ym1, y0, y1, y2);
    }
    // ════════════════════════════════════════════════════════════════════════
    //   A N T I D E R I V A T I V E   A N T I A L I A S I N G   (A D A A)
    // ════════════════════════════════════════════════════════════════════════
    //
    // ADAA antialiases a memoryless nonlinearity f by averaging it over the
    // segment between consecutive inputs using the exact antiderivative(s) of f,
    // instead of point-sampling f (which folds the shaper's new harmonics back
    // below Nyquist). Correctness REQUIRES F to be the exact integral of the
    // exact f used on the fallback path — see lnCosh below for the tanh pairing.

    // ln(cosh x) — the exact antiderivative of tanh(x). Overflow-safe form:
    //     ln(cosh x) = |x| + ln(1 + e^{-2|x|}) - ln 2
    // The e^{} argument is in (0, 1], so std::log1p stays well-conditioned.
    // Evaluated once per sample per stage (F is cached inside adaa1), so the
    // std:: calls here are acceptable; swap in a fitted approximation only if
    // profiling demands it (accuracy of F is load-bearing for ADAA correctness).
    inline float lnCosh(float x) noexcept
    {
        const float ax = std::abs(x);
        return ax + std::log1p(std::exp(-2.0f * ax)) - 0.6931471805599453f;
    }

    // First-order ADAA of a memoryless nonlinearity.
    //   x0 : current input
    //   x1 : PREVIOUS input (caller-owned state, updated in place)
    //   F1 : cached F(x1)     (caller-owned state, updated in place)
    //   f  : the shaper       (called only on the ill-conditioned fallback path)
    //   F  : exact antiderivative of f
    // Falls back to the midpoint value f((x0+x1)/2) when the two inputs are too
    // close for a stable divided difference. No allocation, RT-safe.
    template <typename Fval, typename Fint>
    inline float adaa1(float x0, float& x1, float& F1,
                       Fval f, Fint F, float eps = 1.0e-5f) noexcept
    {
        const float F0 = F(x0);
        const float dx = x0 - x1;
        const float y  = (std::abs(dx) < eps) ? f(0.5f * (x0 + x1))
                                              : (F0 - F1) / dx;
        x1 = x0;
        F1 = F0;
        return y;
    }

    // Second-order ADAA (Parker/Esqueda/Välimäki form) — lower alias floor than
    // adaa1 for stiff shapers (hard clip, cubic soft-clip, tube LUT), at the
    // cost of a two-sample history. Caller-owned state: x1, x2 (input history),
    // F2x1 (cached second antiderivative of x1), d1 (previous 1st divided diff).
    //   F1 : first antiderivative of f   (fallback paths only)
    //   F2 : second antiderivative of f  (main path)
    // Uses the standard nested ill-conditioned fallbacks.
    template <typename Fval, typename Fint1, typename Fint2>
    inline float adaa2(float x0, float& x1, float& x2, float& F2x1, float& d1,
                       Fval f, Fint1 F1, Fint2 F2, float eps = 1.0e-5f) noexcept
    {
        const float F2_0 = F2(x0);

        float d0;
        if (std::abs(x0 - x1) < eps)
            d0 = F1(0.5f * (x0 + x1));
        else
            d0 = (F2_0 - F2x1) / (x0 - x1);

        float y;
        if (std::abs(x0 - x2) < eps)
        {
            // Ill-conditioned outer difference: fall back through the midpoint.
            const float xbar  = 0.5f * (x0 + x2);
            const float delta = xbar - x1;
            if (std::abs(delta) < eps)
                y = f(0.5f * (xbar + x1));
            else
                y = (2.0f / delta) * (F1(xbar) + (F2x1 - F2(xbar)) / delta);
        }
        else
        {
            y = (2.0f / (x0 - x2)) * (d0 - d1);
        }

        d1   = d0;
        x2   = x1;
        x1   = x0;
        F2x1 = F2_0;
        return y;
    }

    // Dynamically choose oversampling factor to target ~350-400 kHz effective
    // internal rate across all host sample rates.
    //   44.1/48 kHz   → factor 3 (8x)  → 352.8/384 kHz internal
    //   88.2/96 kHz   → factor 2 (4x)  → 352.8/384 kHz internal
    //   176.4/192 kHz → factor 1 (2x)  → 352.8/384 kHz internal
    inline int chooseOversamplingFactor(double hostSampleRate) noexcept
    {
        if (hostSampleRate >= 144000.0) return 1;
        if (hostSampleRate >= 72000.0)  return 2;
        return 3;
    }
}
