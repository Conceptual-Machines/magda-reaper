# MAGDA API Response Format

## Endpoint
`POST /api/v1/magda/chat`

## Request Format
```json
{
  "question": "Create a new track called 'Drums'",
  "state": {
    "project": { "name": "My Project", "length": 120.5 },
    "play_state": { "playing": false, "paused": false, "recording": false, "position": 0, "cursor": 0 },
    "time_selection": { "start": 0, "end": 0 },
    "tracks": [
      {
        "index": 0,
        "name": "Track 1",
        "folder": false,
        "selected": true,
        "has_fx": false,
        "muted": false,
        "soloed": false,
        "rec_armed": false,
        "volume_db": 0.0,
        "pan": 0.0,
        "ui_muted": false
      }
    ]
  }
}
```

## Response Format

The backend MUST return a JSON object with an `"actions"` field containing an array of action objects:

```json
{
  "actions": [
    {
      "action": "create_track",
      "index": 1,
      "name": "Drums"
    }
  ]
}
```

### Supported Actions

#### 1. `create_track`
Creates a new track in REAPER.

**Fields:**
- `action` (required): `"create_track"`
- `index` (optional): Track index to insert at (defaults to end)
- `name` (optional): Track name

**Example:**
```json
{
  "action": "create_track",
  "index": 1,
  "name": "Drums"
}
```

#### 2. `create_clip`
Creates a clip/item on a track.

**Fields:**
- `action` (required): `"create_clip"`
- `track` (required): Track index (integer as string)
- `position` (required): Start position in seconds (double as string)
- `length` (required): Clip length in seconds (double as string)

**Example:**
```json
{
  "action": "create_clip",
  "track": "0",
  "position": "0.0",
  "length": "4.0"
}
```

#### 3. `set_track_name`
Sets the name of a track.

**Fields:**
- `action` (required): `"set_track_name"`
- `track` (required): Track index (integer as string)
- `name` (required): New track name

**Example:**
```json
{
  "action": "set_track_name",
  "track": "0",
  "name": "Bass"
}
```

#### 4. `set_track_volume`
Sets the volume of a track in dB.

**Fields:**
- `action` (required): `"set_track_volume"`
- `track` (required): Track index (integer as string)
- `volume_db` (required): Volume in dB (double as string, e.g., "-3.0")

**Example:**
```json
{
  "action": "set_track_volume",
  "track": "0",
  "volume_db": "-3.0"
}
```

#### 5. `set_track_pan`
Sets the pan of a track (-1.0 to 1.0).

**Fields:**
- `action` (required): `"set_track_pan"`
- `track` (required): Track index (integer as string)
- `pan` (required): Pan value from -1.0 (left) to 1.0 (right) (double as string)

**Example:**
```json
{
  "action": "set_track_pan",
  "track": "0",
  "pan": "0.5"
}
```

#### 6. `set_track_mute`
Sets the mute state of a track.

**Fields:**
- `action` (required): `"set_track_mute"`
- `track` (required): Track index (integer as string)
- `mute` (required): `"true"` or `"false"` (or `"1"`/`"0"`)

**Example:**
```json
{
  "action": "set_track_mute",
  "track": "0",
  "mute": "true"
}
```

#### 7. `set_track_solo`
Sets the solo state of a track.

**Fields:**
- `action` (required): `"set_track_solo"`
- `track` (required): Track index (integer as string)
- `solo` (required): `"true"` or `"false"` (or `"1"`/`"0"`)

**Example:**
```json
{
  "action": "set_track_solo",
  "track": "0",
  "solo": "true"
}
```

## Multiple Actions

The backend can return multiple actions in a single response:

```json
{
  "actions": [
    {
      "action": "create_track",
      "name": "Drums"
    },
    {
      "action": "set_track_volume",
      "track": "0",
      "volume_db": "-3.0"
    }
  ]
}
```

## Notes

1. **Numeric values as strings**: All numeric values (track indices, positions, volumes, etc.) should be sent as strings in the JSON. The client will parse them.

2. **Action execution order**: Actions are executed in the order they appear in the array.

3. **Error handling**: If an action fails, execution continues with the next action. Errors are logged but don't fail the entire request.

4. **Fallback behavior**: If the response doesn't have an `"actions"` field, the client will attempt to execute the entire response as a single action (if it's a valid action object or array).
