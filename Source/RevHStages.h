// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cmath>

namespace field
{
// Experimental G/H-topology input/output stages. These are reduced behavioural
// models, NOT a transistor netlist or a measured B11148 transformer model.
// Calibration assumptions and manufacturer sources: docs/REV_H_MODEL.md.
class RevHStages
{
public:
    void setSampleRate (double rate) noexcept
    {
        if (rate == sampleRate) return;
        sampleRate = std::max (rate, 8000.0);
        const double pi = 3.14159265358979323846;
        const auto inputG = std::tan (pi * 2.0 / sampleRate);
        hp = inputG / (1.0 + inputG);
        inputPole = 1.0 - std::exp (-2.0 * pi * 80000.0 / sampleRate);
        outputPole = 1.0 - std::exp (-2.0 * pi * 40000.0 / sampleRate);
        fluxStep = 2.0 * pi * 50.0 / sampleRate;
        slewStep = 500000.0 / sampleRate; // Normalized units/sec; provisional.
    }
    void reset() noexcept { inputZ = receiver = flux = outputLP = 0.0; }

    float input (float x) noexcept
    {
        // Each DAW channel already represents a differential audio signal.
        // Never subtract L from R to simulate a balanced physical connector.
        const double ac = highPass (std::isfinite (x) ? double (x) : 0.0);
        const double desired = 8.0 * std::tanh (ac / 8.0);
        receiver += std::clamp ((desired - receiver) * inputPole, -slewStep, slewStep);
        return float (receiver);
    }

    // Symmetric push-pull transfer with a small smooth crossover region and
    // negative feedback. Solve y=f(A*x-beta*y), with A chosen for unity DC slope.
    static double pushPull (double x) noexcept
    {
        constexpr double rail = 4.0, feedback = 8.0, width = 0.04, crossover = 0.012;
        constexpr double smallSlope = 1.0 - crossover / width;
        const double drive = (feedback + 1.0 / smallSlope) * x;
        double lo = -rail, hi = rail, y = std::clamp (x, lo, hi);
        for (int n = 0; n < 10; ++n)
        {
            const double u = drive - feedback * y;
            const double t = std::tanh (u / width);
            const double f = std::tanh ((u - crossover * t) / rail);
            const double residual = y - rail * f;
            if (std::abs (residual) < 1.0e-8) break;
            if (residual > 0.0) hi = y; else lo = y;
            const double slope = (1.0 - f * f) * (1.0 - crossover / width * (1.0 - t * t));
            const double next = y - residual / (1.0 + feedback * slope);
            y = next > lo && next < hi ? next : 0.5 * (lo + hi);
        }
        return y;
    }

    float output (float x) noexcept
    {
        const double driven = pushPull (std::clamp (double (x), -640.0, 640.0));
        // Passive saturable magnetizing branch: v = x - R*i(phi), dphi/dt = v.
        // i(phi)=phi+s*phi^3 is monotonic. Backward Euler makes the flux solve
        // bounded under DC/overload. No borrowed Rev-D magnetic asymmetry/trim.
        constexpr double sourceLoss = 0.04, saturation = 0.12;
        const double target = flux + fluxStep * driven;
        double nextFlux = flux;
        for (int n = 0; n < 6; ++n)
        {
            const double square = nextFlux * nextFlux;
            const double residual = nextFlux + fluxStep * sourceLoss * nextFlux * (1.0 + saturation * square) - target;
            nextFlux -= residual / (1.0 + fluxStep * sourceLoss * (1.0 + 3.0 * saturation * square));
        }
        flux = std::isfinite (nextFlux) ? nextFlux : 0.0;
        const double winding = driven - sourceLoss * flux * (1.0 + saturation * flux * flux);
        outputLP += outputPole * (winding - outputLP);
        // Preserve the existing plugin's nominal output reference (not a
        // measured transformer turns ratio) so D/H comparison is practical.
        return float (1.95 * outputLP);
    }
private:
    double highPass (double x) noexcept
    {
        const double v = (x - inputZ) * hp;
        const double low = inputZ + v;
        inputZ = low + v;
        return x - low;
    }
    double sampleRate = 0.0, hp = 0.0, inputPole = 1.0, outputPole = 1.0;
    double fluxStep = 0.0, slewStep = 1.0;
    double inputZ = 0.0, receiver = 0.0, flux = 0.0, outputLP = 0.0;
};
}
