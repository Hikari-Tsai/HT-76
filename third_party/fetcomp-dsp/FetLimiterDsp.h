/*
  ==============================================================================

    FetLimiterDsp.h

    A circuit-derived model of the classic FET limiting amplifier gain cell.
    This is the DSP behind FetComp. See README.md for the topology, and for the
    list of approximations it makes and why.

    It replaced an earlier conventional compressor that smoothed gain reduction
    in the dB domain. That one measured 0.0000% gain-modulation THD, which is
    also why it had no character: C1-continuous applied gain cannot produce the
    ragged, level-dependent gain movement that makes a FET limiter sound like
    one. The two goals are mutually exclusive in a single signal path, so this
    is built the other way round.

    WHAT IS TAKEN FROM THE CIRCUIT (Rev D drawing, UA schematic R-10743)

      - The gain cell is a voltage divider: a 27k series resistor into a shunt
        JFET. Nothing else. Gain is R_ds/(R5 + R_ds).
      - Feedback sensing: the sidechain is tapped AFTER gain reduction, so the
        loop is closed round the gain cell. This is the structural reason the
        unit "grabs" rather than "clamps".
      - The LN linearisation network feeds a fraction of the drain voltage back
        to the gate. See the algebra below - this is the entire distortion
        mechanism, and it is inseparable from the compression.
      - Q1 is marked SELECTED on the drawing. There is no canonical FET here;
        two original units differ. Idss/Vp are therefore exposed as parameters
        rather than pretended to be constants.

    WHAT IS CALIBRATED TO PUBLISHED BEHAVIOUR RATHER THAN COMPONENT VALUES

      The ratio bank's 1% resistors are not legible on any scan I could find, so
      the four ratio positions are fitted to the documented behaviour instead:
      each position sets a sidechain slope AND raises the threshold, which the
      manufacturer's own manual states explicitly. Attack 20us-800us and release
      50ms-1.1s are from the published specification. Both knobs run BACKWARDS
      on the hardware; that belongs in the UI, not here.

    THE GAIN CELL, DERIVED

      JFET in the ohmic region:

          I_d = b[2(Vgs - Vp)Vds - Vds^2],    b = Idss/Vp^2

      The LN network makes the gate follow the drain: Vgs = Vg0 + a*Vds.
      Substituting:

          I_d = b[2(Vg0 - Vp)Vds + (2a - 1)Vds^2]

      so the channel conductance is affine in the drain voltage:

          g(v) = G0 + k*v,    G0 = 2b(Vg0 - Vp),   k = b(2a - 1)

      G0 is what the sidechain controls. k is the distortion, and note that it
      vanishes at a = 0.5 - that is precisely what the LN modification does, and
      why Rev C onward is quieter and cleaner than Rev A. Expose a and you have
      a physically meaningful revision control instead of a waveshaper.

      Solving the divider node (x -> R5 -> v, FET from v to ground):

          (x - v)/R5 = v*g(v)   =>   R5*k*v^2 + (1 + R5*G0)*v - x = 0

      One quadratic per sample, closed form, no iteration. Compression and
      distortion fall out of the same equation because in the real circuit they
      are the same event.

    Oversampled, because unlike the old design this one genuinely needs it: the
    gain cell is a static nonlinearity and the 20us attack puts control-signal
    energy near Nyquist.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MathUtils.h"
#include <cmath>

// HT-76 local integration: optional Apache-2.0 Rev H behavioural stages.
#include "RevHStages.h"

class FetLimiterDsp
{
public:
    //==========================================================================
    // Ratio positions. The hardware selects these with four latching buttons,
    // and pressing all four at once shorts the bank into a combination the
    // designer never intended - hence allButtons being a separate flag rather
    // than a fifth enum value.
    enum class Ratio { four, eight, twelve, twenty };

    struct Parameters
    {
        // The hardware has no threshold control. You drive INTO a fixed
        // threshold with the input knob, which is why input level, gain
        // reduction and distortion are inseparable on the real unit. Keeping
        // that coupling is most of the point of this model, so there is
        // deliberately no thresholdDb here. Do not add one.
        float inputDb   = 0.0f;    // -20 .. +40
        float outputDb  = 0.0f;    // -20 .. +20

        Ratio ratio      = Ratio::four;
        bool  allButtons = false;

        // Published ranges. Fast end is clockwise on the hardware.
        float attackUs  = 400.0f;   // 20 .. 800
        float releaseMs = 400.0f;   // 50 .. 1100

        // The Attack control has an OFF detent past its slow end. There, per
        // the manual, "signal is passing through the 1176LN circuitry but with
        // a compression ratio of 1:1, thus adding colour, but with no gain
        // reduction" - the cell sits open and you get the line amp and the
        // transformer on their own. Using the unit as a colour box is a real
        // technique, and it is also the only way to reach the condition the
        // published THD figure is measured under, since a feedback design that
        // is always compressing always ripples on bass.
        bool limiting = true;

        // Gate-feedback fraction. 0.5 cancels the square-law term exactly (LN,
        // Rev C+). 0.0 is Rev A with the full second harmonic. Real units sit
        // near 0.5 but never on it - resistor tolerance and the Q-bias trimmer
        // leave a residual, and that residual is audible.
        float linearisation = 0.47f; // real units sit NEAR 0.5, never on it:
                                     // 0.5 exactly cancels the cell's 2nd
                                     // harmonic, which no hardware unit does.
                                     // 0.47 = plausible matched-pair residual;
                                     // buys ~2 dB of the measured h2 deficit,
                                     // the rest lives in the iron (TxCoreAsym,
                                     // next calibration pass).  // 0 .. 0.5

        // Selected-part latitude. Defaults are typical small-signal n-JFET.
        float idss = 0.010f;        // A
        float pinchOffV = -3.0f;    // V, negative

        // How hard the output section is driven. 1.0 is the unit as built;
        // 0.0 takes the Class A stage and the transformer core out of the
        // picture entirely, leaving the gain cell on its own.
        //
        // This is a DRIVE control, not a crossfade. Both stages are normalised
        // to unity slope at the origin, so winding their drive to zero leaves
        // each one mathematically transparent - the Class A tanh degenerates to
        // its own tangent, and with no flux the core sits at its small-signal
        // permeability and never leaves it. No dry/wet mixing, no phase
        // cancellation between two paths, and every value in between is a real
        // operating point rather than a blend of two.
        //
        // Worth having for its own sake, and necessary for any A/B: it is the
        // only way to tell whether a difference lives in the gain cell or in
        // the iron.
        float ironAmount = 1.0f;    // 0 .. 1

        bool revisionH = false; // HT-76 experimental G/H input/output topology.

        float mix = 1.0f;           // parallel blend, 0 = dry

        // 0 = v1 iron (ships today). 1 = physics-corrected iron for the A/B:
        // stage gain capped at unity (a passive transformer can RECOVER the
        // LF its finite inductance loses, never exceed it - the +5.5 dB LF
        // gain TxProbe measured at +20 out is v1's un-capped permeability
        // rise), stronger low-level hysteresis (the missing half of the
        // U-shaped THD-vs-level curve), and a working leakage rolloff (v1's
        // coefficient clamps to 1 = inert). Same J-A core, same state.
        int txModel = 1;    // 1 = the physics-correct iron IS the product
                            // (verdict Aug 2026); 0 = v1 kept for bench archaeology

        // 0 = the single-cap release law that shipped as "finally stable";
        // 1 = the network derived from the RelLawProbe capture (two-node
        // timing + mid-anchored pot taper). Exposed for the ear A/B.
        int relModel = 1;
    };

    FetLimiterDsp() = default;

    //==========================================================================
    void prepare (double newSampleRate, int blockSize, int numChannels = 2)
    {
        sampleRate    = newSampleRate;
        channels      = juce::jlimit (1, MaxChannels, numChannels);
        preparedBlock = juce::jmax (1, blockSize);
        dryBuffer.setSize (channels, preparedBlock);

        // All three engines are built here so the factor can be switched from
        // the audio thread later without allocating. 1x is a JUCE dummy stage
        // (zero latency, pass-through).
        for (int f = 0; f < 3; ++f)
        {
            // useIntegerLatency = true: without it JUCE leaves the group
            // delay fractional (2x = 3.137 samples) and getLatencySamples()
            // ceils, so host PDC is sub-sample wrong and null tests can
            // never close (measured on the Windows bench, 2026-08-07).
            oversamplers[f] = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, f,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true, true);
            oversamplers[f]->initProcessing ((size_t) juce::jmax (1, blockSize));
        }

        osRate = sampleRate * (double) (1 << osFactorLog2);

        // Smoothed at the oversampled rate, since that is where they are read.
        gainsPrimed = false;
        smoothedInput .reset (osRate, GainSmoothingSeconds);
        smoothedOutput.reset (osRate, GainSmoothingSeconds);
        smoothedInput .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputDb));
        smoothedOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));

        updateCoefficients();
        reset();
    }

    // Oversampling is not free of delay. Report this to the host, or every
    // track carrying one drifts against the rest of the kit.
    int getLatencySamples() const noexcept
    {
        auto* os = oversamplers[osFactorLog2].get();
        return (os != nullptr) ? (int) std::ceil (os->getLatencyInSamples()) : 0;
    }

    // 0 = 1x (off), 1 = 2x, 2 = 4x. Every timing coefficient in the model is
    // expressed at the oversampled rate, so a factor change recomputes them
    // all; nothing here allocates, so this is safe per-block from processBlock.
    void setOversamplingLog2 (int newFactorLog2)
    {
        const int f = juce::jlimit (0, 2, newFactorLog2);
        if (f == osFactorLog2)
            return;

        osFactorLog2 = f;
        osRate = sampleRate * (double) (1 << osFactorLog2);

        smoothedInput .reset (osRate, GainSmoothingSeconds);
        smoothedOutput.reset (osRate, GainSmoothingSeconds);
        smoothedInput .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputDb));
        smoothedOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));

        updateCoefficients();

        if (auto* os = oversamplers[osFactorLog2].get())
            os->reset();
    }

    int getOversamplingLog2() const noexcept { return osFactorLog2; }

    void reset()
    {
        for (auto& s : state)
        {
            s.env       = 0.0f;
            s.envRes    = 0.0f;
            s.biasShift = 0.0f;
            s.ctrlVolts = 0.0f;
            s.lastCell  = 0.0f;
        }

        for (auto& h : hpState)
            h.z = 0.0f;

        for (auto& t : txState)
            t = TxState {};

        for (auto& stage : revHStages) stage.reset();

        peakGrDb = 0.0f;
        // A transport reset must not retain a partially completed knob ramp.
        // Re-prime on the next parameter set just as after prepare().
        gainsPrimed = false;
        smoothedInput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputDb));
        smoothedOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));

        for (auto& os : oversamplers)
            if (os != nullptr)
                os->reset();
    }

    //==========================================================================
    void setParameters (const Parameters& p)
    {
        const bool timingChanged = (p.attackUs  != params.attackUs)
                                || (p.releaseMs != params.releaseMs);

        const bool cellChanged = (p.linearisation != params.linearisation)
                              || (p.idss         != params.idss)
                              || (p.pinchOffV    != params.pinchOffV);

        const bool ratioChanged = (p.ratio      != params.ratio)
                               || (p.allButtons != params.allButtons);

        const bool txModelChanged  = (p.txModel  != params.txModel);
        const bool relModelChanged = (p.relModel != params.relModel);
        const bool ironChanged  = (p.ironAmount != params.ironAmount)
                               || txModelChanged;

        params = p;

        params.attackUs      = juce::jlimit (20.0f, 800.0f,  params.attackUs);
        params.releaseMs     = juce::jlimit (50.0f, 1100.0f, params.releaseMs);
        params.linearisation = juce::jlimit (0.0f, 0.5f,     params.linearisation);
        params.pinchOffV     = juce::jmin  (-0.5f,           params.pinchOffV);
        params.idss          = juce::jmax  (1.0e-4f,         params.idss);
        params.mix           = juce::jlimit (0.0f, 1.0f,     params.mix);
        params.ironAmount    = juce::jlimit (0.0f, 1.0f,     params.ironAmount);

        const float inG  = juce::Decibels::decibelsToGain (params.inputDb);
        const float outG = juce::Decibels::decibelsToGain (params.outputDb);

        // Ramp knob moves, but SNAP the first set after prepare. Otherwise
        // loading a preset glides up to its input gain over 20 ms, which is
        // both wrong and quietly poisonous to measurement - it made every
        // attack setting read 1161 us, i.e. the ramp, not the attack.
        if (gainsPrimed)
        {
            smoothedInput .setTargetValue (inG);
            smoothedOutput.setTargetValue (outG);
        }
        else
        {
            smoothedInput .setCurrentAndTargetValue (inG);
            smoothedOutput.setCurrentAndTargetValue (outG);
            gainsPrimed = true;
        }

        if (timingChanged || cellChanged || ratioChanged || ironChanged || relModelChanged)
            updateCoefficients();

        // Entering the network with an empty reservoir would release fast
        // for a moment; level it with the fast node instead.
        if (relModelChanged)
            for (auto& st : state)
                st.envRes = st.env;

        // The two iron models run the same core at flux scales 111x apart
        // (TxFluxScaleB / TxFluxScale). Carrying H across a switch unscaled
        // left the incoming mode at a crushed operating point - heard as one
        // channel dropping out after an A/B flip. A hard reset is no better:
        // the J-A loop has remanence, so a demagnetised restart settles to a
        // switch-phase-dependent level (measured +-1.4 dB). RESCALE instead:
        // same normalised flux, both modes continuous through the switch.
        if (txModelChanged)
        {
            const float f = (params.txModel == 1)
                              ? TxFluxScaleB / (float) TxFluxScale
                              : (float) TxFluxScale / TxFluxScaleB;
            for (auto& t : txState)
            {
                t.H = t.H * f;
                t.M = juce::jlimit (-4.0f, 4.0f, t.M * f);
            }
        }
    }

    // Peak gain reduction over the last block, in dB (negative). For metering.
    float getGainReductionDb() const noexcept { return peakGrDb; }

    //==========================================================================
    // Panel calibration shared by every surface that exposes this DSP (the
    // FetComp plugin, BeatForge's mixer inserts). Maps the -60..0 faceplate
    // knob values to internal drive via the measured INCAL/OUTCAL tables
    // (VersusBench, bisected against the UADx). One copy so the surfaces
    // cannot drift apart. Between points: linear. Above the top point:
    // clamp. Below the bottom measured point: continue the last slope. At
    // the counterclockwise stop: -inf - they are attenuators.
    struct PanelCal
    {
        static float mapInput (float knobDb) noexcept
        {
            static constexpr float kp[17] = { -55.0f, -50.0f, -45.0f, -40.0f, -36.0f, -33.0f,
                                              -30.0f, -27.0f, -24.0f, -21.0f, -18.0f, -15.0f,
                                              -12.0f, -9.0f, -6.0f, -3.0f, 0.0f };
            static constexpr float dp[17] = { -21.71f, -19.86f, -17.72f, -14.05f, -9.03f, -6.60f,
                                              -4.29f, 1.76f, 5.53f, 9.40f, 10.90f, 12.49f,
                                              14.37f, 18.49f, 18.79f, 18.84f, 18.86f };
            return interp (knobDb, kp, dp);
        }

        static float mapOutput (float knobDb) noexcept
        {
            static constexpr float kp[17] = { -55.0f, -50.0f, -45.0f, -40.0f, -36.0f, -33.0f,
                                              -30.0f, -27.0f, -24.0f, -21.0f, -18.0f, -15.0f,
                                              -12.0f, -9.0f, -6.0f, -3.0f, 0.0f };
            static constexpr float dp[17] = { -73.97f, -55.81f, -42.31f, -34.80f, -29.01f, -20.52f,
                                              -13.77f, -7.80f, -2.41f, 2.54f, 6.50f, 10.16f,
                                              13.34f, 15.90f, 17.87f, 19.25f, 19.40f };
            return interp (knobDb, kp, dp);
        }

        // The 1-7 panel dials (7 = fastest, as on the front panel).
        static float attackUs (float dial) noexcept
        { return 800.0f * std::pow (20.0f / 800.0f, (dial - 1.0f) / 6.0f); }
        static float releaseMs (float dial) noexcept
        { return 1100.0f * std::pow (50.0f / 1100.0f, (dial - 1.0f) / 6.0f); }

    private:
        static float interp (float k, const float (&kp)[17], const float (&dp)[17]) noexcept
        {
            if (k <= -57.5f) return -120.0f;
            float d;
            if      (k <= kp[0])  d = dp[0] + (k - kp[0]) * (dp[1] - dp[0]) / (kp[1] - kp[0]);
            else if (k >= kp[16]) d = dp[16];
            else
            {
                d = dp[16];
                for (int i = 1; i < 17; ++i)
                    if (k <= kp[i])
                    {
                        d = juce::jmap (k, kp[i-1], kp[i], dp[i-1], dp[i]);
                        break;
                    }
            }
            return juce::jlimit (-120.0f, 25.0f, d);
        }
    };

    //==========================================================================
    // juce::dsp::Oversampling only guards its block size with a debug-build
    // jassert - a Release host handing more samples than prepareToPlay
    // announced (offline render, freeze) would write past its buffers. So
    // oversized blocks are processed in prepared-size slices.
    // Optional caller-owned trace contains negative peak gain-cell dB for
    // every base-rate sample. Storage is prepared by the caller, never here.
    void process (juce::AudioBuffer<float>& buffer, int numSamples, float* reductionTrace = nullptr)
    {
        if (numSamples <= preparedBlock)
        {
            processChunk (buffer, numSamples, reductionTrace);
            return;
        }

        float worstGr = 0.0f;
        for (int done = 0; done < numSamples; done += preparedBlock)
        {
            const int n = juce::jmin (preparedBlock, numSamples - done);
            juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                            buffer.getNumChannels(), done, n);
            processChunk (slice, n, reductionTrace != nullptr ? reductionTrace + done : nullptr);
            worstGr = juce::jmin (worstGr, peakGrDb);
        }
        peakGrDb = worstGr;
    }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int numSamples, float* reductionTrace)
    {
        auto* oversampler = oversamplers[osFactorLog2].get();
        if (oversampler == nullptr || numSamples <= 0)
            return;

        const int numCh = juce::jmin (buffer.getNumChannels(), channels);
        if (numCh <= 0)
            return;

        juce::ScopedNoDenormals noDenormals;

        // Dry copy for the parallel blend, taken before anything touches it.
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            (size_t) numCh, 0, (size_t) numSamples);

        auto osBlock = oversampler->processSamplesUp (block);

        const int  osSamples = (int) osBlock.getNumSamples();
        float      blockPeakGr = 0.0f;
        if (reductionTrace != nullptr)
            juce::FloatVectorOperations::clear (reductionTrace, numSamples);

        // ONE detector for both channels. Running an independent envelope per
        // channel lets the louder side get pulled down harder, so the stereo
        // image collapses toward the middle and then WANDERS with the
        // programme - measured 9.54 dB of separation arriving as 2.48 dB, a
        // 7.06 dB shift, which is grossly audible on anything stereo. The
        // hardware links via the stereo strap for exactly this reason. Detector
        // is max(|L|,|R|) so the loudest channel governs and neither can duck
        // out from under the other.
        {
            auto* dL = osBlock.getChannelPointer (0);
            auto* dR = (numCh > 1) ? osBlock.getChannelPointer (1) : nullptr;
            auto& s  = state[0];

            const bool hasRight    = (dR != nullptr);
            const bool useLimiting = params.limiting;

            for (int i = 0; i < osSamples; ++i)
            {
                float xL = dL[i];
                float xR = (dR != nullptr) ? dR[i] : 0.0f;

                if (! std::isfinite (xL)) xL = 0.0f;
                if (! std::isfinite (xR)) xR = 0.0f;

                // --- Input stage. Raises level and threshold together, which
                //     is the whole ergonomic point of the original. Smoothed,
                //     because Input is the control you actually ride on this
                //     design and stepping it raw put a 0.31 discontinuity into
                //     a signal whose steady-state sample delta is 0.004 - a
                //     click, 74x the noise floor of the waveform itself.
                const float inG  = smoothedInput.getNextValue();
                const float outG = smoothedOutput.getNextValue();

                // H uses an electronic receiver BEFORE the input control.
                if (params.revisionH)
                {
                    xL = revHStages[0].input (xL);
                    xR = revHStages[1].input (xR);
                }
                float x = xL * inG;
                xR *= inG;

                // --- Input coupling ---------------------------------------
                // The real signal path is transformer-coupled into the
                // attenuator and capacitor-coupled out of the gain cell, so DC
                // never reaches the FET. Without this the model passes DC
                // straight through, and a sample carrying an offset reads to
                // the detector as constant signal - so it sits on permanent
                // gain reduction that never releases, because from the loop's
                // point of view nothing ever gets quieter. Vinyl rips and
                // badly-trimmed one-shots carry offsets more often than you
                // would like.
                //
                // One pole at 8 Hz. The published response is 20 Hz - 20 kHz
                // +/-1 dB, and a single pole here costs 0.64 dB at 20 Hz, so it
                // fits inside that with room to spare. Note the real unit's LF
                // character comes mostly from the transformers, which are not
                // modelled - this blocks DC honestly but does not pretend to be
                // an iron-cored bass response.
                if (! params.revisionH)
                {
                    x  = highPass (x,  hpState[0]);
                    xR = highPass (xR, hpState[1]);
                }

                // --- Gain cell -------------------------------------------
                // Control voltage sets the linear conductance. At rest the gate
                // sits AT pinch-off, so u = Vgs - Vp = 0, the channel is shut,
                // R_ds is infinite and the divider passes the signal untouched.
                // The sidechain drives the gate up from there, opening the
                // channel and shunting signal to ground. Getting this polarity
                // backwards pins the cell at full attenuation and nothing
                // downstream can move it - measured -45.15 dB regardless of
                // input, ratio or attack, which is 150R/(27k+150R) exactly.
                // G0 = 2*beta*(vg0 - Vp) with vg0 = Vp + ctrlVolts, so the
                // pinch-off voltage cancels outright - it was being added and
                // subtracted again every single sample. cellK is
                // parameter-invariant and now lives in updateCoefficients.
                const float G0  = twoBeta * s.ctrlVolts;
                // Meter the conductance actually applied to THIS sample, not
                // the detector state at the end of the host block. This is
                // the linked gain cell's small-signal attenuation, excluding
                // makeup gain, transformer colour, and Input/Output controls.
                const float appliedGr = juce::Decibels::gainToDecibels
                    (1.0f / (1.0f + R5 * G0), -60.0f);
                blockPeakGr = juce::jmin (blockPeakGr, appliedGr);
                if (reductionTrace != nullptr)
                {
                    auto& trace = reductionTrace[i >> osFactorLog2];
                    trace = juce::jmin (trace, appliedGr);
                }

                // Audio units are not volts. The signal across the FET is small
                // by design - part of what the LN revision does is drop the
                // voltage at the gain-reduction FET to keep it in its linear
                // region. Feeding full-scale audio straight into the device
                // equation puts the k*v^2 term on a par with the linear one and
                // yields ~23% THD, which is nothing like a real unit (<0.5%).
                const float v   = solveCell (x  * CellVoltsPerUnit, G0, cellK) * InvCellVolts;
                const float vR  = hasRight
                                ? solveCell (xR * CellVoltsPerUnit, G0, cellK) * InvCellVolts
                                : 0.0f;

                // --- Sidechain, tapped AFTER the cell. Feedback topology, so
                //     this sample's detector reading drives the NEXT sample's
                //     gain - a one-sample loop delay, which is what keeps a
                //     digital feedback compressor stable and is inaudible at
                //     the oversampled rate.
                //
                // The ratio bank sets the SHAPE of the detector law, not its
                // gain. In a feedback loop the static slope falls out of that
                // shape alone: a linear detector gives c ~ sqrt(X), so output
                // ~ sqrt(input), which is 2:1 no matter how hard you drive it.
                // Measured that directly - 1.94/1.97/1.98/1.99 for the four
                // positions when the bank was only scaling. A power law of
                // exponent p gives a ratio of exactly p+1, hence p = ratio-1.
                // That is also the honest circuit reading: a diode rectifier
                // behind a resistive divider is a power law, not a gain.
                // ORDER MATTERS HERE, and getting it wrong costs both ends.
                // The timing capacitor is charged by the RECTIFIER, and the
                // control amp's nonlinearity sits after it. Smoothing the
                // control voltage instead - i.e. putting the RC after the power
                // law - wrecks both behaviours at once: the law's output runs
                // to hundreds, so the slowest attack still covers the distance
                // in one step (measured a flat 83 us across the whole 20-800 us
                // range), and clamping it back down to fix that flattens the
                // very loop gain the ratio is made of (4:1 collapsed to 2.35:1,
                // 20:1 to 6.03:1). Rectify, smooth, THEN shape.
                const float rect = hasRight ? juce::jmax (std::abs (v), std::abs (vR))
                                            : std::abs (v);

                // The capacitor. Deliberately NOT a smoother on gain reduction
                // in dB - it is the real charge/discharge on a signal-domain
                // envelope. At 20 us it partially tracks individual cycles of
                // low-frequency content, and that intermodulation is a large
                // part of the sound. Smoothing it away in the gain domain is
                // exactly the mistake the old design made.
                //
                // ATTACK IS SLEW-LIMITED, NOT EXPONENTIAL, and that is the
                // whole reason the dial means anything. In a feedback topology
                // the detector sees the UNCOMPRESSED signal until the loop
                // closes, so the envelope's target starts out enormous - at
                // +30 dB into a DC step it is ~15.8 while the envelope settles
                // near 0.29, a 54x overshoot. An exponential covers that first
                // 0.29 in 1.85% of its own time constant, so the loop shuts
                // before the RC contributes anything and every setting from
                // 20 us to 800 us measured the same 62-83 us - which was really
                // just the oversampler's group delay.
                //
                // The hardware does not charge that way either: the timing cap
                // is fed through the rectifier diode, which behaves far more
                // like a current source than a resistor to a distant rail. So
                // attack charges at a bounded rate and the dialled time is the
                // time to traverse the control range. Release stays exponential
                // because that genuinely is a passive RC discharge.
                // Attack OFF holds the cell open. The signal still travels the
                // whole path - divider, line amp, transformer - so the colour
                // is all still there; only the gain reduction goes away.
                if (! useLimiting)
                {
                    s.env = 0.0f;
                    s.envRes = 0.0f;
                    s.biasShift = 0.0f;
                }
                else
                {
                    // Current-limited diode charge: slew-limited far from the
                    // target (the dial calibration - see the block comment
                    // above), EXPONENTIAL landing near it. And the rectifier
                    // is a DIODE, not a comparator: conduction does not snap
                    // off the instant the signal falls below the cap - the
                    // exponential knee keeps a trickle of charge flowing
                    // while peaks sit just under it. Two consequences, both
                    // measured on the UAD and previously missing here:
                    //   - release SLOWS while programme is present (each LF
                    //     cycle peak tops the cap back up a little), while
                    //     the burst-into-silence reference curves are
                    //     untouched - far below the knee the trickle is gone
                    //   - small mid-tail bumps are met inside the knee, so
                    //     the re-grab is gentle instead of a full-rate slew:
                    //     the 2-3 dB tail chatter on 808 tails (Probe808,
                    //     wobblesource) where the UAD moved ~1 dB.
                    //
                    // ONE smooth conduction law, no branches. The first cut
                    // was piecewise, and its discontinuous velocity at the
                    // segment edges made the envelope limit-cycle around
                    // them: half-harmonic sidebands at -25 dBc, heard as a
                    // buzzy tone with everything on max. softplus IS the
                    // diode knee - exact charge law far above, zero far
                    // below, C-infinity everywhere between - and the
                    // discharge runs unconditionally, as the release
                    // resistor does.
                    // While the diode conducts, its low impedance OWNS the
                    // node - the release resistor's pull is irrelevant next
                    // to it (which the old comparator encoded implicitly;
                    // running the discharge unconditionally shifted every
                    // static operating point by up to 8 dB). So the
                    // discharge is gated by the logistic complement of the
                    // same knee: full in silence, zero under conduction,
                    // smooth everywhere - no segment edges to limit-cycle
                    // around.
                    // The knee lives in the DISCHARGE GATE ONLY. Two failed
                    // cuts taught the constraints: a piecewise gate has
                    // discontinuous velocity at its segment edges and the
                    // envelope limit-cycles around them (half-harmonic
                    // sidebands at -25 dBc - the buzzy tone at max); a
                    // softplus CHARGE trickle below conduction is not
                    // physics - a reverse-biased diode passes nothing, and
                    // the trickle grew a signal-independent envelope floor
                    // that saturated the detector (statics slopes collapsed
                    // to ~1.1:1). So: charge exactly as before - zero below
                    // conduction, continuous at the origin - and the diode
                    // knee gates the DISCHARGE as a logistic: full in
                    // silence, zero under conduction, C-infinity between.
                    // Programme whose peaks graze the cap slows its own
                    // release; a burst into silence releases on the
                    // reference curve.
                    const float over = rect - s.env;
                    if (over > 0.0f)
                        s.env += juce::jmin (attackStep, over * attackRcCoef);

                    // DiodeKneeV = 0 is the exact comparator law that shipped
                    // as "finally stable". Nonzero re-enables the logistic
                    // gate for bench experiments ONLY: three formulations all
                    // measured worse than the UAD on real material (limit
                    // cycle; unphysical charge floor; and the gate version
                    // audible as sudden release jumps at high settings).
                    // The programme-dependent release law will be derived
                    // from real-hardware release-under-programme captures,
                    // not fitted blind - see the test pack's busy-tail file.
                    if (DiodeKneeV <= 0.0f)
                        s.envDischargeGate = (over > 0.0f) ? 0.0f : 1.0f;
                    else if (over > 6.0f * DiodeKneeV)
                        s.envDischargeGate = 0.0f;
                    else if (over < -12.0f * DiodeKneeV)
                        s.envDischargeGate = 1.0f;
                    else
                        s.envDischargeGate = 1.0f / (1.0f + dsp_math::fastExp2 (over * (1.4427f / DiodeKneeV)));
                }

                if (useLimiting)
                    // RELEASE: exponential discharge toward the Q-bias node,
                    // NOT an RC to the signal. R56/R57 run from the storage
                    // cap to BRN -> R59 -> the Q-bias network (Rev D trace,
                    // Notes/1176-revD-component-values.md), and the service
                    // trimmer sits that bias EXACTLY at the detector's
                    // conduction threshold - that is what "Q bias adjust"
                    // means. With the target AT threshold, t63 = R x C, and
                    // the published 50 ms-1.1 s endpoints land on R56/R57
                    // with a 0.22 uF cap (a discharge-to-bias t63 cannot
                    // exceed ~1.2x RC, so C27's 5.9-116 ms range can never
                    // reach them - the release cap is 0.22 uF, full stop).
                    // Three measured UAD behaviours pin this law uniquely:
                    //   - hit-to-hit stability: release rate proportional to
                    //     remaining GR, so deep GR stays deep between fast
                    //     snares (bias BELOW threshold reaches zero in finite
                    //     time and slams - tried, measured, heard)
                    //   - held tails: GR decays exponentially, never zero in
                    //     finite time (constant-current swept 21 dB during
                    //     one snare tail vs the UAD's 7 - tried, measured)
                    //   - exp-in-dB recovery matching the UADx curve family.
                    // The old RC-to-signal released several times too fast on
                    // decaying material: every snare tail pumped ~14 dB and
                    // slammed back on the next hit - the "falling apart"
                    // audible on the 909 snare (FlutterProbe).
                {
                    const float biasEff = envBias + s.biasShift;
                    const float dis = (biasEff - s.env) * releaseCoef * s.envDischargeGate;
                    s.env += dis;
                    // what leaves the fast cap through the pot arrives at
                    // the bias node and charges its capacitance
                    if (dis < 0.0f)
                        s.biasShift -= biasFracEff * dis;
                }

                // coupling resistor between the fast node and the reservoir
                // - always conducting, it is a resistor; different caps give
                // the two directions different taus
                s.env    += (s.envRes - s.env) * relCoupleA;
                s.envRes += (s.env - s.envRes) * relChargeB;

                // the bias node drains through its fixed network resistance
                s.biasShift -= s.biasShift * biasDrainCoef;

                if (! std::isfinite (s.env))       s.env = 0.0f;
                if (! std::isfinite (s.envRes))    s.envRes = 0.0f;
                if (! std::isfinite (s.biasShift)) s.biasShift = 0.0f;

                s.env       = juce::jlimit (0.0f, MaxEnvelope, s.env);
                s.envRes    = juce::jlimit (0.0f, MaxEnvelope, s.envRes);
                s.biasShift = juce::jlimit (0.0f, 0.5f * MaxEnvelope, s.biasShift);

                // Detector law: a DIODE, because that is what CR3/CR4 are, and
                // it is where the ratio actually comes from. UREI's own theory
                // of operation says the ratio switch sets "the rate of change
                // of bias, as well as the threshold point" - two ganged poles
                // doing two jobs:
                //
                //   ratioDrive  - the 56k ladder, scaling signal into the
                //                 detector. This is the "rate of change".
                //   ratioBiasV  - the low-value ladder (470/560/1.5k with 150R,
                //                 fed through 10M), biasing the diodes. This is
                //                 the "threshold point".
                //
                // Loop analysis gives ratio = 1 + A with A = d(log ctrl)/d(log
                // env), and for an exponential law A is proportional to the
                // drive level. So MORE DRIVE GENUINELY PRODUCES A HIGHER RATIO
                // and the four ratios emerge from two sets of resistor values
                // rather than being dialled in. The previous power law with a
                // switched exponent could only imitate that - it hit the right
                // numbers with an invented mechanism.
                //
                // exp(x) - 1 is the diode's own form, and it gives zero output
                // below the bias for free: with no signal the argument is
                // negative, exp is under 1, and the result clamps off.
                // DetectorInvVt and Log2E are both constants; folding them saves
                // a multiply per sample and the clamp simply moves with them.
                const float arg = juce::jlimit (-43.3f, 28.9f,
                                    (ratioDrive * s.env - ratioBiasV) * DetectorExpScale);

                s.ctrlVolts = juce::jlimit (0.0f, MaxCtrlVolts,
                                            DetectorK * (dsp_math::fastExp2 (arg) - 1.0f));

                // --- Output section ---------------------------------------
                // Signal order follows the hardware: gain cell, then the Class A
                // line amp, then the Output control, then the transformer. The
                // Output knob sitting BEFORE the iron is not an accident - it
                // is how you drive the transformer harder on a real unit, and
                // keeping that order gives the control the same dual character
                // it has on the hardware.
                if (params.revisionH)
                {
                    // H output control feeds its push-pull line amplifier.
                    dL[i] = revHStages[0].output (v * outG);
                    if (hasRight) dR[i] = revHStages[1].output (vR * outG);
                }
                else
                {
                    dL[i] = transformer (classA (v) * outG, txState[0]);
                    if (hasRight)
                        dR[i] = transformer (classA (vR) * outG, txState[1]);
                }

                s.lastCell = v;
            }


        }

        oversampler->processSamplesDown (block);

        peakGrDb = blockPeakGr;

        // --- Parallel blend ---------------------------------------------
        if (params.mix < 1.0f)
        {
            const float wet = params.mix;
            const float dry = 1.0f - wet;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* w = buffer.getWritePointer (ch);
                auto* r = dryBuffer.getReadPointer (ch);

                for (int i = 0; i < numSamples; ++i)
                    w[i] = w[i] * wet + r[i] * dry;
            }
        }
    }

private:
    //==========================================================================
    // The closed-form divider solution derived in the header comment.
    //
    //     R5*k*v^2 + (1 + R5*G0)*v - x = 0
    //
    // k == 0 is the linearised case and degenerates to a plain divider, so it
    // gets its own branch rather than a 0/0. When k < 0 (Rev A territory) the
    // discriminant can go negative for large enough x - physically that is the
    // FET leaving the ohmic region, and the model has no business extrapolating
    // past it, so it clamps at the vertex.
    static inline float solveCell (float x, float G0, float k) noexcept
    {
        const float b = 1.0f + R5 * G0;

        if (std::abs (k) < 1.0e-12f)
            return x / juce::jmax (1.0e-6f, b);

        const float a    = R5 * k;
        const float disc = b * b + 4.0f * a * x;

        if (disc <= 0.0f)
            return -b / (2.0f * a);

        return (-b + std::sqrt (disc)) / (2.0f * a);
    }

    //==========================================================================
    // Class A line amplifier. Single-ended, so its transfer curve is asymmetric
    // and the dominant product is SECOND harmonic - the opposite of a push-pull
    // stage, which cancels evens by construction. The bias term is what breaks
    // the symmetry; without it a tanh gives odd harmonics only and sounds like
    // a fuzz pedal rather than an amplifier.
    //
    // This is also where "gets dirtier the harder you push it" comes from. The
    // gain cell alone does the opposite - a JFET gets proportionally MORE
    // linear as it turns on harder - which is why the model measured 1.23% THD
    // at light drive falling to 0.31% at heavy drive, exactly backwards from a
    // real unit. The cell's output rises with drive, so this stage sees more
    // level and contributes more, and the trend flips.
    inline float classA (float x) const noexcept
    {
        // The clamp is not cosmetic. dsp_math::fastTanh evaluates
        // x_poly / sqrt(x_poly^2 + 1) with a 5th-order x_poly, so a large
        // argument overflows to inf and inf/inf is NaN - where std::tanh would
        // have saturated cleanly at 1.0. A 1e30 sample is FINITE, so it sails
        // past the non-finite scrub at the input and only detonates here. That
        // is exactly what the robustness test caught when the fast math went
        // in. Clamping is also what the real stage does: it runs off +-30 V
        // rails and simply cannot output more.
        const float xc = juce::jlimit (-RailVolts, RailVolts, x);
        return (dsp_math::fastTanh (classADrive * xc + ClassABias) - classABiasOut) * classANorm;
    }

    //==========================================================================
    // Output transformer.
    //
    // The essential thing, and the reason a static waveshaper cannot stand in
    // for iron: core saturation follows FLUX, and flux is the integral of the
    // voltage across the winding. A 50 Hz signal therefore drives many times
    // the core excursion of a 5 kHz signal at the same amplitude - roughly
    // 100x with these coefficients - so the transformer smears bass and leaves
    // treble essentially untouched. That frequency-dependent saturation is the
    // whole character. Integrate, saturate, differentiate.
    //
    // (Approach borrowed conceptually from the flux/hysteresis path in Jatin
    // Chowdhury's tape work - the structure, not the code, which is GPL.)
    //
    // In the linear region the integrator and differentiator are exact
    // inverses, so this is transparent until the core actually starts to work.
    // The final one-pole stands in for leakage inductance, which is what rolls
    // off the top of a real transformer.
    // Langevin function and its derivative, with the near-zero approximations
    // that keep coth's singularity out of the audio path. Below 1e-4 the series
    // expansions L -> x/3 and L' -> 1/3 are exact to float precision and the
    // reciprocals are not evaluated at all.
    // THE THRESHOLD HERE IS 0.5, NOT THE 1e-4 THE LITERATURE QUOTES, AND THAT
    // MATTERS ENORMOUSLY IN SINGLE PRECISION.
    //
    // L'(x) = 1/x^2 - coth^2(x) + 1. At x = 2e-4 both 1/x^2 and coth^2(x) are
    // about 2.5e7 and the true difference is ~0.333. Subtracting two huge
    // nearly-equal floats leaves roughly 2.5e7 * 1e-7 = 2.5 of noise, so the
    // result is garbage - it returns 1 +/- 2.5 instead of a third. Catastrophic
    // cancellation, and it does not announce itself: the model still runs, the
    // state stays finite, and the only symptom is that the permeability term
    // swings between -19 and +27 where it should sit at 1.0. Measured THD then
    // RISES with frequency (0.05% at 40 Hz, 23% at 5 kHz), which is backwards
    // for a flux-driven core and was what gave it away. The 1e-4 guard is fine
    // in double; in float it is far too small.
    //
    // Below 0.5 the series expansions are used instead. Three terms are exact
    // to about 0.06% at x = 0.5 and improve rapidly below that, and above 0.5
    // the closed forms lose at most a digit to cancellation.
    // BOTH values come out of one call because both need coth(x), and the
    // J-A core always wants the pair. This was two functions, each computing
    // 1/fastTanh(x) on the same x.
    //
    // MEASURED, so nobody re-does this expecting a win: merging them saves
    // NOTHING on clang -O3. Both helpers were static, pure and called with
    // the same argument, so the optimiser already collapsed the duplicate -
    // the compiled FetProbe binary contains exactly 162 sqrt instructions
    // either way (fastTanh is a sqrt), and the 2x-oversampled cost sits at
    // ~97 ns/sample before and after, inside run-to-run noise. The pair form
    // is kept because it states the sharing in the source instead of relying
    // on the optimiser to notice it, which also covers toolchains that do not
    // (the Windows build is MSVC). It is not a speed-up; do not describe it
    // as one.
    //
    // The merge is bit-exact: every expression below is character-for-
    // character what the two helpers computed, so hoisting `ct` cannot move a
    // last bit. Tests/LangevinNullProbe holds the old algebra beside the new
    // and proves it over a Q sweep across the threshold and past +/-3, plus a
    // whole-DSP render null on both transformer branches.
    //
    // Do not "improve" 1.0f / (x * x) into invX * invX to save the divide.
    // It is not bit-identical, and this core is fitted to measured tables.
    static inline void langevinPair (float x, float& L, float& Lp) noexcept
    {
        if (std::abs (x) > 0.5f)
        {
            const float ct = 1.0f / dsp_math::fastTanh (x);
            L  = ct - 1.0f / x;
            Lp = 1.0f / (x * x) - ct * ct + 1.0f;
            return;
        }

        const float x2 = x * x;
        L  = x * (1.0f / 3.0f - x2 * (1.0f / 45.0f - x2 * (2.0f / 945.0f)));
        Lp = 1.0f / 3.0f - x2 * (1.0f / 15.0f - x2 * (2.0f / 189.0f));
    }

    // The Jiles-Atherton ODE. Returns dM/dH_dot - i.e. multiply by Hdot to get
    // dM/dt. This is the whole reason to use J-A rather than a static curve:
    // hysteresis enters as a modification of the SLOPE, so the output stays
    // continuous. The previous attempt added a direction-dependent offset to
    // the magnetisation VALUE and then differentiated it, which put a step
    // through a 3500x differentiator and produced 95-128% THD.
    inline float jilesAthertonSlope (float M, float H, float Hdot) const noexcept
    {
        // The bias term is what produces EVEN harmonics, and it has to be here
        // rather than in the hysteresis. A symmetric loop driven symmetrically
        // has half-wave symmetry, M(t + T/2) = -M(t), which mathematically
        // permits odd orders ONLY - so adding Jiles-Atherton did not on its own
        // move the even/odd balance at all (measured -28 dB before and after).
        // Hysteresis buys loop area, remanence and frequency dependence; it
        // does not buy second harmonic.
        //
        // What does is asymmetry, and the circuit has a real source of it: the
        // Class A line amp ahead of the transformer is single-ended, so it sits
        // at a DC operating point and magnetically biases the core. Working
        // off-centre on the Langevin curve makes the positive and negative flux
        // excursions unequal, which is exactly second harmonic.
        // Asymmetric core. The shape parameter differs slightly between the two
        // polarities, so the curve is not point-symmetric and the positive and
        // negative flux excursions are unequal - which is second harmonic.
        //
        // The asymmetry has to be in the SHAPE, not a DC offset on Q. A constant
        // offset was tried and is wrong twice over: it dominates at small signal
        // (so THD went frequency-independent at ~10%, flux dependence collapsed
        // to 1.1x) and it puts a DC term in diff, which makes the delta_M gate
        // open on one half-cycle and shut on the other - half-wave rectifier
        // behaviour, level-independent. Scaling `a` instead makes the asymmetry
        // proportional to how hard the core is driven, so it appears when pushed
        // and vanishes at bench level where the THD spec is measured.
        const float qRaw = (H + txAlpha * M) * invTxA;
        const float aEff = 1.0f + txCoreAsym * dsp_math::fastTanh (
                               juce::jlimit (-RailVolts, RailVolts, qRaw));

        // One reciprocal serves both Q and dMan; this was two divisions.
        const float invAEff = 1.0f / aEff;

        const float Q = qRaw * invAEff;

        // One call, one coth: the anhysteretic curve and its slope together.
        float Man, dManRaw;                       // Ms folded in as 1 (normalised)
        langevinPair (Q, Man, dManRaw);
        const float dMan = dManRaw * invTxA * invAEff;

        const float diff  = Man - M;
        const float delta = (Hdot >= 0.0f) ? 1.0f : -1.0f;

        // delta_M gates the irreversible term off whenever the magnetisation is
        // already moving toward the anhysteretic curve. Without it the model
        // admits the well-known nonphysical branch where the loop runs backwards.
        const float deltaM = ((delta > 0.0f) == (diff > 0.0f)) ? 1.0f : 0.0f;

        float den = oneMinusCk * delta - txAlpha * diff;

        // The denominator passes through zero at the loop tips. Clamp rather
        // than divide into it; the magnitude floor is small enough not to alter
        // the loop shape and large enough to stay well inside float range.
        if (std::abs (den) < 1.0e-6f)
            den = (den < 0.0f) ? -1.0e-6f : 1.0e-6f;

        const float irr = oneMinusC * deltaM * diff / den;
        const float rev = txC * dMan;

        const float scale = 1.0f - cAlpha * dMan;

        return (irr + rev) / ((std::abs (scale) < 1.0e-6f) ? 1.0e-6f : scale);
    }

    struct TxState { float H = 0.0f, M = 0.0f, lp = 0.0f; };

    inline float transformer (float x, TxState& s) const noexcept
    {
        float y;
        if (txCorrect)
        {
            // Physics-corrected topology (mode B of the iron A/B). v1 below is
            // a CURRENT-driven core: H is imposed by the input and the output
            // is dB/dt = (1+u)*Hdot, so rising permeability becomes voltage
            // GAIN - the +5.5 dB LF boost TxProbe measured. A line output
            // drives the primary from LOW impedance: the winding imposes the
            // FLUX (B = integral of V), and the core answers with whatever
            // magnetising field H(B) the B-H loop demands. That field is
            // magnetising current through the source impedance - a pure,
            // LF-weighted LOSS that spikes when M plateaus. Incremental
            // inversion of B = H + M: dH = dB / (1 + dM/dH).
            // The drop must close the loop: magnetising current is in
            // QUADRATURE with the input (H is the integral), and open-loop
            // subtraction of a quadrature term does not attenuate - it grows
            // magnitude, |x - jk*x| > |x|. Feeding the flux from the POST-drop
            // winding voltage makes this the passive RL divider it is on the
            // schematic: V_wind = x - Rs*i_mag, flux integrates V_wind. Linear
            // regime = a plain one-pole HP; saturation collapses dM/dH, the
            // same voltage needs far more field, the drop grows and the bass
            // thins exactly when it distorts. A divider cannot exceed unity.
            // Line-amp gain trim for the B core's insertion loss: measured
            // +5.8 dB flat while compressing (TxProbe 7b) - the fixed trim
            // a bench tech sets so the output stage hits nominal level.
            y = (x - txSrcB * s.H) * TxBTrim;

            const float dB  = y * txFluxGain;
            const float s2b = jilesAthertonSlope (s.M, s.H, dB);
            const float dH  = dB / juce::jmax (0.05f, 1.0f + s2b);

            s.H += dH;
            s.M += s2b * dH;

            if (! std::isfinite (s.H)) s.H = 0.0f;
            if (! std::isfinite (s.M)) s.M = 0.0f;

            s.H *= txDecay;
            s.M  = juce::jlimit (-4.0f, 4.0f, s.M);
        }
        else
        {
            // The applied field is the integral of the winding voltage, so the
            // input IS Hdot. That is what makes the core frequency dependent: a
            // low note drives far more field excursion than a high one at equal
            // level.
            const float Hdot = x * txFluxGain;

            const float s2 = jilesAthertonSlope (s.M, s.H, Hdot);

            s.H += Hdot;
            s.M += s2 * Hdot;

            if (! std::isfinite (s.H)) s.H = 0.0f;
            if (! std::isfinite (s.M)) s.M = 0.0f;

            s.H *= txDecay;   // finite magnetising inductance; also bleeds any DC
            s.M  = juce::jlimit (-4.0f, 4.0f, s.M);

            // Output is dB/dt with B = H + M, so dB/dt = Hdot*(1 + dM/dH). Writing
            // it that way is not cosmetic: the obvious form, (Hdot + dM)*(1/txFluxGain),
            // divides by a number around 1e-5 and so multiplies every bit of solver
            // noise by ~40000. That produced THD readings that jumped around
            // incoherently with frequency - 11.9% at 1 kHz while 40 Hz read 5.3%.
            // Factoring Hdot out cancels the huge gain algebraically and leaves a
            // plain gain term that IS the permeability. txNorm restores unity at
            // small signal, so the core is transparent until it actually works.
            y = x * (1.0f + s2) * txNorm;
        }

        s.lp += (y - s.lp) * txLeakCoef;
        if (! std::isfinite (s.lp)) s.lp = 0.0f;
        return s.lp;
    }

    // TPT one-pole high-pass. Topology-preserving so it stays well behaved at
    // a cutoff this far below the sample rate, where a naive direct form loses
    // its coefficients into rounding error.
    struct HpState { float z = 0.0f; };

    inline float highPass (float x, HpState& s) const noexcept
    {
        const float v  = (x - s.z) * hpG;
        const float lp = v + s.z;
        s.z = lp + v;

        if (! std::isfinite (s.z))
            s.z = 0.0f;

        return x - lp;
    }

    // x^n for the small odd exponents the ratio bank uses (3, 7, 11, 19).
    // Repeated squaring - six multiplies at worst, no pow() in the hot loop.
    static inline float powInt (float x, int n) noexcept
    {
        float result = 1.0f;
        float base   = x;

        while (n > 0)
        {
            if (n & 1) result *= base;
            base *= base;
            n >>= 1;
        }

        return result;
    }

    void updateCoefficients()
    {
        for (auto& stage : revHStages) stage.setSampleRate (osRate);
        // First: several blocks below (flux scale, leak, core k) branch on it.
        txCorrect = (params.txModel == 1);

        beta = params.idss / (params.pinchOffV * params.pinchOffV);

        // TPT prewarp for the coupling high-pass, at the oversampled rate.
        const double g = std::tan (juce::MathConstants<double>::pi
                                   * CouplingHz / juce::jmax (1.0, osRate));
        hpG = (float) (g / (1.0 + g));

        // Class A stage. classABiasOut removes the DC the asymmetric curve
        // would otherwise introduce, and classANorm restores unity slope at the
        // origin so the stage is transparent at low level.
        classADrive   = ClassADriveBase * juce::jmax (1.0e-4f, params.ironAmount);
        // MUST be the same tanh the audio path evaluates: computing the
        // cancellation reference with std::tanh against fastTanh in the loop
        // leaves the approximation error as a constant DC offset - measured
        // -82 dBFS of idle output after the iron trim (FreqPhaseProbe),
        // where the UADx is digitally silent.
        classABiasOut = dsp_math::fastTanh (ClassABias);
        classANorm    = 1.0f / (classADrive * (1.0f - classABiasOut * classABiasOut));

        // Transformer. txFluxGain sets where the core starts to work: the
        // magnetising integrator has gain g/w, so at 50 Hz it multiplies by
        // ~1200 and at 5 kHz by ~12. That 100:1 is the frequency dependence,
        // and it falls out of the integrator rather than being imposed.
        // txDiffGain inverts it so the pair is transparent below saturation.
        const double wLf = 2.0 * juce::MathConstants<double>::pi
                           * TxLowCornerHz / juce::jmax (1.0, osRate);
        txDecay    = (float) (1.0 - wLf);
        // Mode B drives the core to REAL flux: v1's scale keeps Q ~ 0.1 and
        // gets its distortion from the asym term modulating the through-gain;
        // a shunt core only distorts when M genuinely saturates (Q ~ 1+).
        txFluxGain = (float) ((txCorrect ? (double) TxFluxScaleB : TxFluxScale)
                              * (double) params.ironAmount
                              * 2.0 * juce::MathConstants<double>::pi
                              * 50.0 / juce::jmax (1.0, osRate));
        // txDiffGain used to scale a differentiated output. The output is no
        // longer differentiated - dM/dH comes from the ODE - so it was dead
        // weight recomputed on every parameter change. Removed.

        twoBeta = 2.0f * beta;
        cellK   = beta * (2.0f * params.linearisation - 1.0f);

        txA     = TxCoreA;
        txK     = txCorrect ? TxCoreK * TxKScaleB : TxCoreK;
        txC     = TxCoreC;
        txAlpha    = TxCoreAlpha;
        // Mode B: a symmetric core is nearly pure h3 (Whitlock); h2 belongs
        // to DC bias off-centre on the loop, not to a shape hack. v1's 2.4
        // sources its h2 here, which is TxProbe BUG 2.
        txCoreAsym = txCorrect ? TxAsymB : TxCoreAsym;

        invTxA     = 1.0f / txA;
        oneMinusC  = 1.0f - txC;
        oneMinusCk = oneMinusC * txK;
        cAlpha     = txC * txAlpha;

        txSrcB = TxSrcB;
        if (txCorrect)
        {
            // Proper one-pole coefficient. v1's 2*pi*fc/fs form exceeds 1 for
            // fc = 32 kHz at every rate we run and clamps - i.e. no filter.
            txLeakCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi
                                                  * TxLeakageHz / juce::jmax (1.0, osRate)));
        }
        else
        {
            const double wHf = 2.0 * juce::MathConstants<double>::pi
                               * TxLeakageHz / juce::jmax (1.0, osRate);
            txLeakCoef = (float) juce::jlimit (0.0, 1.0, wHf);
        }

        // One-pole time constants at the oversampled rate. The hardware's
        // attack and release are separate charge/discharge paths on the same
        // capacitor, which is why the coefficients are asymmetric rather than
        // one smoother with two speeds.
        // Attack pot taper through a mid anchor, same finding as the release
        // pot (AttackProbe step trajectories): the UAD's mid-dial attack
        // decays with tau ~1.4 ms where a log-linear 20-800 us interpolation
        // predicts 138 us - endpoints agree, the middle does not. Two log
        // segments through the anchored midpoint; AtkMidScale 1 = log-linear.
        const double atkNom  = (double) params.attackUs * 1.0e-6;
        const double atkFrac = juce::jlimit (0.0, 1.0,
            std::log (atkNom / 20.0e-6) / std::log (800.0 / 20.0));
        const double atkMid  = std::sqrt (20.0e-6 * 800.0e-6) * AtkMidScale;
        const double attackSec = (atkFrac <= 0.5)
            ? 20.0e-6 * std::pow (atkMid / 20.0e-6, atkFrac / 0.5)
            : atkMid * std::pow (800.0e-6 / atkMid, (atkFrac - 0.5) / 0.5);
        const double releaseSec = (double) params.releaseMs * 1.0e-3;

        // Attack is a slew rate: volts of envelope per oversampled sample, set
        // so that traversing EnvSlewSpan takes the dialled time.
        attackStep  = (float) (EnvSlewSpan / juce::jmax (1.0, attackSec * osRate));
        // Landing RC for the attack: 8x the dial time (so SLOWER than the
        // dial). Swept 0.25x-8x against the UAD's drum-train trajectory:
        // 8x gives 8.3 dB/ms worst gain step vs the UAD's 8.4, and t90 for
        // a 100 us dial lands at 107 us - the slew still sets t90, the
        // exponential only rounds the top of the move off.
        attackRcCoef = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, AtkLandFactor * attackSec * osRate)));

        // Release: exponential discharge toward the Q-bias point through the
        // dial's resistance into the 0.22 uF release cap. t63 = R x C, so the
        // published endpoints (50 ms / 1.1 s) come straight off R57/R56+R57.
        {
            const double relFrac = juce::jlimit (0.0, 1.0,
                std::log (releaseSec / 0.050) / std::log (1.100 / 0.050));
            // Pot taper through the MEASURED mid anchor (RelLawProbe,
            // Notes/rellaw-capture-2026-08-08.txt): endpoints matched the
            // UAD exactly (silence t63 2070/2060 ms at dial 1, 110/110 at
            // dial 7) but mid-dial ran 2.2x fast - the pot law is not
            // log-linear. Two log segments through the anchored midpoint.
            const double relRMid = std::sqrt (270.0e3 * 5.27e6) * (double) RelMidScale;
            const bool relNet = RelNetworkOn && params.relModel == 1;
            const double relR = ! relNet
                ? 270.0e3 * std::pow (5.27e6 / 270.0e3, relFrac)
                : (relFrac <= 0.5)
                    ? 270.0e3 * std::pow (relRMid / 270.0e3, relFrac / 0.5)
                    : relRMid * std::pow (5.27e6 / relRMid, (relFrac - 0.5) / 0.5);

            // Two-node timing network (RelLawProbe bump experiment: the UAD
            // sheds a bump's GR in <30 ms AT DIAL 4, 30x faster than its own
            // sustained release - a small fast cap in front of the main
            // storage). The release pot drains the FAST node; the reservoir
            // couples through R_c and has no path of its own. Sustained
            // material levels both nodes, so silence t63 = relR x 0.22u and
            // the dial calibration is untouched; transient charge lands on
            // the fast cap alone and sheds at t63/(1+ratio).
            const double cTot = 0.22e-6;
            if (relNet)
            {
                releaseCoef = timeToCoefficient (relR * cTot / (1.0 + (double) RelCapRatio));
                relCoupleA  = timeToCoefficient (RelCoupleTauMs * 1.0e-3);
                relChargeB  = timeToCoefficient (RelCoupleTauMs * 1.0e-3 * RelCapRatio);
                biasFracEff = BiasChargeFrac;
            }
            else
            {
                // Shipping path: single-cap law, bit-exact with the state
                // signed off by ear. Coupling and bias pickup are zeroed, so
                // the extra states stay at 0 and the loop reduces exactly.
                releaseCoef = timeToCoefficient (relR * cTot);
                relCoupleA  = 0.0f;
                relChargeB  = 0.0f;
                biasFracEff = 0.0f;
            }

            // Q-bias node compliance (the memory floor): release current
            // charges the bias network's own capacitance, raising the
            // discharge target; it drains through the bias network's FIXED
            // resistors - which is exactly why the measured -0.6..-1.2 dB
            // residual floor decays dial-independently on the UAD at every
            // dial including 7.
            biasDrainCoef = timeToCoefficient ((double) BiasTauSec);
        }

        // Ratio bank. Each position sets how hard the output drives the
        // detector AND where it starts working - the manual is explicit that
        // higher ratios raise the threshold, which falls out of the same
        // resistive network on the hardware.
        // Both columns are read off the Rev D drawing, not fitted.
        //
        // drive  = the 56k ladder's divider into its 47k termination, taps at
        //          56k / 124k / 180k / 236k from the source (R78 end).
        // biasR  = the low-value ladder's tap resistance, 150 / 620 / 1180 /
        //          2680 ohms, fed through 10M so it is a current-mode divider
        //          and the tap voltage scales with resistance.
        //
        // See Notes/1176-revD-component-values.md.
        struct RatioFit { float drive, biasR; };

        RatioFit fit;

        // Scale note: the cell reaches -6 dB at ctrlVolts ~= 0.017 and -20 dB at
        // ~0.15, because R5*G0 = 60*ctrlVolts with the default selected-part
        // values. The whole useful control range is therefore well under a volt,
        // and the thresholds below live in that domain. Change idss or pinchOffV
        // and this rescales - which is exactly what swapping a selected FET does
        // on the hardware.
        if (params.allButtons)
        {
            // All four buttons shorts the bank into parallel. Steep slope, low
            // threshold, and a bias shift that drags the cell into its nonlinear
            // region - the "distortion increases radically ... changes in attack
            // and release times, as well as a change in the bias points" the
            // manual warns about.
            // All four buttons in shorts every tap together. On the drive
            // ladder that parallels the taps, so the detector sees the least
            // attenuated one; on the bias ladder it shorts out the upper
            // sections, leaving the LOWEST threshold. Maximum drive against
            // minimum threshold is exactly why all-buttons never stops working.
            fit = { 0.456f, 150.0f };
        }
        else
        {
            switch (params.ratio)
            {
                case Ratio::four:   fit = { 0.166f,  150.0f }; break;
                case Ratio::eight:  fit = { 0.207f,  620.0f }; break;
                case Ratio::twelve: fit = { 0.275f, 1180.0f }; break;
                case Ratio::twenty: fit = { 0.456f, 2680.0f }; break;
            }
        }

        // Pole B, resolved empirically. The drawing gives the drive ladder
        // (pole A) but pole B - "the rate of change of bias" half of the
        // ratio switch - was never traced (see the notes: its wires leave
        // into the inter-board bundle). These per-position trims are solved
        // against the UADx's measured effective ratios (which START at the
        // nominal value near threshold and stiffen with drive - i.e. the
        // extra grip is loop behaviour, not a hot panel): the trim IS the
        // missing pole, determined by measurement instead of the missing
        // interconnect sheet. Index: 4/8/12/20/ALL.
        {
            const int sel = params.allButtons ? 4 : (int) params.ratio;
            ratioDrive = fit.drive * DriveScale * poleBDrive[sel];
            ratioBiasV = fit.biasR * BiasScale  * poleBBias[sel];
        }

        // Quiescent point of the timing cap: the Q-bias node the release
        // discharges toward, sitting QBiasFrac under the detector threshold
        // env (arg = 0). The real board has a trimmer for exactly this
        // ("Q BIAS ADJ. P.C.B."), so a calibration constant here is a
        // component fact, not a fudge.
        envBias = QBiasFrac * (ratioBiasV / juce::jmax (1.0e-6f, ratioDrive));
    }

    float timeToCoefficient (double seconds) const noexcept
    {
        if (seconds <= 0.0 || osRate <= 0.0)
            return 1.0f;

        return (float) (1.0 - std::exp (-1.0 / (seconds * osRate)));
    }

    //==========================================================================
    static constexpr float R5 = 27000.0f;          // series resistor, from the drawing
    static constexpr float MaxCtrlVolts = 2.0f;    // cell is ~-41 dB here; ample
    static constexpr float CellVoltsPerUnit = 0.03f;  // 0 dBFS -> volts at the FET
    static constexpr float RailVolts = 40.0f;         // stage cannot swing past its supply
    static constexpr float InvCellVolts = 1.0f / CellVoltsPerUnit;
    // Three global calibration constants. The PER-POSITION values are all
    // circuit-derived; these set the shared operating point that the drawing
    // cannot give, since env is in audio units rather than volts.
    static constexpr float DriveScale    = 1.0f;
public:
    // Pole-B empirical resolution (see updateCoefficients). Public and
    // static-mutable so the bench can solve them; the solved values are
    // the committed defaults.
    // Solved by VersusBench RATCAL against the UADx: effective plateau
    // ratios 5.58/9.16/13.39/25.49 vs its 5.44/9.06/13.39/25.60, output
    // within 0.2 dB, statics within 0.5 dB across the sweep. ALL stays at
    // unity trims: its shorted-bank fit is its own animal and reusing the
    // 20:1 trims overdrove it 18 dB - own RATCAL pass pending.
    // Mode-B hysteresis: scales the J-A pinning parameter k so the loop has
    // real area at bench level (the Rayleigh half of the U-curve). Static
    // like the poleB trims so the bench can sweep it.
    static inline float TxKScaleB = 1.0f;

    // Rectifier diode knee, in envelope volts: the conduction transition
    // width around rect == env. Above the knee: full charge law. Within it:
    // the trickle scales the discharge back quadratically. Bench-tunable.
    static inline float DiodeKneeV   = 0.0f;    // 0 = comparator law (shipping); bench experiments only

    // Release network, derived from the RelLawProbe capture. OFF by default
    // until the fit against the captured matrix converges and passes every
    // gate - the shipping release stays bit-exact meanwhile. Bench-tunable.
    static inline bool  RelNetworkOn   = true;   // fitted 2026-08-08, RelLawProbe FIT
    static inline float RelMidScale    = 1.85f;  // pot taper mid anchor vs geometric mean
    static inline float RelCapRatio    = 1.0f;   // reservoir/fast capacitance
    static inline float RelCoupleTauMs = 160.0f; // R_c x C_fast
    static inline float BiasTauSec     = 1.5f;   // bias node drain (FIXED R - dial independent)
    static inline float BiasChargeFrac = 0.0f;   // fit chose 0: the reservoir alone
                                                 // reproduces the bump residue; the
                                                 // -24-tail long floor stays a known gap
    static inline float DiodeKneeMul = 4.0f;

    // Mode-B source impedance: scales the magnetising-current voltage drop.
    // Sets the small-signal LF rolloff; tuned so 20 Hz at bench level matches
    // v1's -0.5 dB. Static so the bench can sweep it.
    static inline float TxSrcB = 0.5f;

    // Mode-B flux scale (v1's TxFluxScale stays untouched below).
    static inline float TxFluxScaleB = 0.2f;

    // Mode-B core asymmetry (see txCoreAsym above).
    static inline float TxAsymB = 0.3f;

    // Mode-B output-stage gain trim (linear). +5.8 dB: the measured
    // compressing-condition level delta to the v1 stage the calibrations
    // were built around.
    static inline float TxBTrim = 1.95f;

    // Attack shape, bench-tunable for the trajectory fit against the UAD
    // (VersusBench section C: ours lets +10 dB more through at 0.5 ms on a
    // 20 dB step, then over-grabs -8 dB at 2 ms before converging).
    static inline double EnvSlewSpan   = 0.035;  // dial calibration span
    static inline double AtkLandFactor = 16.0;   // landing RC vs dial time
    static inline double AtkMidScale   = 2.0;    // attack pot taper mid anchor
    // ^ fitted 2026-08-08 against the UAD step trajectories (AttackProbe
    //   FIT): score 96 -> 4.4, all three dials within ~0.5-1.3 dB across
    //   0.5-20 ms. The mid anchor is the attack pot's taper, found exactly
    //   as the release pot's was.

    static inline float poleBDrive[5] = { 1.7507f, 2.5986f, 2.9198f, 3.2227f, 3.3415f };
    static inline float poleBBias[5]  = { 2.5505f, 1.3858f, 1.2092f, 1.1407f, 20.7546f };
private:
    static constexpr float BiasScale     = 3.3e-5f;
    static constexpr float DetectorInvVt = 180.0f;
    static constexpr float DetectorK     = 0.10f;
    static constexpr float Log2E         = 1.44269504f;   // exp(x) == exp2(x*log2 e)
    static constexpr float DetectorExpScale = DetectorInvVt * Log2E;

    // Where the rectifier/control-amp output saturates. This has to sit close
    // to the loop's own settling point (roughly 0.02-0.3 V for useful gain
    // reduction), NOT merely somewhere above it. A target far beyond where the
    // loop closes means the RC only ever contributes its initial slope, and the
    // dialled attack stops mattering - the whole 20-800 us range measured a
    // flat 83 us until this came down from 2.0 V.
    static constexpr float MaxEnvelope = 4.0f;

    // Q-bias node as a fraction of the detector-threshold envelope. The
    // service trimmer's job; calibrated so the published release endpoints
    // (50 ms / 1.1 s to recover) emerge from the component RC constants.
    // 1.0 = the trimmer's target: quiescent point AT conduction threshold.
    static constexpr float QBiasFrac = 1.0f;

    // Nominal envelope travel the attack dial is calibrated against. The loop
    // settles somewhere inside this, so measured attack comes out a fraction of
    // the dial - which is correct for a feedback design, and stays proportional
    // across the range, which is what actually matters.
    static constexpr int   MaxChannels = 2;

    std::array<HpState, MaxChannels> hpState {};
    float hpG = 0.0f;

    std::array<TxState, MaxChannels> txState {};
    float txDecay = 1.0f, txFluxGain = 1.0f, txLeakCoef = 1.0f;
    float txSrcB = 0.05f;
    bool  txCorrect = false;
    float txA = 0.063f, txK = 0.077f, txC = 0.17f, txAlpha = 0.0016f;
    float invTxA = 1.0f, oneMinusC = 1.0f, oneMinusCk = 1.0f, cAlpha = 0.0f;
    float twoBeta = 0.0f, cellK = 0.0f;
    float txNorm = 1.0f;
    float txCoreAsym = 0.0f;
    float classADrive = 1.0f, classABiasOut = 0.0f, classANorm = 1.0f;

    // Asymmetry of the Class A curve. Small - this is an amplifier biased into
    // its linear region, not a distortion box.
    static constexpr float ClassABias      = -0.18f;
    static constexpr float ClassADriveBase = 0.14f;

    // Where the core starts to work, referred to a 50 Hz full-scale signal.
    // Jiles-Atherton core parameters, normalised so Ms = 1.
    //   a     - shape of the anhysteretic curve (how gently it saturates)
    //   k     - pinning / coercivity. THIS IS THE LOOP WIDTH, and the loop is
    //           where even harmonics come from. A transformer core is far
    //           softer than tape, so k sits well below tape's ~0.077.
    //   c     - reversible fraction. Higher c means a thinner loop.
    //   alpha - interdomain coupling.
    static constexpr float TxCoreA     = 0.063f;
    static constexpr float TxCoreK     = 0.040f;
    static constexpr float TxCoreC     = 0.17f;
    static constexpr float TxCoreAlpha = 0.0016f;
    // Magnetic bias from the single-ended stage ahead of the core. This is
    // the ONLY source of even harmonics in the transformer - see the note in
    // jilesAthertonSlope.
    // Core asymmetry - the only source of even harmonics here, and it scales
    // with drive so bench-level THD is unaffected.
    static constexpr float TxCoreAsym  = 2.4f;

    static constexpr double TxFluxScale  = 0.0018;
    static constexpr double TxLowCornerHz = 3.0;   // gentle; the 8 Hz input coupling owns the LF limit
    static constexpr double TxLeakageHz   = 32000.0;

    struct ChannelState
    {
        float env       = 0.0f;   // fast timing node, signal domain
        float envRes    = 0.0f;   // reservoir behind the coupling R
        float biasShift = 0.0f;   // Q-bias node displacement (the memory floor)
        float envDischargeGate = 1.0f;   // diode conduction complement, per sample
        float ctrlVolts = 0.0f;   // control-amp output, drives the gate
        float lastCell  = 0.0f;
    };

    std::array<field::RevHStages, MaxChannels> revHStages {};
    Parameters params;

    std::array<ChannelState, MaxChannels> state {};

    // Index = log2 factor: [0]=1x dummy, [1]=2x, [2]=4x. Default 4x so every
    // probe keeps measuring the model at its cleanest; FetComp selects at
    // runtime. AliasProbe, driven 7 kHz tone, worst inharmonic: -92 dBc at
    // 4x, -87 at 2x, -71 at 1x - and the UADx itself sits at -69, so even
    // 1x aliases no more than the reference does.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int osFactorLog2 = 2;
    juce::AudioBuffer<float> dryBuffer;

    double sampleRate = 44100.0;
    double osRate     = 352800.0;
    int    channels   = 2;
    int    preparedBlock = 512;

    float beta = 1.0f;
    juce::SmoothedValue<float> smoothedInput { 1.0f }, smoothedOutput { 1.0f };
    bool gainsPrimed = false;
    static constexpr double GainSmoothingSeconds = 0.02;
    static constexpr double CouplingHz = 8.0;
    float attackStep = 1.0f, attackRcCoef = 1.0f, releaseCoef = 1.0f;
    float relCoupleA = 0.0f, relChargeB = 0.0f, biasDrainCoef = 0.0f, biasFracEff = 0.0f;
    float ratioDrive = 0.166f, ratioBiasV = 0.005f;
    float envBias = 0.0f;

    float peakGrDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FetLimiterDsp)
};
