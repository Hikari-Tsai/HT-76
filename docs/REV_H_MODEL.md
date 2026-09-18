# Experimental Rev H model

HT-76 adds an automatable `revision` parameter: **Rev D** (default) and
**Rev H (Experimental)**. The two buttons appear in both Rack and Dynamic views.
The asterisk on `REV H*` identifies the experimental model. H selects a neutral
silver brushed-metal faceplate with dark legends in both views; D selects the
original charcoal finish. Layout and proportions are retained, with black knobs
and dark chart displays. The lower Rack IN/OUT PPM faceplate also follows the
finish, with dark legends on silver and recessed dark LED lanes. The finish follows the same automatable revision
parameter, including when restoring a session.

Related: [Rev D 演算法說明](REV_D_MODEL.md) · [README](../README.md#演算法來源)

## What is supported by the sources

- Universal Audio's [2-1176 manual](https://media.uaudio.com/assetlibrary/2/-/2-1176_manual.pdf)
  describes the Rev F change from the earlier single-ended Class A output stage
  to an 1109-derived push-pull output stage, Rev G's electronically balanced
  input, and Rev H's return to a silver faceplate.
- The original UREI [1176LN manual effective with serial number 7652](https://www.sowter.co.uk/schematics/1176manual.pdf)
  identifies the late hardware family. This manufacturer document is hosted by
  Sowter; it is a reference, not redistributed with this project.
- The existing compressor core is the MIT-licensed
  [fetcomp-dsp](https://github.com/Paulllux/fetcomp-dsp), derived from a Rev D
  schematic and calibrated by its author against a reference plugin.

The H implementation is **a topology-informed behavioural prototype**, not a
component-by-component simulation or a calibrated replica of a specific serial
number. Its model coefficients below are engineering assumptions, not values
measured from hardware or extracted from the schematic.

## Signal path

H: electronic input receiver -> Input gain -> shared FET gain cell/detector ->
Output gain -> symmetric push-pull amplifier -> saturable output transformer.

Each DAW channel already represents a differential signal. Left and right are
processed separately; they are never subtracted to emulate a balanced connector.
The gain-control detector remains stereo linked within each revision.

`Source/RevHStages.h` is original Apache-2.0 code:

- Electronic receiver: a 2 Hz TPT DC blocker, normalized soft rails at +/-8,
  an 80 kHz one-pole bandwidth limit and a normalized slew bound of 500,000/s.
  Receiver nonlinearity precedes the Input control. These describe a generic
  transformerless receiver, not the measured op-amp in a particular Rev H.
- Push-pull amplifier: odd-symmetric smooth crossover with rail +/-4, feedback
  factor 8, crossover width 0.04 and depth 0.012 (normalized audio units).
  A safeguarded Newton solve closes the feedback equation per oversampled sample.
  Unity small-signal gain is normalized analytically. This is a reduced transfer
  model, not a transistor-level Class AB simulation.
- Transformer: an independent symmetric saturable magnetizing branch with
  `i(phi) = phi + 0.12 * phi^3`, source loss 0.04, and flux integration scaled
  to 50 Hz. Backward Euler closes the winding-voltage/current loss relationship;
  a 40 kHz output pole represents finite bandwidth. It does not include measured
  B11148 inductance, load-dependent winding losses or hysteresis/remanence.
- A fixed output factor 1.95 retains the previous plugin's nominal output
  reference; it is not a claim about hardware transformer turns ratio. D and H
  can still differ in loudness under drive, so level-match when comparing tone.

The model keeps fetcomp-dsp's FET equations, LN residual, attack/release network,
ratio mapping and calibration tables. Those were developed for the original
Rev D-oriented model. **The H timing, threshold and ratio behaviour have not
been independently calibrated**, and should not be represented as measured
Rev H behaviour. Changing one saturation parameter or using a silver panel
would not establish a Rev H model either.

## Compatibility and real-time behaviour

- Rev D remains the default; existing parameter IDs/order are preserved.
- The new choice is appended with JUCE parameter version hint 2 and stored in state schema 2.
  States without `revision` explicitly restore D, including when loaded over H.
- Two independent cores run continuously. A 20 ms linear crossfade switches
  between them without resetting either detector or transformer. This increases
  DSP cost compared with running only one core.
- Both cores use the same existing oversampling policy and reported latency.
  A one-second host tail allowance covers H coupling-state decay.
  Rev D retains the existing external input-gain ramp and signal equations.
  H applies its gain ramp inside the core after the electronic receiver.
- IN metering remains a digital post-Input-gain reference, before H receiver
  coloration is reflected in the output. OUT measures the delivered blend.
  During a model switch, GR is a blend of the cores' gain-cell dB traces; it is
  not an exact gain measurement of two nonlinear paths mixed together.
- Buffers and both cores are prepared outside the audio callback. Switching is
  included in the existing no-allocation callback test. Bypass still uses the
  existing latency-compensated dry path and smoothing.

## Validation boundary

Tests cover numerical bounds/feedback residual, silence and reset, multiple
sample rates, different host block partitions, state compatibility, UI/automation,
and return to the continuously running D path. These establish software
behaviour, not hardware accuracy.

The implementation was also compared against a saved copy of the pre-change
Rev D core: 254,000 samples across 44.1, 48, 88.2, 96 and 192 kHz were bit-identical.
This is a finite regression check, not a guarantee for every possible input.
The local macOS ARM64 build and signal/state/editor test suite passed; Windows
and Intel builds remain covered by the configured CI workflow rather than a
local validation claim.

Before removing the experimental designation, compare with an identified Rev H:
frequency/phase response, THD and harmonic spectra over frequency and level,
static ratio/knee curves, attack/release trajectories for short and long bursts,
ALL behaviour, and output-load/headroom behaviour. Receiver rails, amp feedback,
transformer parameters and sidechain calibration must be fitted to that evidence.
