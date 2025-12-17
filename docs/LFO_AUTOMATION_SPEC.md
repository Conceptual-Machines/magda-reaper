# LFO Automation Specification

## Overview
Enhance the automation system to support proper LFO (Low Frequency Oscillator) functionality with intuitive parameters.

## Current State
- Basic sine/saw/square curves implemented
- Sine now calculates in dB space for visual symmetry
- Parameters: `curve`, `freq`, `amplitude`, `phase`, `from`, `to`

## Proposed LFO Parameters

### Core Parameters
| Parameter | Description | Default | Example |
|-----------|-------------|---------|---------|
| `shape` | Waveform type | `sine` | `sine`, `saw`, `square`, `triangle` |
| `rate` | Speed in Hz OR sync to tempo | `1` | `0.5` (Hz), `1/4` (quarter note), `1bar` |
| `depth` | How much the LFO affects the parameter (0-100%) | `100` | `50` = half the range |
| `min` | Minimum value (in natural units for param) | `0` | `-inf` dB, `0` linear |
| `max` | Maximum value | `1` | `0` dB, `1` linear |
| `phase` | Starting phase (0-360° or 0-1) | `0` | `90` = start at peak |
| `center` | Center point of oscillation | auto | Calculated from min/max |

### Tempo Sync Options
```
rate: "1/4"     → quarter note
rate: "1/8"     → eighth note
rate: "1/16"    → sixteenth note
rate: "1bar"    → one bar
rate: "2bars"   → two bars
rate: "4bars"   → four bars
rate: 0.5       → 0.5 Hz (free running)
```

### Example DSL Calls
```
// Simple tremolo effect (volume wobble)
lfo(track: 0, param: "volume", shape: "sine", rate: "1/8", depth: 50)

// Auto-pan effect
lfo(track: 0, param: "pan", shape: "triangle", rate: "1/4", min: -1, max: 1)

// Slow filter sweep
lfo(track: 0, param: "Filter Cutoff", shape: "saw", rate: "2bars", min: 200, max: 8000)

// Fast vibrato-style (free Hz)
lfo(track: 0, param: "pitch", shape: "sine", rate: 5.0, depth: 10)
```

## Visual Symmetry
- Volume/gain parameters: Calculate in dB space for symmetric display
- Pan: Linear (-1 to 1), already symmetric
- Other params: Linear by default, option for log scaling

## Implementation Notes

### Rate Calculation
```cpp
double getSecondsPerCycle(const char* rate, double bpm) {
    if (isNumeric(rate)) {
        // Free Hz mode
        return 1.0 / atof(rate);
    }
    // Tempo sync mode
    double beats_per_second = bpm / 60.0;
    if (rate == "1/4") return 1.0 / beats_per_second;
    if (rate == "1/8") return 0.5 / beats_per_second;
    if (rate == "1bar") return 4.0 / beats_per_second;  // Assuming 4/4
    // etc.
}
```

### Point Density
- For smooth curves: ~32 points per cycle minimum
- For tempo-synced: Align points to beat grid when possible
- Cap at reasonable max (256 points) to avoid performance issues

## Future Enhancements
- [ ] Multiple LFOs on same parameter (layering)
- [ ] LFO modulating LFO rate (meta-modulation)
- [ ] Envelope followers
- [ ] Random/S&H (sample and hold) waveform
- [ ] Smoothing/slew rate limiting
- [ ] Bipolar vs unipolar modes

## Priority
Medium - Current sine/fade implementation works for basic use cases. LFO enhancements improve creative possibilities but not blocking.
