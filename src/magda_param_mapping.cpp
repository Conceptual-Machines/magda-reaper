#include "magda_param_mapping.h"
#include "reaper_plugin.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

extern reaper_plugin_info_t *g_rec;

// Global instance
ParamMappingManager *g_paramMappingManager = nullptr;

// Helper: Get config directory
static std::string GetConfigDirectory() {
#ifdef _WIN32
  const char *appdata = getenv("APPDATA");
  if (appdata) {
    return std::string(appdata) + "\\MAGDA";
  }
  return ".";
#else
  const char *home = getenv("HOME");
  if (home) {
    return std::string(home) + "/.magda";
  }
  return ".";
#endif
}

// Helper: Ensure config directory exists
static bool EnsureConfigDirectory() {
  std::string dir = GetConfigDirectory();
#ifdef _WIN32
  _mkdir(dir.c_str());
#else
  mkdir(dir.c_str(), 0755);
#endif
  return true;
}

std::string ParamMappingManager::GetMappingsFilePath() {
  return GetConfigDirectory() + "/param_mappings.json";
}

ParamMappingManager::ParamMappingManager() { LoadMappings(); }

ParamMappingManager::~ParamMappingManager() { SaveMappings(); }

bool ParamMappingManager::LoadMappings() {
  std::string filepath = GetMappingsFilePath();
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return false;
  }

  // Simple JSON parsing (manual for minimal dependencies)
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  file.close();

  // Parse JSON array of mappings
  // Format: [{"plugin_key":"...", "plugin_name":"...",
  // "aliases":{"cutoff":42}}]
  m_mappings.clear();

  size_t pos = 0;
  while ((pos = content.find("\"plugin_key\"", pos)) != std::string::npos) {
    ParamMapping mapping;

    // Find plugin_key value
    size_t keyStart = content.find(':', pos) + 1;
    size_t keyQuoteStart = content.find('"', keyStart) + 1;
    size_t keyQuoteEnd = content.find('"', keyQuoteStart);
    mapping.plugin_key =
        content.substr(keyQuoteStart, keyQuoteEnd - keyQuoteStart);

    // Find plugin_name value
    size_t namePos = content.find("\"plugin_name\"", keyQuoteEnd);
    if (namePos != std::string::npos &&
        namePos < content.find('}', keyQuoteEnd) + 100) {
      size_t nameStart = content.find(':', namePos) + 1;
      size_t nameQuoteStart = content.find('"', nameStart) + 1;
      size_t nameQuoteEnd = content.find('"', nameQuoteStart);
      mapping.plugin_name =
          content.substr(nameQuoteStart, nameQuoteEnd - nameQuoteStart);
    }

    // Find aliases object
    size_t aliasesPos = content.find("\"aliases\"", keyQuoteEnd);
    if (aliasesPos != std::string::npos) {
      size_t aliasesStart = content.find('{', aliasesPos);
      size_t aliasesEnd = content.find('}', aliasesStart);
      std::string aliasesStr =
          content.substr(aliasesStart, aliasesEnd - aliasesStart + 1);

      // Parse alias entries
      size_t aliasPos = 0;
      while ((aliasPos = aliasesStr.find('"', aliasPos)) != std::string::npos) {
        size_t aliasNameStart = aliasPos + 1;
        size_t aliasNameEnd = aliasesStr.find('"', aliasNameStart);
        if (aliasNameEnd == std::string::npos)
          break;

        std::string aliasName =
            aliasesStr.substr(aliasNameStart, aliasNameEnd - aliasNameStart);

        size_t colonPos = aliasesStr.find(':', aliasNameEnd);
        if (colonPos == std::string::npos)
          break;

        // Parse integer value
        size_t valueStart = colonPos + 1;
        while (
            valueStart < aliasesStr.length() &&
            (aliasesStr[valueStart] == ' ' || aliasesStr[valueStart] == '\t')) {
          valueStart++;
        }
        size_t valueEnd = valueStart;
        while (valueEnd < aliasesStr.length() &&
               (isdigit(aliasesStr[valueEnd]) || aliasesStr[valueEnd] == '-')) {
          valueEnd++;
        }

        if (valueEnd > valueStart) {
          int paramIndex =
              std::stoi(aliasesStr.substr(valueStart, valueEnd - valueStart));
          mapping.aliases[aliasName] = paramIndex;
        }

        aliasPos = valueEnd;
      }
    }

    if (!mapping.plugin_key.empty()) {
      m_mappings[mapping.plugin_key] = mapping;
    }

    pos = keyQuoteEnd + 1;
  }

  return true;
}

bool ParamMappingManager::SaveMappings() const {
  EnsureConfigDirectory();
  std::string filepath = GetMappingsFilePath();
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return false;
  }

  file << "[\n";
  bool first = true;
  for (const auto &pair : m_mappings) {
    const ParamMapping &mapping = pair.second;

    if (!first) {
      file << ",\n";
    }
    first = false;

    file << "  {\n";
    file << "    \"plugin_key\": \"" << mapping.plugin_key << "\",\n";
    file << "    \"plugin_name\": \"" << mapping.plugin_name << "\",\n";
    file << "    \"aliases\": {";

    bool firstAlias = true;
    for (const auto &alias : mapping.aliases) {
      if (!firstAlias) {
        file << ",";
      }
      firstAlias = false;
      file << "\n      \"" << alias.first << "\": " << alias.second;
    }

    if (!mapping.aliases.empty()) {
      file << "\n    ";
    }
    file << "}\n";
    file << "  }";
  }
  file << "\n]\n";

  file.close();
  return true;
}

const ParamMapping *
ParamMappingManager::GetMappingForPlugin(const std::string &plugin_key) const {
  auto it = m_mappings.find(plugin_key);
  if (it != m_mappings.end()) {
    return &it->second;
  }
  return nullptr;
}

void ParamMappingManager::SetMapping(const ParamMapping &mapping) {
  m_mappings[mapping.plugin_key] = mapping;
  SaveMappings();
}

void ParamMappingManager::RemoveMapping(const std::string &plugin_key) {
  m_mappings.erase(plugin_key);
  SaveMappings();
}

int ParamMappingManager::ResolveParamAlias(const std::string &plugin_key,
                                           const std::string &alias) const {
  const ParamMapping *mapping = GetMappingForPlugin(plugin_key);
  if (!mapping) {
    return -1;
  }

  // Case-insensitive lookup
  std::string aliasLower = alias;
  std::transform(aliasLower.begin(), aliasLower.end(), aliasLower.begin(),
                 ::tolower);

  for (const auto &pair : mapping->aliases) {
    std::string keyLower = pair.first;
    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(),
                   ::tolower);
    if (keyLower == aliasLower) {
      return pair.second;
    }
  }

  return -1;
}
