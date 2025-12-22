# Parameter Alias System

## Context

For mix analysis to provide actionable recommendations, we need to send all parameter values for every plugin in the FX chain (except analysis/metering plugins). This requires a parameter alias system similar to the existing plugin alias system.

## Current State

- **Plugin Aliases**: We have a system that maps plugin names to short aliases (e.g., `Serum` → `serum`, `FabFilter Pro-Q 3` → `proq3`)
- **FX Chain Info**: We can read the FX chain and get parameter names/values
- **Problem**: Parameter names vary wildly between plugins and are often cryptic (e.g., `param_23`, `Osc1Vol`, `Filter1_Cutoff_Hz`)

## Proposed Solution

### 1. Parameter Alias Database

Create a mapping system for common parameters across plugins:

```json
{
  "serum": {
    "aliases": {
      "cutoff": ["Filter1_Cutoff_Hz", "Cutoff", "Filter Cutoff"],
      "resonance": ["Filter1_Res", "Resonance", "Q"],
      "volume": ["Master Vol", "Volume", "Output"],
      "attack": ["Env1_Attack", "Attack", "A"],
      "decay": ["Env1_Decay", "Decay", "D"],
      "sustain": ["Env1_Sustain", "Sustain", "S"],
      "release": ["Env1_Release", "Release", "R"]
    }
  },
  "proq3": {
    "aliases": {
      "band1_freq": ["Band 1 Frequency"],
      "band1_gain": ["Band 1 Gain"],
      "band1_q": ["Band 1 Q"]
    }
  }
}
```

### 2. Generic Parameter Categories

For unknown plugins, use heuristic matching:

| Category | Common Names |
|----------|-------------|
| `volume` | vol, volume, level, gain, output, master |
| `cutoff` | cutoff, freq, frequency, filter |
| `resonance` | res, resonance, q, bandwidth |
| `attack` | attack, att, a |
| `decay` | decay, dec, d |
| `sustain` | sustain, sus, s |
| `release` | release, rel, r |
| `mix` | mix, wet, dry/wet, blend |
| `drive` | drive, saturation, distortion |
| `threshold` | threshold, thresh, thr |
| `ratio` | ratio |
| `makeup` | makeup, gain, output |

### 3. @ Command Integration

Extend the existing `@` autocomplete system:

```
@plugin:serum         - Reference plugin
@param:cutoff         - Reference parameter (in selected/last plugin)
@plugin:serum:cutoff  - Reference specific plugin's parameter
```

**Example prompts:**
```
"Make @plugin:serum brighter by increasing @param:cutoff"
"Reduce the @param:attack on the compressor"
"Set @plugin:proq3:band1_gain to -3dB"
```

### 4. State Payload Format

When sending to mix analysis API:

```json
{
  "context": {
    "track_name": "Synth Lead",
    "track_type": "synth",
    "existing_fx": [
      {
        "name": "Serum",
        "alias": "serum",
        "index": 0,
        "enabled": true,
        "params": {
          "cutoff": { "value": 0.75, "raw_name": "Filter1_Cutoff_Hz" },
          "resonance": { "value": 0.3, "raw_name": "Filter1_Res" },
          "volume": { "value": 0.8, "raw_name": "Master Vol" }
        }
      },
      {
        "name": "FabFilter Pro-Q 3",
        "alias": "proq3", 
        "index": 1,
        "enabled": true,
        "params": {
          "band1_freq": { "value": 0.5, "raw_name": "Band 1 Frequency" },
          "band1_gain": { "value": 0.4, "raw_name": "Band 1 Gain" }
        }
      }
    ]
  }
}
```

## Implementation Plan

### Phase 1: Parameter Reading
- [ ] Enumerate all parameters for each FX in chain
- [ ] Read current parameter values (normalized 0-1)
- [ ] Skip analysis/metering plugins (configurable list)

### Phase 2: Alias Generation (API)
- [ ] Extend plugin agent to generate parameter aliases
- [ ] Use LLM to suggest aliases for unknown parameters
- [ ] Cache aliases per plugin type

### Phase 3: Autocomplete Integration
- [ ] Add `@param:` prefix to autocomplete
- [ ] Show parameter suggestions based on selected plugin
- [ ] Support `@plugin:name:param` syntax

### Phase 4: Mix Analysis Integration
- [ ] Include aliased params in analysis request
- [ ] AI can reference params by alias in recommendations
- [ ] Future: Generate actions that modify specific params

## Plugins to Skip (Analysis/Metering)

```
- SPAN
- Insight 2
- Youlean Loudness Meter
- LUFS Meter
- Correlometer
- Phase Meter
- Spectrum Analyzer
```

## Open Questions

1. How to handle plugins with 100+ parameters? (e.g., Kontakt, Omnisphere)
   - Option A: Only send "important" params
   - Option B: Group params by category
   - Option C: Send all, let AI filter

2. Should we cache param aliases locally or always fetch from API?

3. How to handle parameter automation? (values change over time)
   - Option A: Send current value at analysis point
   - Option B: Send min/max/avg over analysis region

## Related Files

- `magda-reaper/src/magda_plugin_scanner.cpp` - Plugin scanning
- `magda-agents-go/agents/plugin/plugin_agent.go` - Plugin alias generation
- `magda-reaper/src/magda_imgui_chat.cpp` - @ autocomplete
- `magda-reaper/src/magda_bounce_workflow.cpp` - FX chain reading



