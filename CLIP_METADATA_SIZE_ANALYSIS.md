# Clip Metadata Size Analysis

## Current Clip Data (Minimal)
```json
{
  "index": 0,
  "position": 0.0,
  "length": 4.0,
  "selected": false,
  "name": "Clip 1"
}
```
**Size**: ~80 bytes per clip

## Potential Additions

### 1. MIDI Note Data
```json
{
  "midi_notes": [
    {"note": 60, "velocity": 100, "start": 0.0, "length": 0.5},
    {"note": 64, "velocity": 90, "start": 0.5, "length": 0.5},
    // ... 100+ notes for a complex clip
  ]
}
```
**Size**: ~50-100 bytes per note
- Simple clip (10 notes): +500 bytes
- Complex clip (100 notes): +5KB
- Very complex (500 notes): +25KB

**Impact**: With 10 clips × 100 notes = 50KB just for MIDI data

### 2. Audio Analysis Metadata
```json
{
  "audio_analysis": {
    "rms": -12.5,
    "peak": -3.2,
    "spectral_centroid": 1200.5,
    "zero_crossings": 450,
    "tempo": 120.0,
    "key": "C major"
  }
}
```
**Size**: ~200 bytes per clip

### 3. Take Information
```json
{
  "takes": [
    {
      "index": 0,
      "name": "Take 1",
      "source": "audio_file.wav",
      "channels": 2,
      "sample_rate": 44100
    }
  ],
  "active_take": 0
}
```
**Size**: ~150 bytes per clip

### 4. FX Chain (Take-level)
```json
{
  "take_fx": [
    {"name": "ReaEQ", "enabled": true},
    {"name": "ReaComp", "enabled": true}
  ]
}
```
**Size**: ~50 bytes per FX × N FX

### 5. Automation Envelopes
```json
{
  "envelopes": [
    {
      "type": "volume",
      "points": [
        {"time": 0.0, "value": 0.5},
        {"time": 2.0, "value": 1.0}
      ]
    }
  ]
}
```
**Size**: ~30 bytes per point × N points

## Size Scenarios

### Scenario 1: Minimal (Current)
- 10 tracks × 5 clips = 50 clips
- 50 clips × 80 bytes = **4KB**
- ✅ Very manageable

### Scenario 2: With MIDI Data
- 10 tracks × 5 clips = 50 clips
- 50 clips × (80 + 5000 bytes avg) = **254KB**
- ⚠️ Getting large, but manageable

### Scenario 3: Full Metadata
- 10 tracks × 5 clips = 50 clips
- 50 clips × (80 + 5000 + 200 + 150 + 100) = **276KB**
- ⚠️ Large, but still manageable

### Scenario 4: Complex Project
- 20 tracks × 20 clips = 400 clips
- 400 clips × 5500 bytes = **2.2MB**
- ❌ Too large for LLM context

## Filtering Impact

### Without Filtering (All Mode)
- 400 clips × 5500 bytes = **2.2MB** (~550K tokens)
- ❌ Expensive and slow

### With Selected Tracks Only (2 tracks selected)
- 2 tracks × 20 clips = 40 clips
- 40 clips × 5500 bytes = **220KB** (~55K tokens)
- ✅ 90% reduction

### With Selected Track Clips Only (2 tracks, 5 clips selected)
- 2 tracks × 5 clips = 10 clips
- 10 clips × 5500 bytes = **55KB** (~14K tokens)
- ✅ 97.5% reduction

## Recommendations

1. **Make metadata optional** - Add flags for what to include:
   ```cpp
   struct ClipMetadataFlags {
     bool includeMidi = false;
     bool includeAudioAnalysis = false;
     bool includeTakes = false;
     bool includeFX = false;
     bool includeAutomation = false;
   };
   ```

2. **Use filtering preferences** - Always respect user's filter mode

3. **Limit MIDI notes** - If including MIDI, limit to:
   - First N notes (e.g., 100)
   - Or notes in first N bars
   - Or notes in time selection

4. **Lazy loading** - For very large projects, consider:
   - Function calling to get clip details on-demand
   - Only include metadata for selected clips
   - Cache analysis results

5. **Compression** - For MIDI data:
   - Use more compact format
   - Delta encoding for timing
   - Group notes by time

## Implementation Priority

1. ✅ **Basic clip info** (current) - Always include
2. ⏳ **MIDI data** - Optional, filtered by preferences
3. ⏳ **Audio analysis** - Optional, only for selected clips
4. ⏳ **Take info** - Optional, minimal
5. ⏳ **FX/Automation** - On-demand via function calling

## Example: Smart MIDI Inclusion

```cpp
// Only include MIDI for selected clips, limit to 100 notes
if (clipSelected && prefs.includeMidiData) {
  // Get MIDI notes, limit to 100
  int noteCount = GetMIDINoteCount(item);
  int notesToInclude = noteCount > 100 ? 100 : noteCount;
  // Include notes...
}
```

This keeps state small while allowing detailed data when needed.
