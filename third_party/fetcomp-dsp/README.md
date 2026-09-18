# FetComp DSP

The circuit-derived FET limiter that runs inside [FetComp](https://github.com/Paulllux/FetComp)
and BeatForge. This repository is the DSP only — no UI, no plugin wrapper.

Two headers:

| File | What it is |
|---|---|
| `FetLimiterDsp.h` | The whole processor: gain cell, detector, release network, output stage, oversampling. |
| `MathUtils.h` | Fast transcendental approximations it depends on. |

It needs JUCE (JUCE 9 here) for `juce::AudioBuffer`, `juce::dsp::Oversampling`
and the usual `jlimit`/`jmap` helpers. Nothing else.

## Topology

```
in ──► input gain ──► [gain cell: v solved from the divider] ──► Class A amp ──► transformer ──► output gain
                                    ▲                    │
                                    │                    │
                            gate bias ◄── control amp ◄── detector RC ◄── rectifier ◄┘
                                                                        (post-cell, pre-iron)
```

Everything downstream of the rectifier is the sidechain, and the sidechain is
tapped **after** the gain cell. That is what makes it a feedback design, and it
is the structural reason you never program a ratio: the computer slope for a
dialled ratio R is R−1, and the knee softening and ratio stiffening with drive
fall out of the loop gain instead of being drawn by hand.

### The gain cell

A 27k series resistor into a shunt JFET, and nothing else. The LN network makes
the gate follow the drain, `Vgs = Vg0 + a·Vds`, so substituting into the
ohmic-region JFET equation leaves the channel conductance affine in the drain
voltage:

```
g(v) = G0 + k·v,    G0 = 2b(Vg0 − Vp),    k = b(2a − 1)
```

The divider node `(x − v)/R5 = v·g(v)` is then a quadratic in v:

```
R5·k·v² + (1 + R5·G0)·v − x = 0
```

One quadratic per sample, closed form, no iteration. Compression and distortion
come out of the same equation because in the circuit they are the same event.

`k` vanishes at `a = 0.5`, which is exactly what the LN modification does — so
`a` is a physically meaningful revision control rather than a distortion knob.

## Approximations, and why each one is there

This is the honest list. Every one of these is a deliberate trade, not an
oversight, and where a number was chosen by measurement the measurement is
named.

**1. Quadratic (Shichman–Hodges) JFET law rather than a truer device model.**
The FET here works in the ohmic region as a voltage-controlled resistor, which
is where the quadratic form is most defensible. It also runs into a hard limit
on the other side: the part is house-numbered and *selected* (UREI 13-0027,
marked "VVR"), so there is no canonical Idss or Vp and two units differ. A more
exact device equation for a component whose parameters vary unit to unit is
precision without accuracy.

**2. One-sample delay closing the sidechain loop.** The detector sees the
post-cell signal from the previous sample rather than solving cell and detector
simultaneously. The loop closes through the detector RC, which is 20 µs to
1.1 s — hundreds to tens of thousands of samples — so a single sample cannot
matter against the loop's own dynamics. This is the opposite of a ZDF filter,
where the loop is tight and a delay becomes real frequency error. Measured
attack trajectories track the reference within about a dB at every dial
position, which is where a delay this size would have shown up.

**3. Fixed-rate integration with 2× oversampling, not an adaptive-step solver.**
Adaptive stepping controls integration error, not aliasing: samples still have
to come out at the host rate, and a static nonlinearity making harmonics above
Nyquist still folds when you sample it. A solver taking small steps *is*
oversampling under another name. 2× is enough here because the only stiff
nonlinearity is the static gain cell. Policy is 2× below 88.2 kHz and native
at or above it, latency 4 samples / 0.

The one thing that cannot be cheated: a 20 µs attack is a single sample at
48 kHz, which is why running at 1× audibly changes the compression.

**4. Explicit integration of the Jiles–Atherton core, with one linearised
correction.** The field step depends on the slope which depends on the field.
Rather than a Newton loop, the flux step is divided by `(1 + slope)` and the
denominator is clamped where it passes through zero at the loop tips.
Hysteresis must enter as a modification of the *slope* — adding a
direction-dependent offset to the magnetisation *value* and differentiating it
pushes a step through a 3500× differentiator and gives 95–128% THD.

**5. Langevin series expansions below |x| = 0.5.** `L'(x) = 1/x² − coth²(x) + 1`
subtracts two huge nearly-equal floats near zero. At x = 2e-4 both terms are
about 2.5e7 and the true difference is ~0.333, so single precision leaves
roughly 2.5 of noise on a result that should be a third. The literature's 1e-4
guard is fine in double and far too small in float. Three series terms are
exact to about 0.06% at 0.5 and improve rapidly below it.

**6. Polynomial transcendentals instead of libm.** `fastTanh` is
`p(x)/sqrt(p(x)²+1)` with a 5th-order odd polynomial, ~3–4× faster than
`std::tanh`, max error ~0.005%, smooth everywhere. Two traps learned the hard
way: clamp before calling, because unlike `std::tanh` the rational form is not
bounded for huge inputs; and never mix `fastTanh` and `std::tanh` in one signal
path — computing a bias-cancellation reference with one and the audio with the
other leaves a constant −82 dBFS DC offset.

**7. The transformer is a fitted model, not a specific measured core.** This is
the weakest link and worth stating plainly: second harmonic is short by roughly
7–19 dB with a low-frequency slope that a fit cannot justify. Closing it needs
measurements of a real core, not more curve fitting.

**8. Pot tapers and knob calibration are measured, not derived.** The service
documentation does not give the tapers (they are custom T-pad elements), so the
attack, release, input and output laws are interpolated from measured tables.
That means they reproduce observed behaviour rather than faceplate numbers, and
the measured t63 values deliberately do not match the printed ones.

## The known ceiling

The model is calibrated against a reference *plugin*, not against hardware.
That is a real limit and it cannot be argued away: where the reference is ruler
flat from 30 Hz to 16 kHz and this rolls off slightly at both ends, there is no
way from the outside to tell whether the hardware is flat or whether that was a
modelling choice. Renting time on a real unit does not fix it either, since you
only get the front-panel input and output. Internals or nothing.

## Attribution

`MathUtils.h` reimplements approximations from
[chowdsp_math_approx](https://github.com/Chowdhury-DSP/chowdsp_utils) (BSD),
credit to Jatin Chowdhury. The hysteresis in this repository is an independent
anhysteretic Jiles–Atherton implementation and shares no code with chowdsp's
GPL hysteresis module.

Circuit values were read from the UREI 1176LN Rev D service drawing (R-10743,
1970). The service manual itself is not redistributed here.

## Licence

MIT — see [LICENSE](LICENSE). Use it in anything, including commercially. If it
saves you time, a mention is welcome but not required.

Note that JUCE itself is separately licensed; this repository only assumes you
already have it.
