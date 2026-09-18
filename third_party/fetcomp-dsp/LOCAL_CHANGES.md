# Local integration changes

Base: fetcomp-dsp commit `de18f5ac793e36397c725abdca7fcb8c08760ce2`.
The upstream MIT license remains in `LICENSE`.

`FetLimiterDsp.h` changes:

- Clamp the prepared channel count to the model's actual two-channel capacity.
- Clear gain-ramp priming in `reset` and snap its smoothers to current parameters, so the next parameter set behaves like a fresh prepare rather than retaining an old output-gain ramp.
- Allocate its parallel-blend dry buffer in `prepare`, never on the first audio callback or a later block-size change.
- Measure the gain-cell conductance attenuation for each oversampled audio sample before updating the detector. The block meter now takes the true peak of the applied attenuation instead of reading only the end-of-block detector state.
- Add an optional caller-owned `float* reductionTrace` to `process`, containing negative peak gain-cell dB for each base-rate sample. The oversize block path offsets this pointer correctly. This tap adds no heap allocation and preserves the model's existing stereo-linked detector and audio circuit equations.

Wrapper integration in `Source/DspEngine.cpp`:

- Apply input gain once, with a 20 ms sample-based ramp before the oversampler, and hold upstream `inputDb` at zero. This provides an exact post-input-gain peak/RMS tap, without estimating or applying input gain a second time. Output gain remains upstream before the transformer with upstream smoothing.
- Use 2x maximum-quality IIR oversampling below 88.2 kHz and 1x at higher rates. Report the oversampler's integer latency and delay the unity-gain bypass path by the same sample count; plugin/host bypass use a 5 ms crossfade. The wet model continues running during bypass.
- Delay input meters by the full reported latency. Delay the gain-cell tap by the remaining downsampler/integer-compensation group delay, rounded to the nearest base sample. The upsampler group delay is computed from JUCE 8's actual maximum-quality filter design in `prepare`. IIR phase delay varies with frequency, so this is nominal group-delay alignment, not a claim of phase-linear filtering.
- Aggregate real input/output peaks, RMS energy, and positive peak GR on a 60 Hz sample clock independent of host block boundaries. Input meters crossfade from the driven input stage to latency-matched raw input using the audio bypass ramp; output meters measure the actual delivered, blended output. Fully bypassed input/output meters match and GR is zero. Mono is mirrored in the display's L/R lanes.
- Bound scratch preparation to 8,192 samples, split larger callbacks, sanitize nonfinite parameters/samples, and bound pathological sample magnitudes before recursive DSP.
- Use a bounded SPSC meter queue. Only the UI consumer advances its cursor; it discards frames more than 100 ms behind the producer's latest sequence, so a closed editor cannot trigger stale meter replay.

## HT-76 experimental Rev H integration

- Add a default-off `revisionH` flag and calls to the original Apache-2.0
  `Source/RevHStages.h` electronic receiver and push-pull/transformer model.
- Rev D retains its original input coupling and output equations. H routes its
  input receiver before Input gain and its Output control before the line amp.
- Prepare and reset the additional channel states outside processing; the
  wrapper uses separate core instances and crossfades their rendered outputs.
- Shared detector, FET and timing code remains upstream-derived under MIT.
  The H branch is an uncalibrated behavioural prototype; see
  `docs/REV_H_MODEL.md` for coefficients, sources and limitations.
