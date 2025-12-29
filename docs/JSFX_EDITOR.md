# JSFX AI Editor

A Cursor-style code editor for JSFX development with integrated AI assistance.

## Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│  MAGDA JSFX Editor                                              [─][□][×]│
├────────────┬────────────────────────────────────┬───────────────────────┤
│ FILES      │ my_compressor.jsfx  │ delay.jsfx × │  AI ASSISTANT         │
│            ├────────────────────────────────────┤                       │
│ 📁 Effects │  1│ desc:My Compressor             │  ┌─────────────────┐  │
│   📄 comp  │  2│ slider1:0<-60,0,1>Threshold    │  │ How can I help? │  │
│   📄 delay │  3│ slider2:4<1,20,0.1>Ratio       │  └─────────────────┘  │
│   📄 reverb│  4│                                │                       │
│ 📁 My FX   │  5│ @init                          │  You: Add soft knee   │
│   📄 my_co▶│  6│ threshold = 10^(slider1/20);   │                       │
│            │  7│ ratio = slider2;               │  AI: Here's soft knee │
│            │  8│                                │  implementation:      │
│            │  9│ @slider                        │  ```                  │
│            │ 10│ threshold = 10^(slider1/20);   │  knee = 0.5;          │
│            │ 11│                                │  ...                  │
│            │ 12│ @sample                        │  ```                  │
│            │ 13│ input = spl0;                  │  [Apply] [Copy]       │
│            │ 14│ // compression logic           │                       │
│            │ 15│                                │  ───────────────────  │
│            │                                    │  [Type message...]    │
├────────────┴────────────────────────────────────┴───────────────────────┤
│ [New File] [Save] [Save As] [Test on Track]          Line 14, Col 5     │
└─────────────────────────────────────────────────────────────────────────┘
```

## Components

### 1. File Browser (Left Panel)
- Browse REAPER's Effects folder (`~/.config/REAPER/Effects` or platform equivalent)
- Tree view with folders
- Icons for files/folders
- Right-click context menu (New, Delete, Rename)
- Click to open in editor

### 2. Code Editor (Center Panel)
- **Tabs**: Multiple files open simultaneously
- **Line Numbers**: Gutter with line numbers
- **Syntax Highlighting**: JSFX keywords (@init, @slider, @sample, @block, @gfx)
- **Text Editing**: Multi-line input with scroll
- **Status Bar**: Current line/column, file modified indicator

### 3. AI Chat (Right Panel)
- Chat history with user/assistant messages
- Context-aware: sends current file content to AI
- Code blocks in responses with [Apply] button
- Streaming responses

### 4. Toolbar (Bottom)
- New File
- Save / Save As
- Test on Track (adds JSFX to selected track for live testing)
- Undo/Redo

## Implementation Phases

### Phase 1: Basic Editor Window
- [ ] Create `MagdaJSFXEditor` class
- [ ] ImGui window with 3-panel layout
- [ ] Basic text input for code editing
- [ ] File browser showing Effects folder
- [ ] Open/Save functionality

### Phase 2: Enhanced Editor
- [ ] Tab system for multiple files
- [ ] Line numbers
- [ ] Basic syntax highlighting (color keywords)
- [ ] Undo/redo stack

### Phase 3: AI Integration
- [ ] Chat panel with history
- [ ] Send code context to AI
- [ ] Parse code blocks from AI response
- [ ] [Apply] button to insert/replace code

### Phase 4: REAPER Integration
- [ ] "Test on Track" - add JSFX to selected track
- [ ] Hot reload on save
- [ ] Error display from JSFX compilation

## JSFX Syntax Highlighting

Keywords to highlight:
- Sections: `@init`, `@slider`, `@block`, `@sample`, `@gfx`, `@serialize`
- Declarations: `desc:`, `slider1:`, `in_pin:`, `out_pin:`, `options:`
- Functions: `sin`, `cos`, `sqrt`, `log`, `exp`, `min`, `max`, `floor`, `ceil`
- Variables: `spl0`-`spl63`, `samplesblock`, `srate`, `num_ch`, `pdc_delay`
- Operators: `+`, `-`, `*`, `/`, `%`, `^`, `|`, `&`, `~`
- Memory: `memset`, `memcpy`, `freembuf`, `__memtop`

## File Locations

- **macOS**: `~/Library/Application Support/REAPER/Effects/`
- **Windows**: `%APPDATA%\REAPER\Effects\`
- **Linux**: `~/.config/REAPER/Effects/`

## API Endpoint

New endpoint needed: `POST /api/v1/jsfx/assist`

```json
{
  "code": "current file content",
  "filename": "my_effect.jsfx",
  "message": "Add soft knee compression",
  "history": [...]
}
```

Response:
```json
{
  "response": "Here's how to add soft knee...",
  "code_blocks": [
    {
      "code": "knee = slider3;\n...",
      "action": "replace",  // or "insert"
      "start_line": 10,
      "end_line": 15
    }
  ]
}
```
