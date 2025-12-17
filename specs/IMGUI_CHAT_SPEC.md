# MAGDA ImGui Chat Window Specification

## Overview

Replace the current SWELL-based chat window with a ReaImGui implementation to enable:
- `@mention` autocomplete for plugins/instruments/drum kits
- Syntax highlighting in chat input and history
- Better text rendering and streaming display
- Modern, responsive UI

## Dependencies

- **ReaImGui** extension must be installed (available via ReaPack)
- Falls back to existing SWELL chat if ReaImGui not available

---

## 1. Chat Window Features

### 1.1 `@mention` Autocomplete

**Trigger**: User types `@` character

**Behavior**:
1. Detect `@` in input buffer
2. Capture prefix after `@` (e.g., `@addi` → prefix = `addi`)
3. Filter suggestions based on prefix (fuzzy match)
4. Show popup below cursor with matching suggestions
5. Navigate with ↑/↓ arrows, select with Tab/Enter
6. Insert completed `@alias` and close popup

**Suggestion Sources**:
- Plugin aliases (from scanned plugins + user aliases)
- Drum kit presets (`@addictivedrums`, `@superior`, `@ezdrummer`)
- User-defined custom mappings

**Data Structure**:
```
AutocompleteSuggestion {
    alias: string       // "@addictivedrums" (what user types)
    display_name: string // "Addictive Drums 2" (shown in popup)
    type: string        // "drums" | "synth" | "fx" | "sampler"
    icon: string        // "🥁" | "🎹" | "🎸" | etc.
    mapping_id: string? // For drums: references drum mapping
}
```

### 1.2 Syntax Highlighting

**In Input Field**:
- `@mentions` rendered in accent color (blue)
- Regular text in default color

**In Chat History**:
- User messages: default color, right-aligned or prefixed
- Assistant messages: slightly different color
- Code blocks (DSL output): monospace, syntax colored
- Errors: red text

### 1.3 Chat History Display

- Scrollable region with message bubbles/blocks
- Auto-scroll to bottom on new messages
- Support for streaming text (character-by-character append)
- Copy message on click or context menu

### 1.4 Input Field

- Multi-line capable (Shift+Enter for newline, Enter to send)
- Character limit: 4096
- Placeholder text: "Ask MAGDA anything... (use @ for plugins)"

---

## 2. API Integration

### 2.1 Autocomplete Endpoint

```
GET /api/v1/autocomplete?prefix={prefix}&type={type}

Query params:
  - prefix: string (e.g., "addi")
  - type: "plugin" | "drumkit" | "all" (default: "all")

Response:
{
  "suggestions": [
    {
      "alias": "addictivedrums",
      "display_name": "Addictive Drums 2",
      "type": "drums",
      "icon": "🥁",
      "mapping_id": "addictive_drums_v2"
    }
  ]
}
```

### 2.2 User Preferences Endpoints

```
GET /api/v1/preferences
Response: { drum_kit: string, default_instrument: string, ... }

PUT /api/v1/preferences
Body: { drum_kit: "addictive_drums_v2", ... }

GET /api/v1/mappings/{mapping_id}
Response: { id, name, notes: { kick: 36, snare: 38, ... } }

GET /api/v1/mappings
Response: [{ id, name, type, is_preset }]
```

### 2.3 Context Injection

When user message contains `@mention`:
1. Parse mentions from message before sending
2. Resolve each mention to its full context
3. Inject context into system prompt or message metadata

Example:
```
User input: "add a funky break to @addictivedrums"

Resolved context:
{
  "plugins_referenced": [
    {
      "alias": "addictivedrums",
      "plugin_name": "Addictive Drums 2",
      "type": "drums",
      "mapping": { "kick": 36, "snare": 38, "hi_hat": 42, ... }
    }
  ]
}

LLM receives: "User wants drums using Addictive Drums 2.
Use canonical drum names. Mapping will convert to MIDI."
```

---

## 3. ImGui Implementation Details

### 3.1 Function Pointers Required

```cpp
// Context management
ImGui_CreateContext
ImGui_DestroyContext
ImGui_Attach           // Attach to HWND for rendering

// Window
ImGui_Begin
ImGui_End
ImGui_SetNextWindowSize
ImGui_SetNextWindowPos

// Text display
ImGui_Text
ImGui_TextColored
ImGui_TextWrapped

// Input
ImGui_InputText
ImGui_InputTextMultiline

// Popup/Autocomplete
ImGui_BeginPopup
ImGui_EndPopup
ImGui_OpenPopup
ImGui_Selectable
ImGui_SetKeyboardFocusHere

// Layout
ImGui_SameLine
ImGui_Separator
ImGui_BeginChild      // For scrollable chat history
ImGui_EndChild

// Interaction
ImGui_Button
ImGui_IsKeyPressed
ImGui_GetCursorPos
```

### 3.2 Rendering Loop

```
1. Check if context exists, create if needed
2. ImGui_Begin("MAGDA Chat", ...)
3. Render chat history (scrollable child region)
4. Render input area
   - InputTextMultiline with callback for @ detection
   - If @ detected and prefix exists, show autocomplete popup
5. Render send button
6. ImGui_End()
```

### 3.3 Autocomplete Popup Positioning

- Position popup directly below the `@` character in input
- Calculate position based on:
  - Input field position
  - Character offset to `@` symbol
  - Font metrics for accurate placement

---

## 4. Fallback Behavior

If ReaImGui is not installed:
1. Detect missing `ImGui_CreateContext` function pointer
2. Use existing SWELL-based chat window
3. `@mentions` typed but no autocomplete (just plain text)
4. Show one-time prompt: "Install ReaImGui for enhanced features"

---

## 5. Files to Create

```
include/
  magda_imgui_chat.h
  magda_imgui_common.h      // Shared ImGui utilities

src/
  magda_imgui_chat.cpp
  magda_imgui_common.cpp
```

---

## 6. Migration Plan

### Phase 1: Parallel Implementation
- Build ImGui chat alongside SWELL chat
- Feature flag to switch between them
- Test thoroughly

### Phase 2: Default Switch
- Make ImGui chat the default (if ReaImGui installed)
- SWELL chat as fallback only

### Phase 3: Cleanup (optional)
- Remove SWELL chat code if ReaImGui adoption is high
- Or keep indefinitely for compatibility

---

## 7. Future Extensions

- **Plugins Window**: Rebuild plugin list with ImGui for better search/filter
- **Preferences Window**: Drum kit editor, alias management
- **Themes**: Dark/light mode, custom colors
- **Docking**: Allow chat to dock in Reaper's docker system
