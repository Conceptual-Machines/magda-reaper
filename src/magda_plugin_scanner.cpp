#include "magda_plugin_scanner.h"
#include "reaper_plugin.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "../WDL/WDL/wdlcstring.h"
#include "reaper_plugin_functions.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern reaper_plugin_info_t *g_rec;

MagdaPluginScanner::MagdaPluginScanner() : m_initialized(false) {}

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
    const char *open_paren = strchr(name_start, '(');
    if (open_paren) {
      // Extract name (everything before opening paren, trimmed)
      int name_len = open_paren - name_start;
      while (name_len > 0 && name_start[name_len - 1] == ' ') {
        name_len--;
      }
      info.name.assign(name_start, name_len);

      // Extract manufacturer (everything between parentheses)
      const char *close_paren = strchr(open_paren, ')');
      if (close_paren) {
        int mfr_len = close_paren - open_paren - 1;
        info.manufacturer.assign(open_paren + 1, mfr_len);
      } else {
        // No closing paren, use rest of string
        info.manufacturer = open_paren + 1;
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
