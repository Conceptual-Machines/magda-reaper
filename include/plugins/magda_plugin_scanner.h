#pragma once

#include "../WDL/WDL/wdlstring.h"
#include <map>
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

  // Deduplicate plugins based on format preferences (VST3 > VST > AU > JS)
  // Groups plugins by name and selects the best format based on preferences
  // Returns deduplicated plugin list
  std::vector<PluginInfo>
  DeduplicatePlugins(const std::vector<std::string> &format_order = {}) const;

  // Generate aliases programmatically for plugins (no API call needed)
  // Automatically deduplicates plugins first, then generates aliases
  // Returns true on success, false on error
  // On success, aliases are saved to cache
  bool GenerateAliases();

  // Legacy: Send plugins to API to generate aliases (deprecated - use
  // GenerateAliases instead) Returns true on success, false on error On
  // success, aliases are saved to cache
  bool GenerateAliasesFromAPI();

  // Callback type for async scan completion
  // Called from background thread - should use PostMessage to update UI
  typedef void (*ScanCallback)(bool success, int plugin_count,
                               const char *error);

  // Scan plugins and generate aliases asynchronously in background thread
  // callback will be invoked from the background thread when scan completes
  void ScanAndGenerateAliasesAsync(ScanCallback callback);

  // Get plugin aliases (reverse mapping: full_name -> list of aliases)
  // Returns map of full plugin name -> vector of aliases
  const std::map<std::string, std::vector<std::string>> &
  GetAliasesByPlugin() const {
    return m_aliasesByPlugin;
  }

  // Get all aliases (forward mapping: alias -> full_name, for quick lookup)
  const std::map<std::string, std::string> &GetAliases() const {
    return m_aliases;
  }

  // Resolve plugin alias to full name
  // Returns full name if alias found, otherwise returns alias as-is
  std::string ResolveAlias(const char *alias) const;

  // Set aliases for a plugin (replaces existing aliases)
  void SetPluginAliases(const char *full_name,
                        const std::vector<std::string> &aliases);

  // Add alias for a plugin
  void AddPluginAlias(const char *full_name, const char *alias);

  // Remove alias for a plugin
  void RemovePluginAlias(const char *full_name, const char *alias);

  // Set single alias for a plugin by key (ident), replaces all existing aliases
  void SetAliasForPlugin(const std::string &plugin_key,
                         const std::string &alias);

private:
  std::vector<PluginInfo> m_plugins;
  std::map<std::string, std::vector<std::string>>
      m_aliasesByPlugin; // full_name -> [aliases]
  std::map<std::string, std::string>
      m_aliases; // alias -> full_name (for quick lookup)
  bool m_initialized;

  // Rebuild forward mapping from reverse mapping
  void RebuildAliasMap();

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

  // Load aliases from cache file
  bool LoadAliasesFromCache();

  // Save aliases to cache file
  bool SaveAliasesToCache() const;

  // Helper methods for programmatic alias generation
  std::string ExtractBaseName(const std::string &full_name) const;
  std::vector<std::string>
  GenerateAliasesForPlugin(const PluginInfo &plugin) const;
  std::vector<std::string>
  GenerateVersionAliases(const std::string &base_name) const;
  std::vector<std::string> SplitCamelCase(const std::string &name) const;
  std::vector<std::string>
  GenerateManufacturerAliases(const std::string &base_name,
                              const std::string &manufacturer) const;
  std::vector<std::string>
  GenerateAbbreviationAliases(const std::string &base_name) const;

  // Helper: Convert string to lowercase
  static std::string ToLower(const std::string &str);
  // Helper: Trim whitespace
  static std::string Trim(const std::string &str);
};
