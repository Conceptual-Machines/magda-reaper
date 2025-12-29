#pragma once

#include <map>
#include <string>
#include <vector>

// Canonical drum names used by the drummer agent
// These are the standard names that get mapped to MIDI notes
namespace CanonicalDrums {
constexpr const char *KICK = "kick";
constexpr const char *SNARE = "snare";
constexpr const char *SNARE_RIM = "snare_rim";
constexpr const char *SNARE_XSTICK = "snare_xstick";
constexpr const char *HI_HAT = "hi_hat";
constexpr const char *HI_HAT_OPEN = "hi_hat_open";
constexpr const char *HI_HAT_PEDAL = "hi_hat_pedal";
constexpr const char *TOM_HIGH = "tom_high";
constexpr const char *TOM_MID = "tom_mid";
constexpr const char *TOM_LOW = "tom_low";
constexpr const char *CRASH = "crash";
constexpr const char *RIDE = "ride";
constexpr const char *RIDE_BELL = "ride_bell";

// Get list of all canonical drum names
inline std::vector<std::string> GetAllDrumNames() {
  return {KICK,     SNARE,   SNARE_RIM, SNARE_XSTICK, HI_HAT, HI_HAT_OPEN, HI_HAT_PEDAL,
          TOM_HIGH, TOM_MID, TOM_LOW,   CRASH,        RIDE,   RIDE_BELL};
}
} // namespace CanonicalDrums

// A drum mapping maps canonical drum names to MIDI note numbers
struct DrumMapping {
  std::string id;                   // Unique identifier (e.g., "addictive_drums_v2")
  std::string name;                 // Display name (e.g., "Addictive Drums 2")
  std::string plugin_key;           // Associated plugin (ident or full_name)
  bool is_preset;                   // True for built-in presets, false for user-created
  std::map<std::string, int> notes; // drum_name -> MIDI note

  // Get MIDI note for a canonical drum name, returns -1 if not found
  int GetNote(const std::string &drum_name) const {
    auto it = notes.find(drum_name);
    return it != notes.end() ? it->second : -1;
  }

  // Set MIDI note for a canonical drum name
  void SetNote(const std::string &drum_name, int note) { notes[drum_name] = note; }
};

// Manages drum mappings - loading, saving, presets
class DrumMappingManager {
public:
  DrumMappingManager();
  ~DrumMappingManager();

  // Load mappings from cache file
  bool LoadFromCache();

  // Save mappings to cache file
  bool SaveToCache() const;

  // Get all mappings
  const std::vector<DrumMapping> &GetMappings() const { return m_mappings; }

  // Get mapping by ID
  const DrumMapping *GetMapping(const std::string &id) const;

  // Get mapping for a specific plugin (by plugin_key)
  const DrumMapping *GetMappingForPlugin(const std::string &plugin_key) const;

  // Add or update a mapping
  void SetMapping(const DrumMapping &mapping);

  // Remove a mapping by ID
  void RemoveMapping(const std::string &id);

  // Get preset mappings (built-in)
  static std::vector<DrumMapping> GetPresetMappings();

  // Create a new mapping from a preset for a specific plugin
  DrumMapping CreateMappingFromPreset(const std::string &preset_id, const std::string &plugin_key,
                                      const std::string &name);

  // Check if a plugin has a drum mapping
  bool HasMappingForPlugin(const std::string &plugin_key) const;

private:
  std::vector<DrumMapping> m_mappings;

  // Cache file path
  static const char *GetCacheFilePath();
};

// Check if a plugin name indicates it's a drum plugin
bool IsDrumPlugin(const std::string &plugin_name);

// Global instance
extern DrumMappingManager *g_drumMappingManager;
