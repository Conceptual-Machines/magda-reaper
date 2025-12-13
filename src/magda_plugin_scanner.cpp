#include "magda_plugin_scanner.h"
#include "magda_api_client.h"
#include "magda_auth.h"
#include "reaper_plugin.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "../WDL/WDL/wdlcstring.h"
#include "reaper_plugin_functions.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <set>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pthread.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern reaper_plugin_info_t *g_rec;

MagdaPluginScanner::MagdaPluginScanner() : m_initialized(false) {
  // Try to load plugins and aliases from cache on initialization
  LoadFromCache();
  LoadAliasesFromCache();
}

MagdaPluginScanner::~MagdaPluginScanner() {}

const char *MagdaPluginScanner::GetConfigDirectory() {
  static char config_dir[512] = {0};
  if (config_dir[0] != '\0') {
    return config_dir;
  }

#ifdef _WIN32
  // Windows: %APPDATA%/MAGDA
  char appdata[256];
  if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                       appdata) == S_OK) {
    snprintf(config_dir, sizeof(config_dir), "%s\\MAGDA", appdata);
  } else {
    strcpy(config_dir, "C:\\Users\\Public\\MAGDA");
  }
#else
  // Mac/Linux: ~/.magda
  const char *home = getenv("HOME");
  if (home) {
    snprintf(config_dir, sizeof(config_dir), "%s/.magda", home);
  } else {
    // Fallback to current directory
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
      snprintf(config_dir, sizeof(config_dir), "%s/.magda", pw->pw_dir);
    } else {
      strcpy(config_dir, ".magda");
    }
  }
#endif

  return config_dir;
}

bool MagdaPluginScanner::EnsureConfigDirectory() {
  const char *config_dir = GetConfigDirectory();
  if (!config_dir || config_dir[0] == '\0') {
    return false;
  }

#ifdef _WIN32
  // Create directory on Windows
  CreateDirectoryA(config_dir, NULL);
  return GetFileAttributesA(config_dir) != INVALID_FILE_ATTRIBUTES;
#else
  // Create directory on Mac/Linux
  struct stat st;
  if (stat(config_dir, &st) != 0) {
    // Directory doesn't exist, create it
    if (mkdir(config_dir, 0755) != 0) {
      return false;
    }
  }
  return true;
#endif
}

const char *MagdaPluginScanner::GetCacheFilePath() {
  static char cache_path[512] = {0};
  if (cache_path[0] != '\0') {
    return cache_path;
  }

  const char *config_dir = GetConfigDirectory();
  snprintf(cache_path, sizeof(cache_path), "%s/plugins.json", config_dir);
  return cache_path;
}

bool MagdaPluginScanner::IsInstrument(const char *full_name) const {
  if (!full_name) {
    return false;
  }
  // Check for instrument indicators: VSTi, AUi, or "instrument" in name
  return (strstr(full_name, "VSTi:") != nullptr) ||
         (strstr(full_name, "AUi:") != nullptr) ||
         (strstr(full_name, "instrument") != nullptr);
}

bool MagdaPluginScanner::ParsePluginName(const char *full_name,
                                         PluginInfo &info) const {
  if (!full_name || full_name[0] == '\0') {
    return false;
  }

  info.full_name = full_name;
  info.is_instrument = IsInstrument(full_name);

  // Parse format and name
  // Formats: "VST3:", "VST:", "AU:", "JS:", "ReaPlugs:"
  const char *colon = strchr(full_name, ':');
  if (colon) {
    // Extract format (everything before colon)
    int format_len = colon - full_name;
    info.format.assign(full_name, format_len);

    // Extract name and manufacturer (everything after colon and space)
    const char *name_start = colon + 1;
    while (*name_start == ' ') {
      name_start++;
    }

    // Look for manufacturer in parentheses: "Serum (Xfer Records)"
    // But skip bitness markers like "(x64)", "(x86)"
    const char *open_paren = strchr(name_start, '(');
    if (open_paren) {
      const char *close_paren = strchr(open_paren, ')');
      std::string paren_content;
      if (close_paren) {
        paren_content.assign(open_paren + 1, close_paren - open_paren - 1);
      } else {
        paren_content = open_paren + 1;
      }

      // Check if this is a bitness marker (not a manufacturer)
      std::string paren_lower;
      for (char c : paren_content) {
        paren_lower += (char)std::tolower(c);
      }
      bool is_bitness = (paren_lower == "x64" || paren_lower == "x86" ||
                         paren_lower == "64bit" || paren_lower == "32bit" ||
                         paren_lower == "64-bit" || paren_lower == "32-bit");

      // Extract name (everything before opening paren, trimmed)
      int name_len = open_paren - name_start;
      while (name_len > 0 && name_start[name_len - 1] == ' ') {
        name_len--;
      }
      info.name.assign(name_start, name_len);

      // Only set manufacturer if it's not a bitness marker
      if (!is_bitness && !paren_content.empty()) {
        info.manufacturer = paren_content;
      }
    } else {
      // No manufacturer, name is everything after colon
      info.name = name_start;
    }
  } else {
    // No colon, assume JS or ReaPlugs format
    // Try to detect format from name
    if (strncmp(full_name, "JS:", 3) == 0) {
      info.format = "JS";
      const char *name_start = full_name + 3;
      while (*name_start == ' ') {
        name_start++;
      }
      info.name = name_start;
    } else {
      info.format = "JS"; // Default to JS if no format indicator
      info.name = full_name;
    }
  }

  // Trim whitespace from name
  size_t start = 0;
  while (start < info.name.length() && info.name[start] == ' ') {
    start++;
  }
  size_t end = info.name.length();
  while (end > start && info.name[end - 1] == ' ') {
    end--;
  }
  if (start > 0 || end < info.name.length()) {
    info.name = info.name.substr(start, end - start);
  }

  return !info.name.empty();
}

int MagdaPluginScanner::ScanPlugins() {
  if (!g_rec) {
    return 0;
  }

  // Get EnumInstalledFX function
  bool (*EnumInstalledFX)(int index, const char **nameOut,
                          const char **identOut) =
      (bool (*)(int, const char **, const char **))g_rec->GetFunc(
          "EnumInstalledFX");

  if (!EnumInstalledFX) {
    // Log error
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg(
            "MAGDA: ERROR - EnumInstalledFX function not available\n");
      }
    }
    return 0;
  }

  m_plugins.clear();

  // Enumerate all plugins
  int index = 0;
  while (true) {
    const char *name = nullptr;
    const char *ident = nullptr;

    if (!EnumInstalledFX(index, &name, &ident)) {
      break; // No more plugins
    }

    if (name && name[0] != '\0') {
      PluginInfo info;
      if (ParsePluginName(name, info)) {
        if (ident) {
          info.ident = ident;
        }
        m_plugins.push_back(info);
      }
    }

    index++;
  }

  m_initialized = true;

  // Log result
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA: Scanned %d plugins\n",
               (int)m_plugins.size());
      ShowConsoleMsg(msg);
    }
  }

  return (int)m_plugins.size();
}

const PluginInfo *MagdaPluginScanner::FindPlugin(const char *name) const {
  if (!name || name[0] == '\0') {
    return nullptr;
  }

  // Case-insensitive search
  std::string search_name = name;
  std::transform(search_name.begin(), search_name.end(), search_name.begin(),
                 ::tolower);

  for (const auto &plugin : m_plugins) {
    std::string plugin_name = plugin.name;
    std::transform(plugin_name.begin(), plugin_name.end(), plugin_name.begin(),
                   ::tolower);

    if (plugin_name == search_name) {
      return &plugin;
    }
  }

  return nullptr;
}

std::vector<const PluginInfo *>
MagdaPluginScanner::SearchPlugins(const char *query) const {
  std::vector<const PluginInfo *> results;

  if (!query || query[0] == '\0') {
    return results;
  }

  // Case-insensitive search
  std::string search_query = query;
  std::transform(search_query.begin(), search_query.end(), search_query.begin(),
                 ::tolower);

  for (const auto &plugin : m_plugins) {
    std::string plugin_name = plugin.name;
    std::transform(plugin_name.begin(), plugin_name.end(), plugin_name.begin(),
                   ::tolower);

    std::string full_name = plugin.full_name;
    std::transform(full_name.begin(), full_name.end(), full_name.begin(),
                   ::tolower);

    // Check if query matches name or full name
    if (plugin_name.find(search_query) != std::string::npos ||
        full_name.find(search_query) != std::string::npos) {
      results.push_back(&plugin);
    }
  }

  return results;
}

bool MagdaPluginScanner::LoadFromCache() {
  const char *cache_path = GetCacheFilePath();
  if (!cache_path) {
    return false;
  }

  FILE *f = fopen(cache_path, "r");
  if (!f) {
    return false;
  }

  // TODO: Implement proper JSON parsing using WDL's JSON parser or similar
  // For now, just check if file exists and is readable
  // We'll implement full JSON parsing in a future update
  fclose(f);
  return false; // Not fully implemented yet - will use proper JSON parser
}

bool MagdaPluginScanner::SaveToCache() const {
  if (!EnsureConfigDirectory()) {
    return false;
  }

  const char *cache_path = GetCacheFilePath();
  if (!cache_path) {
    return false;
  }

  FILE *f = fopen(cache_path, "w");
  if (!f) {
    return false;
  }

  // Helper function to escape JSON strings
  auto escape_json = [](const std::string &str) -> std::string {
    std::string result;
    result.reserve(str.length() + 10);
    for (char c : str) {
      switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        if (c >= 0 && c < 32) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
          result += buf;
        } else {
          result += c;
        }
        break;
      }
    }
    return result;
  };

  // Write JSON
  fprintf(f, "{\n");
  fprintf(f, "  \"plugins\": [\n");

  for (size_t i = 0; i < m_plugins.size(); i++) {
    const auto &plugin = m_plugins[i];
    std::string name_escaped = escape_json(plugin.name);
    std::string full_name_escaped = escape_json(plugin.full_name);
    std::string format_escaped = escape_json(plugin.format);
    std::string manufacturer_escaped = escape_json(plugin.manufacturer);
    std::string ident_escaped = escape_json(plugin.ident);

    fprintf(f, "    {\n");
    fprintf(f, "      \"name\": \"%s\",\n", name_escaped.c_str());
    fprintf(f, "      \"full_name\": \"%s\",\n", full_name_escaped.c_str());
    fprintf(f, "      \"format\": \"%s\",\n", format_escaped.c_str());
    fprintf(f, "      \"manufacturer\": \"%s\",\n",
            manufacturer_escaped.c_str());
    fprintf(f, "      \"is_instrument\": %s,\n",
            plugin.is_instrument ? "true" : "false");
    fprintf(f, "      \"ident\": \"%s\"\n", ident_escaped.c_str());
    fprintf(f, "    }%s\n", (i < m_plugins.size() - 1) ? "," : "");
  }

  fprintf(f, "  ],\n");
  fprintf(f, "  \"scanned_at\": %ld\n", (long)time(nullptr));
  fprintf(f, "}\n");

  fclose(f);
  return true;
}

bool MagdaPluginScanner::IsCacheValid() const {
  const char *cache_path = GetCacheFilePath();
  if (!cache_path) {
    return false;
  }

  FILE *f = fopen(cache_path, "r");
  if (!f) {
    return false;
  }
  fclose(f);

  // TODO: Check file modification time and compare with current time
  // For now, just check if file exists
  return true;
}

void MagdaPluginScanner::Refresh() {
  m_plugins.clear();
  m_initialized = false;
  ScanPlugins();
  SaveToCache();
}

std::vector<PluginInfo> MagdaPluginScanner::DeduplicatePlugins(
    const std::vector<std::string> &format_order) const {
  std::vector<PluginInfo> result;

  if (m_plugins.empty()) {
    return result;
  }

  // Default format order: VST3 > VST > AU > JS
  std::vector<std::string> order = format_order;
  if (order.empty()) {
    order = {"VST3", "VST3i", "VST", "VSTi", "AU", "AUi", "JS", "ReaPlugs"};
  }

  // Create format priority map (lower index = higher priority)
  std::map<std::string, int> format_priority;
  for (size_t i = 0; i < order.size(); i++) {
    format_priority[order[i]] = (int)i;
    // Also handle instrument variants (VST3i -> VST3)
    if (order[i].size() > 1 && order[i].back() == 'i') {
      std::string base = order[i].substr(0, order[i].size() - 1);
      if (format_priority.find(base) == format_priority.end()) {
        format_priority[base] = (int)i;
      }
    }
  }

  // Group plugins by name (case-insensitive)
  std::map<std::string, std::vector<const PluginInfo *>> plugin_groups;
  for (const auto &plugin : m_plugins) {
    std::string key = plugin.name;
    // Convert to lowercase for grouping
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    plugin_groups[key].push_back(&plugin);
  }

  // For each group, select the best plugin based on format preferences
  for (const auto &group_pair : plugin_groups) {
    const auto &group = group_pair.second;

    if (group.empty())
      continue;
    if (group.size() == 1) {
      result.push_back(*group[0]);
      continue;
    }

    // Sort by format priority
    std::vector<const PluginInfo *> sorted = group;
    std::sort(
        sorted.begin(), sorted.end(),
        [&format_priority](const PluginInfo *a, const PluginInfo *b) {
          int priority_a = 999;
          int priority_b = 999;

          // Get priority for format
          auto it_a = format_priority.find(a->format);
          if (it_a != format_priority.end()) {
            priority_a = it_a->second;
          } else {
            // Check base format (remove 'i' suffix)
            if (a->format.size() > 1 && a->format.back() == 'i') {
              std::string base = a->format.substr(0, a->format.size() - 1);
              auto it_base = format_priority.find(base);
              if (it_base != format_priority.end()) {
                priority_a = it_base->second;
              }
            }
          }

          auto it_b = format_priority.find(b->format);
          if (it_b != format_priority.end()) {
            priority_b = it_b->second;
          } else {
            // Check base format (remove 'i' suffix)
            if (b->format.size() > 1 && b->format.back() == 'i') {
              std::string base = b->format.substr(0, b->format.size() - 1);
              auto it_base = format_priority.find(base);
              if (it_base != format_priority.end()) {
                priority_b = it_base->second;
              }
            }
          }

          if (priority_a != priority_b) {
            return priority_a <
                   priority_b; // Lower priority number = higher preference
          }

          // If same priority, prefer instrument if available
          if (a->is_instrument != b->is_instrument) {
            return a->is_instrument;
          }

          // If still tied, prefer shorter ident (newer versions often have
          // longer paths)
          return a->ident.length() < b->ident.length();
        });

    // Add the best plugin (first in sorted list)
    result.push_back(*sorted[0]);
  }

  return result;
}

// Helper: Convert string to lowercase
std::string MagdaPluginScanner::ToLower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

// Helper: Trim whitespace
std::string MagdaPluginScanner::Trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

// Extract base name from full plugin name
// Examples: "VST3: Serum (Xfer Records)" -> "Serum"
std::string
MagdaPluginScanner::ExtractBaseName(const std::string &full_name) const {
  std::string name = full_name;

  // Remove format prefix (VST3:, VST:, JS:, etc.)
  size_t colon_pos = name.find(':');
  if (colon_pos != std::string::npos) {
    name = name.substr(colon_pos + 1);
    // Trim leading whitespace
    name = Trim(name);
  }

  // Remove manufacturer suffix in parentheses (but keep checking for multiple)
  while (true) {
    size_t paren_start = name.find_last_of('(');
    if (paren_start != std::string::npos) {
      size_t paren_end = name.find(')', paren_start);
      if (paren_end != std::string::npos) {
        std::string paren_content =
            name.substr(paren_start + 1, paren_end - paren_start - 1);
        // Remove common patterns like "(x64)", "(x86)", "(64bit)", etc.
        std::string paren_lower = ToLower(paren_content);
        if (paren_lower == "x64" || paren_lower == "x86" ||
            paren_lower == "64bit" || paren_lower == "32bit" ||
            paren_lower == "64-bit" || paren_lower == "32-bit") {
          // Remove this parentheses group
          name = name.substr(0, paren_start) + name.substr(paren_end + 1);
          name = Trim(name);
          continue; // Check for more
        }
      }
      // If it's not a bitness marker, treat as manufacturer and remove
      name = name.substr(0, paren_start);
      name = Trim(name);
      break;
    }
    break;
  }

  return name;
}

// Generate version aliases (e.g., "Kontakt 7" -> ["kontakt", "kontakt7",
// "kontakt 7"]) Also handles "ShaperBox 3" -> ["shaperbox", "shaperbox3",
// "shaperbox 3", "sb3", "sb"]
std::vector<std::string>
MagdaPluginScanner::GenerateVersionAliases(const std::string &base_name) const {
  std::vector<std::string> aliases;

  // Match version patterns: "Name 7", "Name 1.2", "Name v2", "ShaperBox 3",
  // etc. Simple regex-like matching: find space followed by digits
  for (size_t i = 0; i < base_name.length(); i++) {
    if (base_name[i] == ' ' && i + 1 < base_name.length()) {
      // Check if next char is digit or 'v'/'V' followed by digit
      char next = base_name[i + 1];
      if (std::isdigit(next) ||
          (std::tolower(next) == 'v' && i + 2 < base_name.length() &&
           std::isdigit(base_name[i + 2]))) {
        std::string name_part = Trim(base_name.substr(0, i));
        std::string version_part = base_name.substr(i + 1);
        version_part = Trim(version_part);

        if (!name_part.empty() && !version_part.empty()) {
          // Name without version
          aliases.push_back(name_part);
          // Name + version (no space)
          aliases.push_back(name_part + version_part);
          // Name + version (with space)
          aliases.push_back(name_part + " " + version_part);

          // For names like "ShaperBox 3", also generate "sb3" and "sb"
          std::string name_lower = ToLower(name_part);
          if (name_lower.length() >= 2) {
            // First two letters abbreviation
            std::string abbrev = name_lower.substr(0, 2);
            // Extract version number (just digits)
            std::string version_num;
            for (char c : version_part) {
              if (std::isdigit(c)) {
                version_num += c;
              }
            }
            if (!version_num.empty()) {
              aliases.push_back(abbrev + version_num); // "sb3"
              aliases.push_back(abbrev);               // "sb"
            }
          }
        }
        break;
      }
    }
  }

  return aliases;
}

// Split camelCase/PascalCase and generate aliases
// Examples: "ReaEQ" -> ["reaeq", "rea-eq", "rea eq", "eq"]
std::vector<std::string>
MagdaPluginScanner::SplitCamelCase(const std::string &name) const {
  std::vector<std::string> aliases;
  std::vector<std::string> words;
  std::string current_word;

  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    bool is_upper = std::isupper(c);

    if (i > 0 && is_upper) {
      char prev = name[i - 1];
      bool prev_lower = std::islower(prev);
      bool next_lower = (i + 1 < name.length()) && std::islower(name[i + 1]);

      // Split if previous was lowercase OR if this is start of new word (next
      // is lowercase)
      if (prev_lower || (next_lower && !current_word.empty())) {
        if (!current_word.empty()) {
          words.push_back(current_word);
          current_word.clear();
        }
      }
    }
    current_word += c;
  }

  if (!current_word.empty()) {
    words.push_back(current_word);
  }

  if (words.size() <= 1) {
    return aliases;
  }

  // Generate combinations
  std::string joined;
  for (const auto &w : words) {
    joined += w;
  }
  aliases.push_back(joined);

  std::string hyphenated;
  for (size_t i = 0; i < words.size(); i++) {
    if (i > 0)
      hyphenated += "-";
    hyphenated += words[i];
  }
  aliases.push_back(hyphenated);

  std::string spaced;
  for (size_t i = 0; i < words.size(); i++) {
    if (i > 0)
      spaced += " ";
    spaced += words[i];
  }
  aliases.push_back(spaced);

  // Last word only (common abbreviation)
  if (words.size() > 1) {
    aliases.push_back(words.back());
  }

  return aliases;
}

// Generate manufacturer-prefixed aliases
std::vector<std::string> MagdaPluginScanner::GenerateManufacturerAliases(
    const std::string &base_name, const std::string &manufacturer) const {
  std::vector<std::string> aliases;

  if (manufacturer.empty()) {
    return aliases;
  }

  // Extract key words from manufacturer (remove common words)
  std::string manufacturer_lower = ToLower(manufacturer);
  std::vector<std::string> words;
  std::string current_word;

  for (char c : manufacturer_lower) {
    if (std::isspace(c)) {
      if (!current_word.empty()) {
        words.push_back(current_word);
        current_word.clear();
      }
    } else {
      current_word += c;
    }
  }
  if (!current_word.empty()) {
    words.push_back(current_word);
  }

  // Filter out common words
  std::set<std::string> common_words = {
      "records", "inc", "ltd", "llc", "audio", "music", "technologies"};
  std::vector<std::string> key_words;

  for (const auto &word : words) {
    if (common_words.find(word) == common_words.end() && word.length() > 2) {
      key_words.push_back(word);
    }
  }

  if (key_words.empty() && !words.empty()) {
    key_words.push_back(words[0]);
  }

  // Generate aliases
  for (const auto &keyword : key_words) {
    aliases.push_back(keyword + " " + base_name);
    aliases.push_back(keyword + base_name);
  }

  return aliases;
}

// Generate abbreviation aliases (e.g., "ReaEQ" -> ["eq"])
std::vector<std::string> MagdaPluginScanner::GenerateAbbreviationAliases(
    const std::string &base_name) const {
  std::vector<std::string> aliases;
  std::string base_lower = ToLower(base_name);

  // Common suffix patterns
  std::map<std::string, std::string> suffix_patterns = {
      {"eq", "eq"},         {"comp", "comp"},        {"compressor", "comp"},
      {"verb", "verb"},     {"reverb", "verb"},      {"delay", "delay"},
      {"limiter", "limit"}, {"gate", "gate"},        {"filter", "filter"},
      {"synth", "synth"},   {"synthesizer", "synth"}};

  for (const auto &pattern : suffix_patterns) {
    if (base_lower.length() >= pattern.first.length() &&
        base_lower.substr(base_lower.length() - pattern.first.length()) ==
            pattern.first) {
      aliases.push_back(pattern.second);
      break;
    }
  }

  return aliases;
}

// Generate all aliases for a single plugin
// Simple approach: lowercase name with underscores instead of spaces
std::vector<std::string>
MagdaPluginScanner::GenerateAliasesForPlugin(const PluginInfo &plugin) const {
  std::vector<std::string> aliases;

  // Use plugin.name if available (cleaner), otherwise extract from full_name
  std::string base_name =
      plugin.name.empty() ? ExtractBaseName(plugin.full_name) : plugin.name;
  if (base_name.empty()) {
    return aliases;
  }

  // Convert to lowercase and replace spaces with underscores
  std::string alias;
  for (char c : base_name) {
    if (c == ' ') {
      alias += '_';
    } else {
      alias += (char)std::tolower(c);
    }
  }

  // Remove any double underscores and trim
  while (alias.find("__") != std::string::npos) {
    size_t pos = alias.find("__");
    alias.erase(pos, 1);
  }
  while (!alias.empty() && alias.front() == '_') {
    alias.erase(0, 1);
  }
  while (!alias.empty() && alias.back() == '_') {
    alias.pop_back();
  }

  if (!alias.empty()) {
    aliases.push_back(alias);
  }

  return aliases;
}

// Generate aliases programmatically (no API call)
bool MagdaPluginScanner::GenerateAliases() {
  if (m_plugins.empty()) {
    return false;
  }

  // Deduplicate plugins first
  std::vector<PluginInfo> deduplicated = DeduplicatePlugins();

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: Generating aliases for %d deduplicated plugins...\n",
               (int)deduplicated.size());
      ShowConsoleMsg(msg);
    }
  }

  // Clear existing aliases
  m_aliases.clear();
  m_aliasesByPlugin.clear();

  // Generate aliases for each plugin
  // Use ident as key (unique), fall back to full_name if ident is empty
  int skipped_count = 0;
  for (const auto &plugin : deduplicated) {
    // Use ident as the unique key, fall back to full_name
    const std::string &plugin_key =
        plugin.ident.empty() ? plugin.full_name : plugin.ident;

    auto plugin_aliases = GenerateAliasesForPlugin(plugin);

    // If no aliases generated, log it for debugging
    if (plugin_aliases.empty()) {
      skipped_count++;
      if (g_rec && skipped_count <= 10) { // Log first 10 skipped plugins
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(msg, sizeof(msg), "MAGDA: Skipped plugin (no aliases): %s\n",
                   plugin.full_name.c_str());
          ShowConsoleMsg(msg);
        }
      }
      // Still add a fallback alias from the plugin name itself
      std::string base_name = ExtractBaseName(plugin.full_name);
      if (!base_name.empty()) {
        std::string fallback = ToLower(base_name);
        m_aliases[fallback] = plugin_key;
        m_aliasesByPlugin[plugin_key].push_back(fallback);
      }
      continue;
    }

    // Store aliases
    for (const auto &alias : plugin_aliases) {
      std::string normalized = ToLower(Trim(alias));
      if (!normalized.empty()) {
        // Handle conflicts: if alias exists, try adding manufacturer prefix
        auto it = m_aliases.find(normalized);
        if (it != m_aliases.end() && it->second != plugin_key) {
          // Conflict - try manufacturer-prefixed version
          if (!plugin.manufacturer.empty()) {
            std::string mfr_lower;
            for (char c : plugin.manufacturer) {
              if (c == ' ') {
                mfr_lower += '_';
              } else {
                mfr_lower += (char)std::tolower(c);
              }
            }
            std::string unique_alias = mfr_lower + "_" + normalized;
            if (m_aliases.find(unique_alias) == m_aliases.end()) {
              m_aliases[unique_alias] = plugin_key;
              m_aliasesByPlugin[plugin_key].push_back(unique_alias);
            }
          }
          // If no manufacturer or still conflicts, just store for display
          // (won't be in forward mapping but will show in list)
          if (m_aliasesByPlugin[plugin_key].empty()) {
            m_aliasesByPlugin[plugin_key].push_back(normalized);
          }
        } else {
          m_aliases[normalized] = plugin_key;
          m_aliasesByPlugin[plugin_key].push_back(normalized);
        }
      }
    }
  }

  if (skipped_count > 0 && g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: %d plugins had no aliases generated (added fallback)\n",
               skipped_count);
      ShowConsoleMsg(msg);
    }
  }

  // Save to cache
  SaveAliasesToCache();

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA: Generated %d aliases for %d plugins\n",
               (int)m_aliases.size(), (int)deduplicated.size());
      ShowConsoleMsg(msg);
    }
  }

  return true;
}

bool MagdaPluginScanner::GenerateAliasesFromAPI() {
  if (m_plugins.empty()) {
    return false;
  }

  // Deduplicate plugins before sending to API (reduces payload size)
  std::vector<PluginInfo> deduplicated = DeduplicatePlugins();

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: Deduplicated %d plugins to %d unique plugins\n",
               (int)m_plugins.size(), (int)deduplicated.size());
      ShowConsoleMsg(msg);
    }
  }

  // Build JSON request with deduplicated plugin list
  WDL_FastString json;
  json.Set("{\"plugins\":[");

  for (size_t i = 0; i < m_plugins.size(); i++) {
    const auto &plugin = m_plugins[i];

    // Helper to escape JSON strings
    auto escape_json = [](const std::string &str) -> std::string {
      std::string result;
      result.reserve(str.length() + 10);
      for (char c : str) {
        switch (c) {
        case '"':
          result += "\\\"";
          break;
        case '\\':
          result += "\\\\";
          break;
        case '\n':
          result += "\\n";
          break;
        case '\r':
          result += "\\r";
          break;
        case '\t':
          result += "\\t";
          break;
        default:
          if (c >= 0 && c < 32) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
            result += buf;
          } else {
            result += c;
          }
          break;
        }
      }
      return result;
    };

    std::string name_escaped = escape_json(plugin.name);
    std::string full_name_escaped = escape_json(plugin.full_name);
    std::string format_escaped = escape_json(plugin.format);
    std::string manufacturer_escaped = escape_json(plugin.manufacturer);
    std::string ident_escaped = escape_json(plugin.ident);

    json.Append("{");
    json.Append("\"name\":\"");
    json.Append(name_escaped.c_str());
    json.Append("\",\"full_name\":\"");
    json.Append(full_name_escaped.c_str());
    json.Append("\",\"format\":\"");
    json.Append(format_escaped.c_str());
    json.Append("\",\"manufacturer\":\"");
    json.Append(manufacturer_escaped.c_str());
    json.Append("\",\"is_instrument\":");
    json.Append(plugin.is_instrument ? "true" : "false");
    json.Append(",\"ident\":\"");
    json.Append(ident_escaped.c_str());
    json.Append("\"}");

    if (i < deduplicated.size() - 1) {
      json.Append(",");
    }
  }

  json.Append("]}");

  // Get API client
  static MagdaHTTPClient *g_apiClient = nullptr;
  if (!g_apiClient) {
    g_apiClient = new MagdaHTTPClient();

    // Set JWT token if available (HTTP client will auto-refresh on 401)
    const char *jwt = MagdaAuth::GetStoredToken();
    if (jwt && strlen(jwt) > 0) {
      g_apiClient->SetJWTToken(jwt);
    }
  }

  // Log request size
  int json_size = (int)json.GetLength();
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: Sending %d deduplicated plugins to API (JSON size: %d "
               "bytes)\n",
               (int)deduplicated.size(), json_size);
      ShowConsoleMsg(msg);
    }
  }

  // Send POST request (HTTP client handles token refresh automatically on 401)
  // Use longer timeout for large plugin lists (120 seconds)
  WDL_FastString response;
  WDL_FastString error_msg;

  bool success = g_apiClient->SendPOSTRequest(
      "/api/v1/magda/plugins/process", json.Get(), response, error_msg, 120);

  if (!success) {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA: Failed to generate aliases: %s\n",
                 error_msg.Get());
        ShowConsoleMsg(msg);
      }
    }
    return false;
  }

  // Parse response JSON: {"aliases": {"alias": "full_name", ...}}
  m_aliases.clear();
  m_aliasesByPlugin.clear();

  // Simple JSON parsing for aliases
  const char *aliases_start = strstr(response.Get(), "\"aliases\"");
  if (aliases_start) {
    const char *obj_start = strchr(aliases_start, '{');
    if (obj_start) {
      const char *p = obj_start + 1;
      while (*p && *p != '}') {
        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
          p++;
        if (*p == '}')
          break;

        if (*p == '"') {
          p++;
          const char *key_start = p;
          while (*p && *p != '"') {
            if (*p == '\\')
              p++;
            p++;
          }
          if (*p != '"')
            break;
          std::string alias(key_start, p - key_start);
          p++;

          while (*p && (*p == ' ' || *p == ':' || *p == '\n' || *p == '\r' ||
                        *p == '\t'))
            p++;
          if (*p != '"')
            break;
          p++;
          const char *val_start = p;
          while (*p && *p != '"') {
            if (*p == '\\')
              p++;
            p++;
          }
          if (*p != '"')
            break;
          std::string full_name(val_start, p - val_start);
          p++;

          // Store in forward mapping
          m_aliases[alias] = full_name;
        } else {
          p++;
        }
      }
    }
  }

  // Rebuild reverse mapping
  RebuildAliasMap();

  // Save to cache
  SaveAliasesToCache();

  return true;
}

// Thread data structure for async scanning
struct ScanThreadData {
  MagdaPluginScanner *scanner;
  MagdaPluginScanner::ScanCallback callback;
  HANDLE threadHandle;
  bool completed;
  bool success;
  int plugin_count;
  WDL_FastString error_msg;
};

// Background thread function for plugin scanning
#ifdef _WIN32
DWORD WINAPI ScanThreadProc(LPVOID param)
#else
void *ScanThreadProc(void *param)
#endif
{
  ScanThreadData *data = (ScanThreadData *)param;

  // Scan plugins
  data->plugin_count = data->scanner->ScanPlugins();

  if (data->plugin_count > 0) {
    // Save to cache
    data->scanner->SaveToCache();

    // Generate aliases via API
    data->success = data->scanner->GenerateAliases();
    if (!data->success) {
      data->error_msg.Set("Failed to generate aliases");
    }
  } else {
    data->success = false;
    data->error_msg.Set("No plugins found");
  }

  data->completed = true;

  // Call callback - the callback should use PostMessage to update UI
  if (data->callback) {
    data->callback(data->success, data->plugin_count,
                   data->success ? nullptr : data->error_msg.Get());
  }

  // Clean up thread handle
#ifdef _WIN32
  if (data->threadHandle) {
    CloseHandle(data->threadHandle);
  }
#else
  if (data->threadHandle) {
    pthread_t thread = (pthread_t)(INT_PTR)data->threadHandle;
    pthread_detach(thread);
  }
#endif

  delete data;

#ifdef _WIN32
  return 0;
#else
  return nullptr;
#endif
}

void MagdaPluginScanner::ScanAndGenerateAliasesAsync(ScanCallback callback) {
  if (!callback) {
    return;
  }

  ScanThreadData *data = new ScanThreadData();
  data->scanner = this;
  data->callback = callback;
  data->completed = false;
  data->success = false;
  data->plugin_count = 0;
  data->threadHandle = nullptr;

  HANDLE threadHandle = nullptr;
#ifdef _WIN32
  DWORD threadId = 0;
  threadHandle = CreateThread(NULL, 0, ScanThreadProc, data, 0, &threadId);
#else
  pthread_t thread;
  if (pthread_create(&thread, NULL, ScanThreadProc, data) == 0) {
    threadHandle = (HANDLE)(INT_PTR)thread;
  } else {
    if (callback) {
      callback(false, 0, "Failed to create scan thread");
    }
    delete data;
    return;
  }
#endif

  if (!threadHandle) {
    if (callback) {
      callback(false, 0, "Failed to create scan thread");
    }
    delete data;
    return;
  }

  data->threadHandle = threadHandle;
}

void MagdaPluginScanner::RebuildAliasMap() {
  m_aliasesByPlugin.clear();
  for (const auto &pair : m_aliases) {
    const std::string &alias = pair.first;
    const std::string &full_name = pair.second;
    m_aliasesByPlugin[full_name].push_back(alias);
  }
}

bool MagdaPluginScanner::LoadAliasesFromCache() {
  if (!g_rec) {
    return false;
  }

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState) {
    return false;
  }

  const char *aliases_json = GetExtState("MAGDA", "plugin_aliases");
  if (!aliases_json || strlen(aliases_json) == 0) {
    return false;
  }

  // Parse JSON: {"alias": "full_name", ...}
  m_aliases.clear();
  const char *p = aliases_json;
  while (*p && *p != '{')
    p++;
  if (*p != '{')
    return false;
  p++;

  while (*p && *p != '}') {
    while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
      p++;
    if (*p == '}')
      break;

    if (*p == '"') {
      p++;
      const char *key_start = p;
      while (*p && *p != '"') {
        if (*p == '\\')
          p++;
        p++;
      }
      if (*p != '"')
        break;
      std::string alias(key_start, p - key_start);
      p++;

      while (*p &&
             (*p == ' ' || *p == ':' || *p == '\n' || *p == '\r' || *p == '\t'))
        p++;
      if (*p != '"')
        break;
      p++;
      const char *val_start = p;
      while (*p && *p != '"') {
        if (*p == '\\')
          p++;
        p++;
      }
      if (*p != '"')
        break;
      std::string full_name(val_start, p - val_start);
      p++;

      m_aliases[alias] = full_name;
    } else {
      p++;
    }
  }

  RebuildAliasMap();
  return true;
}

bool MagdaPluginScanner::SaveAliasesToCache() const {
  if (!g_rec) {
    return false;
  }

  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState) {
    return false;
  }

  // Build JSON: {"alias": "full_name", ...}
  std::string json = "{";
  size_t i = 0;
  for (const auto &pair : m_aliases) {
    if (i > 0) {
      json += ",";
    }
    json += "\"";
    // Escape alias
    for (char c : pair.first) {
      if (c == '"' || c == '\\') {
        json += "\\";
      }
      json += c;
    }
    json += "\":\"";
    // Escape full_name
    for (char c : pair.second) {
      if (c == '"' || c == '\\') {
        json += "\\";
      }
      json += c;
    }
    json += "\"";
    i++;
  }
  json += "}";

  SetExtState("MAGDA", "plugin_aliases", json.c_str(), true);
  return true;
}

std::string MagdaPluginScanner::ResolveAlias(const char *alias) const {
  if (!alias) {
    return "";
  }

  std::string alias_lower = alias;
  std::transform(alias_lower.begin(), alias_lower.end(), alias_lower.begin(),
                 ::tolower);

  for (const auto &pair : m_aliases) {
    std::string key_lower = pair.first;
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(),
                   ::tolower);
    if (key_lower == alias_lower) {
      return pair.second;
    }
  }

  return alias; // Return as-is if not found
}

void MagdaPluginScanner::SetPluginAliases(
    const char *full_name, const std::vector<std::string> &aliases) {
  if (!full_name) {
    return;
  }

  // Remove existing aliases for this plugin
  auto it = m_aliasesByPlugin.find(full_name);
  if (it != m_aliasesByPlugin.end()) {
    for (const auto &alias : it->second) {
      m_aliases.erase(alias);
    }
  }

  // Add new aliases
  m_aliasesByPlugin[full_name] = aliases;
  for (const auto &alias : aliases) {
    m_aliases[alias] = full_name;
  }

  SaveAliasesToCache();
}

void MagdaPluginScanner::AddPluginAlias(const char *full_name,
                                        const char *alias) {
  if (!full_name || !alias) {
    return;
  }

  m_aliases[alias] = full_name;
  m_aliasesByPlugin[full_name].push_back(alias);
  SaveAliasesToCache();
}

void MagdaPluginScanner::RemovePluginAlias(const char *full_name,
                                           const char *alias) {
  if (!full_name || !alias) {
    return;
  }

  m_aliases.erase(alias);
  auto it = m_aliasesByPlugin.find(full_name);
  if (it != m_aliasesByPlugin.end()) {
    it->second.erase(std::remove(it->second.begin(), it->second.end(), alias),
                     it->second.end());
    if (it->second.empty()) {
      m_aliasesByPlugin.erase(it);
    }
  }
  SaveAliasesToCache();
}

void MagdaPluginScanner::SetAliasForPlugin(const std::string &plugin_key,
                                           const std::string &alias) {
  if (plugin_key.empty()) {
    return;
  }

  // Remove old aliases for this plugin from the forward mapping
  auto it = m_aliasesByPlugin.find(plugin_key);
  if (it != m_aliasesByPlugin.end()) {
    for (const auto &old_alias : it->second) {
      m_aliases.erase(old_alias);
    }
  }

  // Set new alias (single alias, replaces all existing)
  m_aliasesByPlugin[plugin_key].clear();
  if (!alias.empty()) {
    m_aliasesByPlugin[plugin_key].push_back(alias);
    m_aliases[alias] = plugin_key;
  }

  // Save immediately
  SaveAliasesToCache();
}
