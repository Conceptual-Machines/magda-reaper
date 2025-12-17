#pragma once

#include <map>
#include <string>
#include <vector>

// Structure to hold parameter mappings for a single plugin
struct ParamMapping {
  std::string plugin_key;  // e.g., "VST3i: Serum (Xfer Records)"
  std::string plugin_name; // Display name
  // Maps canonical name -> parameter index
  // e.g., "cutoff" -> 42, "resonance" -> 43
  std::map<std::string, int> aliases;
};

// Common canonical parameter names (suggestions for users)
const std::vector<std::string> CANONICAL_PARAM_NAMES = {
    // Filter
    "cutoff", "resonance", "filter_type", "filter_drive",
    // Envelope
    "attack", "decay", "sustain", "release",
    // Oscillator
    "osc_pitch", "osc_fine", "osc_level", "osc_pan",
    // Modulation
    "lfo_rate", "lfo_depth", "lfo_shape",
    // Effects
    "mix", "dry_wet", "feedback", "delay_time", "reverb_size",
    // Dynamics
    "threshold", "ratio", "makeup", "knee",
    // EQ
    "low_gain", "mid_gain", "high_gain", "low_freq", "mid_freq", "high_freq",
    // General
    "volume", "pan", "width", "drive", "gain"};

// Manages parameter mappings for all plugins
class ParamMappingManager {
public:
  ParamMappingManager();
  ~ParamMappingManager();

  // Load/save mappings from/to disk
  bool LoadMappings();
  bool SaveMappings() const;

  // Get mapping for a specific plugin
  const ParamMapping *GetMappingForPlugin(const std::string &plugin_key) const;

  // Set/update mapping for a plugin
  void SetMapping(const ParamMapping &mapping);

  // Remove mapping for a plugin
  void RemoveMapping(const std::string &plugin_key);

  // Resolve a canonical parameter name to an index for a plugin
  // Returns -1 if not found
  int ResolveParamAlias(const std::string &plugin_key,
                        const std::string &alias) const;

  // Get all mappings
  const std::map<std::string, ParamMapping> &GetAllMappings() const {
    return m_mappings;
  }

  // Get canonical param names for suggestions
  static const std::vector<std::string> &GetCanonicalParamNames() {
    return CANONICAL_PARAM_NAMES;
  }

private:
  std::map<std::string, ParamMapping> m_mappings; // plugin_key -> mapping

  static std::string GetMappingsFilePath();
};

// Global instance
extern ParamMappingManager *g_paramMappingManager;
