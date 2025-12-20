# Plan: Unified `@` Modifier System with Mix Analysis

## Overview
Consolidate the plugin selector and mix analysis into a unified `@` modifier system in the main chat window, eliminating the separate mix analysis dialog.

---

## Phase 1: Refactor `@` Autocomplete to Support Namespaces

### 1.1 Update Autocomplete Data Structure
- [ ] Create `AutocompleteCategory` enum: `PLUGIN`, `MIX`, `TRACK` (future)
- [ ] Create `AutocompleteItem` struct with `category`, `namespace`, `value`, `displayName`
- [ ] Populate with:
  - Plugins: `@plugin:serum`, `@plugin:massive`, etc.
  - Mix types: `@mix:synth`, `@mix:drums`, `@mix:bass`, `@mix:vocals`, `@mix:guitar`, `@mix:piano`, `@mix:strings`, `@mix:fx`

### 1.2 Update Autocomplete UI
- [ ] Group items by category with headers
- [ ] Show "Use custom: [text]" option when typing non-matching text after `@mix:`
- [ ] Filter across all categories as user types
- [ ] Support shortcuts: `@serum` → `@plugin:serum`, `@drums` → `@mix:drums`

### 1.3 Update Parser
- [ ] Parse `@namespace:value` format
- [ ] Handle shortcuts (no namespace = try plugin first, then mix)
- [ ] Extract namespace and value for different handlers

---

## Phase 2: Integrate Mix Analysis into Chat Flow

### 2.1 Handle `@mix:tracktype` in Chat
- [ ] Detect `@mix:` prefix in message
- [ ] Extract track type from modifier
- [ ] Check if track is selected, show error in chat if not
- [ ] Run mix analysis workflow (existing code)
- [ ] Display results in chat window (not console)

### 2.2 Update Message Processing
- [ ] Before sending to API, check for `@mix:` prefix
- [ ] If present:
  1. Run DSP analysis on selected track
  2. Append analysis data to message context
  3. Send to `/api/v1/chat` with analysis in state
- [ ] Display AI response in chat

### 2.3 Chat UI Updates
- [ ] Show "Analyzing track..." status in chat while processing
- [ ] Display results inline in chat conversation
- [ ] Handle errors gracefully (show in chat, not popup)

---

## Phase 3: Remove Separate Mix Analysis Dialog

### 3.1 Deprecate Old Dialog
- [ ] Remove `MagdaImGuiMixAnalysisDialog` class
- [ ] Remove mix analysis menu item (or repurpose to open chat with `@mix:` prefilled)
- [ ] Clean up related code in `main.cpp`

### 3.2 Update Menu
- [ ] Option A: Remove "Mix Analysis..." menu item entirely
- [ ] Option B: Keep menu item but have it focus chat window and insert `@mix:`

---

## Phase 4: Future Improvements (Later)

### 4.1 Smart Track Type Detection
- [ ] Parse track name for keywords (synth, drum, bass, etc.)
- [ ] Get preset names from plugins on track
- [ ] Suggest track type based on plugin types (Serum → synth)
- [ ] Show smart suggestions in autocomplete

### 4.2 Additional Modifiers
- [ ] `@track:N` - Select track N
- [ ] `@fx:type` - Add effect
- [ ] `@preset:name` - Load preset

---

## Files to Modify

| File | Changes |
|------|---------|
| `magda_imgui_chat.cpp` | Autocomplete categories, `@mix:` parsing, result display |
| `magda_imgui_chat.h` | New data structures for autocomplete |
| `magda_bounce_workflow.cpp` | Expose workflow for chat integration |
| `main.cpp` | Remove/update mix analysis menu item |
| `magda_imgui_mix_analysis_dialog.cpp/h` | Delete (Phase 3) |

---

## Syntax Examples

```
# Plugin insertion (existing, updated format)
@plugin:serum
@plugin:fabfilter-pro-q
@serum                      # shorthand for @plugin:serum

# Mix analysis (new)
@mix:synth make it cut through
@mix:drums more punch
@mix:bass tighten the low end
@mix:vocals more presence
@mix:theremin               # custom track type - user can type anything

# Future
@track:2                    # select track 2
@fx:reverb                  # add reverb
```

---

## Autocomplete UI Mockup

```
User types: @

┌─────────────────────────────────┐
│ ── Plugins ──                   │
│   @plugin:serum                 │
│   @plugin:massive               │
│   @plugin:fabfilter-pro-q       │
│   ... (more plugins)            │
│ ── Mix Analysis ──              │
│   @mix:drums                    │
│   @mix:bass                     │
│   @mix:synth                    │
│   @mix:vocals                   │
│   @mix:guitar                   │
│   @mix:piano                    │
│   @mix:strings                  │
│   @mix:fx                       │
└─────────────────────────────────┘

User types: @mix:ther

┌─────────────────────────────────┐
│ ✓ Use "theremin"                │
│ ── No matches ──                │
└─────────────────────────────────┘
```

---

## Estimated Effort

| Phase | Effort | Priority |
|-------|--------|----------|
| Phase 1 | 2-3 hours | High |
| Phase 2 | 2-3 hours | High |
| Phase 3 | 30 min | Medium |
| Phase 4 | TBD | Low (future) |

---

## Open Questions

1. Should `@serum` default to plugin or require `@plugin:serum`?
   - Recommendation: Default to plugin for backward compatibility

2. Show mix results in chat as text, or formatted card/panel?
   - Recommendation: Start with text, enhance later

3. Keep "Mix Analysis" in menu, or remove entirely?
   - Recommendation: Keep but have it open chat with `@mix:` prefilled

