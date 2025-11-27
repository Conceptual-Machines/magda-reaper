#pragma once

#include "../WDL/WDL/wdlstring.h"
#include <string>
#include <vector>

// Plugin information structure
struct PluginInfo {
  std::string name;         // Short name (e.g., "Serum")
  std::string full_name;    // Full name (e.g., "VST3: Serum (Xfer Records)")
  std::string format;       // Format: "VST3", "VST", "AU", "JS", "ReaPlugs"
  std::string manufacturer; // Manufacturer (e.g., "Xfer Records")
  bool is_instrument;       // true for VSTi/AUi, false for effects
  std::string ident;        // Plugin identifier (unique)
};

// Plugin scanner for REAPER
// Scans installed plugins and caches them to ~/.magda/plugins.json
class MagdaPluginScanner {
public:
  MagdaPluginScanner();
  ~MagdaPluginScanner();

  // Scan all installed plugins from REAPER
  // Returns number of plugins found
  int ScanPlugins();

  // Get all scanned plugins
  const std::vector<PluginInfo> &GetPlugins() const { return m_plugins; }

  // Get plugin by name (fuzzy match)
  // Returns nullptr if not found
  const PluginInfo *FindPlugin(const char *name) const;

  // Get plugins matching query (fuzzy search)
  std::vector<const PluginInfo *> SearchPlugins(const char *query) const;

  // Load plugins from cache file
  bool LoadFromCache();

  // Save plugins to cache file
  bool SaveToCache() const;

  // Get cache file path
  static const char *GetCacheFilePath();

  // Check if cache is valid (exists and not too old)
  bool IsCacheValid() const;

  // Clear cache and rescan
  void Refresh();

private:
  std::vector<PluginInfo> m_plugins;
  bool m_initialized;

  // Parse plugin name to extract format, name, manufacturer
  // Example: "VST3: Serum (Xfer Records)" -> format="VST3", name="Serum",
  // manufacturer="Xfer Records"
  bool ParsePluginName(const char *full_name, PluginInfo &info) const;

  // Check if plugin name indicates it's an instrument
  bool IsInstrument(const char *full_name) const;

  // Helper: Get user config directory (~/.magda on Mac/Linux, %APPDATA%/MAGDA
  // on Windows)
  static const char *GetConfigDirectory();

  // Helper: Create config directory if it doesn't exist
  static bool EnsureConfigDirectory();
};
