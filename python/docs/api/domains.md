# Domains API Reference

Domain-specific implementations in MAGDA.

## DAW Domain

The Digital Audio Workstation domain for audio production automation.

### DAWFactory

Factory for creating DAW-specific components.

```python
from magda import DAWFactory

factory = DAWFactory()

# Create components
context = factory.create_context()
orchestrator = factory.create_orchestrator()
agents = factory.create_agents()
pipeline = factory.create_pipeline()
```

### DAW Agents

#### DAWTrackAgent

Handles track creation and management operations.

```python
from magda.domains.daw import DAWTrackAgent

agent = DAWTrackAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['create track', 'add track', 'rename track', 'delete track']

# Execute operation
result = agent.execute("create track bass", context)
```

#### DAWVolumeAgent

Handles volume control operations.

```python
from magda.domains.daw import DAWVolumeAgent

agent = DAWVolumeAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['set volume', 'adjust volume', 'mute', 'unmute', 'pan']

# Execute operation
result = agent.execute("set volume -6db", context)
```

#### DAWEffectAgent

Handles effect processing operations.

```python
from magda.domains.daw import DAWEffectAgent

agent = DAWEffectAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['add effect', 'compression', 'reverb', 'eq']

# Execute operation
result = agent.execute("add reverb", context)
```

#### DAWClipAgent

Handles clip management operations.

```python
from magda.domains.daw import DAWClipAgent

agent = DAWClipAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['create clip', 'move clip', 'duplicate clip', 'split clip']

# Execute operation
result = agent.execute("create 4 bar clip", context)
```

#### DAWMidiAgent

Handles MIDI operations.

```python
from magda.domains.daw import DAWMidiAgent

agent = DAWMidiAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['add note', 'add chord', 'quantize', 'transpose']

# Execute operation
result = agent.execute("add c major chord", context)
```

## Supported Operations

### Track Operations

- `create track [name]` - Create a new track
- `delete track [name]` - Delete an existing track
- `rename track [old] to [new]` - Rename a track
- `duplicate track [name]` - Duplicate a track

### Volume Operations

- `set volume [track] to [level]` - Set track volume
- `mute [track]` - Mute a track
- `unmute [track]` - Unmute a track
- `pan [track] [direction]` - Pan a track

### Effect Operations

- `add reverb to [track]` - Add reverb effect
- `add compression to [track]` - Add compression
- `add delay to [track]` - Add delay effect
- `add eq to [track]` - Add equalizer

### MIDI Operations

- `add note [note] to [track]` - Add a MIDI note
- `add chord [chord] to [track]` - Add a chord
- `quantize [track]` - Quantize MIDI notes
- `transpose [track] [steps]` - Transpose notes

### Clip Operations

- `create clip [length] on [track]` - Create a clip
- `move clip to [position]` - Move a clip
- `duplicate clip` - Duplicate a clip
- `split clip at [position]` - Split a clip

## Multi-Language Support

MAGDA supports prompts in multiple languages:

### Japanese
```python
# Japanese prompts
"トラック「ベース」の音量を-6dBに設定してください"
# Translation: "Please set the volume of track 'Bass' to -6dB"
```

### Spanish
```python
# Spanish prompts
"Crear una nueva pista llamada 'Guitarra'"
# Translation: "Create a new track called 'Guitar'"
```

### French
```python
# French prompts
"Ajouter de la réverbération à la piste de batterie"
# Translation: "Add reverb to the drums track"
```

### German
```python
# German prompts
"Erstellen Sie eine neue Spur namens 'Bass'"
# Translation: "Create a new track called 'Bass'"
```

## Extending the DAW Domain

To add new operations to the DAW domain:

1. **Create a new agent** (if needed)
2. **Update existing agents** with new capabilities
3. **Add operation keywords** to the fast-path routing
4. **Update tests** to cover new functionality

### Example: Adding a New Agent

```python
from magda.core.domain import DomainAgent, DomainType

class DAWAutomationAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)
        self.name = "automation"

    def can_handle(self, operation: str) -> bool:
        return "automation" in operation.lower()

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Implementation
        pass

    def get_capabilities(self) -> list[str]:
        return ["create automation", "edit automation", "delete automation"]
```
