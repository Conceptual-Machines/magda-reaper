# MAGDA Reaper Integration Design

## Overview

This document outlines the design for integrating MAGDA (Multi Agent Generic DAW API) with Reaper, providing natural language control over DAW operations through a Dear ImGui console interface.

## Architecture

### High-Level Flow
```
User Input → Dear ImGui Console → Reaper Context → MAGDA API → OpenAI → DAW Commands → Reaper Execution
```

### Components
1. **Dear ImGui Console**: User interface for natural language input
2. **Context Manager**: Extracts and manages Reaper project state
3. **MAGDA Client**: Communicates with MAGDA API
4. **Command Executor**: Translates MAGDA responses to Reaper actions
5. **Event System**: Handles Reaper events for context updates

## Context Management

### System Context (Static)
- **Audio Configuration**: Driver, sample rate, buffer size
- **Plugin Database**: Available VST/AU plugins from Reaper config files
- **Reaper Settings**: Version, theme, language preferences

**Sources:**
- `reaper-vstplugins64.ini` - VST plugin database
- `reaper.ini` - Audio and general settings
- `reaper-fxfolders.ini` - Effect categories

### Project Context (Dynamic)
- **Project Metadata**: BPM, key, time signature, sample rate
- **Track Information**: Names, types, routing, colors
- **Selection State**: Currently selected tracks, clips, regions
- **Timeline**: Playhead position, loop points, markers

### Selection Context (Real-time)
- **Selected Tracks**: Names, GUIDs, current parameters
- **Selected Items**: Clip names, positions, lengths
- **Active Effects**: Currently selected plugins and parameters

## GUID System

### Reaper GUIDs
- **Track GUIDs**: Stable across sessions, stored in project file
- **Item GUIDs**: Stable across sessions, for clips/regions
- **Effect GUIDs**: Stable for plugin instances

### Context Structure with GUIDs
```json
{
  "selection": {
    "tracks": [
      {
        "guid": "12345678-1234-1234-1234-123456789abc",
        "name": "Bass",
        "reaper_id": 0,
        "volume": -6.0,
        "muted": false,
        "type": "audio"
      }
    ],
    "items": [
      {
        "guid": "87654321-4321-4321-4321-cba987654321",
        "name": "bass_verse_1",
        "track_guid": "12345678-1234-1234-1234-123456789abc",
        "position": 120.5,
        "length": 8.0
      }
    ]
  },
  "project": {
    "bpm": 120,
    "key": "C major",
    "sample_rate": 44100
  },
  "system": {
    "plugins": {
      "synths": ["Serum", "Kontakt"],
      "eq": ["FabFilter Pro-Q 3"],
      "compression": ["Pro-C 2"]
    },
    "audio": {
      "driver": "ASIO",
      "device": "Focusrite USB ASIO"
    }
  }
}
```

## Event-Driven Context Updates

### Event Filtering
**High Priority Events (Always Update):**
- Track selection changed
- Item selection changed
- Track added/removed
- Project loaded
- Effect added/removed

**Medium Priority Events (Debounced):**
- Parameter changes (volume, pan, etc.)
- Effect parameter changes
- Timeline position changes

**Low Priority Events (Ignore):**
- Playback position during play
- Frequent UI updates
- Temporary selections

### Debouncing Strategy
- **Update Timer**: 0.5 seconds minimum between context updates
- **Context Diffing**: Only send context if it actually changed
- **Batch Updates**: Group multiple rapid changes into single update

## User Interface Design

### Dear ImGui Console Layout
```
┌─────────────────────────────────────┐
│ MAGDA Console                       │
├─────────────────────────────────────┤
│ [Input: "make the bass louder"]     │
│                                     │
│ ┌─────────────────────────────────┐ │
│ │ Selected: Bass Track            │ │
│ │ Context: Volume: -6dB, FX: 2   │ │
│ └─────────────────────────────────┘ │
│                                     │
│ [Send] [Clear] [History]            │
├─────────────────────────────────────┤
│ Response: "Set Bass track volume   │
│ to -3dB"                           │
│                                     │
│ [Execute] [Undo]                    │
└─────────────────────────────────────┘
```

### Console Features
- **Text Input**: Natural language command entry
- **Context Display**: Shows current selection and relevant info
- **Command History**: Previous commands and responses
- **Execute/Undo**: Control over command execution
- **Settings**: Context update preferences

## Implementation Phases

### Phase 1: Basic Integration (Python Prototype)
**Goals:**
- Basic Dear ImGui console in Reaper
- Simple context extraction (selected tracks only)
- MAGDA API communication
- Basic command execution

**Deliverables:**
- Python script with Dear ImGui interface
- Context extraction from Reaper API
- Basic MAGDA client integration
- Simple command mapping (volume, mute, etc.)

### Phase 2: Enhanced Context (Python)
**Goals:**
- Full context extraction (tracks, items, effects)
- System context (plugins, audio settings)
- Event-driven context updates
- Better command mapping

**Deliverables:**
- Complete context manager
- Plugin database integration
- Event system for context updates
- Expanded command set

### Phase 3: C++ Library Integration
**Goals:**
- Port to C++ for performance
- Native Reaper extension
- Full Dear ImGui integration
- Production-ready stability

**Deliverables:**
- C++ library with same API as Python
- Native Reaper extension
- Performance optimizations
- Error handling and recovery

### Phase 4: Advanced Features
**Goals:**
- Undo/redo system
- Command chaining
- Batch operations
- Advanced context awareness

**Deliverables:**
- Undo/redo for all operations
- Multi-step command support
- Context-aware suggestions
- Performance monitoring

## Technical Considerations

### Performance
- **Context Updates**: Debounced to avoid API spam
- **GUID Lookups**: Cached for performance
- **Plugin Database**: Parsed once at startup
- **Memory Usage**: Minimal context storage

### Error Handling
- **API Failures**: Graceful degradation
- **Invalid Commands**: User feedback and suggestions
- **Context Mismatch**: Re-sync when needed
- **Network Issues**: Offline mode considerations

### Security
- **API Keys**: Secure storage in Reaper config
- **Context Privacy**: User control over what's shared
- **Local Processing**: Sensitive data stays local

## Success Metrics

### User Experience
- **Response Time**: < 2 seconds for simple commands
- **Accuracy**: > 90% command interpretation success
- **Usability**: Intuitive natural language interface
- **Reliability**: Stable across Reaper sessions

### Technical Performance
- **Memory Usage**: < 50MB additional RAM
- **CPU Impact**: < 5% additional CPU usage
- **Context Updates**: < 100ms for context changes
- **API Calls**: < 10 calls per minute during normal use

## Future Considerations

### Extensibility
- **Plugin Support**: Custom MAGDA agents for specific plugins
- **Workflow Integration**: Integration with Reaper actions
- **Scripting**: Python script support for custom commands
- **API Evolution**: Backward compatibility with MAGDA updates

### Integration Possibilities
- **Other DAWs**: Architecture designed for portability
- **Cloud Services**: Optional cloud-based processing
- **Collaboration**: Multi-user context sharing
- **Learning**: User preference learning over time

## Voice Control Integration

### macOS Speech Recognition
**Feasibility Assessment:**
- **NSSpeechRecognizer**: Native macOS speech recognition API
- **Availability**: macOS 10.15+ (Catalina and later)
- **Accuracy**: High accuracy for English, good for other languages
- **Latency**: ~200-500ms recognition delay
- **Offline**: Works without internet connection
- **Privacy**: All processing done locally on device

**Technical Implementation:**
```objc
// Objective-C++ integration with Reaper C++ extension
#import <Speech/Speech.h>

@interface VoiceController : NSObject
@property (nonatomic, strong) SFSpeechRecognizer *recognizer;
@property (nonatomic, strong) SFSpeechAudioBufferRecognitionRequest *request;
@property (nonatomic, strong) SFSpeechRecognitionTask *task;
@end
```

**Integration Strategy:**
1. **C++ Extension**: Use Objective-C++ bridge to access NSSpeechRecognizer
2. **Audio Routing**: Capture system audio or microphone input
3. **Command Processing**: Route recognized speech to MAGDA console
4. **Feedback**: Visual/audio confirmation of recognized commands

### Voice Control Features

**Command Recognition:**
- **Natural Language**: "Make the bass track louder"
- **Short Commands**: "Volume up", "Add reverb", "Mute drums"
- **Contextual**: "This track" (referring to selected track)
- **Multi-step**: "Create a new track and add Serum"

**Voice Feedback:**
- **Confirmation**: "Setting bass volume to -3dB"
- **Error Handling**: "I didn't understand that command"
- **Suggestions**: "Did you mean 'add compressor'?"

**User Experience:**
- **Push-to-Talk**: Hotkey to activate voice recognition
- **Continuous Listening**: Optional always-on mode
- **Voice Training**: Adapt to user's accent and vocabulary
- **Noise Handling**: Background noise filtering

### Implementation Phases

**Phase 1: Basic Voice Recognition**
- **Goals**: Basic speech-to-text integration
- **Deliverables**: Voice input to text console
- **Limitations**: English only, basic command set

**Phase 2: Enhanced Recognition**
- **Goals**: Multi-language support, noise handling
- **Deliverables**: Robust voice recognition with feedback
- **Features**: Voice training, custom vocabulary

**Phase 3: Advanced Voice Control**
- **Goals**: Context-aware voice commands
- **Deliverables**: Intelligent voice assistant
- **Features**: Natural conversation, learning capabilities

### Technical Considerations

**Performance:**
- **Recognition Latency**: Target < 500ms for good UX
- **CPU Usage**: Speech recognition can be CPU intensive
- **Memory**: Audio buffer management for continuous listening
- **Battery**: Impact on laptop battery life

**Privacy & Security:**
- **Local Processing**: All speech processing on device
- **No Cloud Storage**: Voice data never leaves the computer
- **User Control**: Enable/disable voice features
- **Data Retention**: No persistent storage of voice data

**Cross-Platform Considerations:**
- **macOS**: NSSpeechRecognizer (native, high quality)
- **Windows**: Windows Speech Recognition API
- **Linux**: Speech recognition libraries (CMU Sphinx, etc.)
- **Fallback**: Cloud-based services for unsupported platforms

### Voice Command Examples

**Track Management:**
- "Create a new audio track"
- "Rename this track to 'Lead Guitar'"
- "Delete the selected track"
- "Duplicate the bass track"

**Mixing Commands:**
- "Set volume to -6dB"
- "Mute the drums"
- "Pan left 30%"
- "Add compressor to this track"

**Effect Commands:**
- "Add reverb with 2 second decay"
- "Remove the EQ from track 3"
- "Set compressor ratio to 4:1"
- "Bypass all effects on this track"

**Project Commands:**
- "Save the project"
- "Set BPM to 120"
- "Create a new marker at current position"
- "Undo the last action"

### Future Enhancements

**AI-Powered Voice Assistant:**
- **Context Awareness**: "Make it sound more aggressive"
- **Learning**: Adapt to user's mixing style and preferences
- **Suggestions**: "Try adding some compression to the bass"
- **Conversation**: Natural back-and-forth about mixing decisions

**Multi-Modal Interaction:**
- **Voice + Mouse**: "Select this track and add reverb"
- **Voice + Keyboard**: "Create a new track and press record"
- **Voice + Gestures**: "Make this louder" while pointing

**Collaborative Voice Control:**
- **Multi-User**: Different voices for different users
- **Role-Based**: Different command sets for different roles
- **Remote Control**: Voice control over network sessions

## Questions and Decisions Needed

1. **GUID Stability**: Verify Reaper GUID persistence across sessions
2. **Context Scope**: Define minimum viable context for MVP
3. **Performance Budget**: Acceptable latency and resource usage
4. **User Control**: Level of user control over context sharing
5. **Error Recovery**: Strategy for handling context mismatches
6. **Undo System**: Integration with Reaper's undo system
7. **Plugin Support**: Which plugins to prioritize for integration
8. **Testing Strategy**: How to validate the integration works correctly

## Next Steps

1. **Verify Reaper GUID stability** with test script
2. **Create Python prototype** with basic console
3. **Test context extraction** with real Reaper projects
4. **Validate MAGDA API integration** with context
5. **Design command mapping** for common operations
6. **Plan C++ library structure** for final implementation
