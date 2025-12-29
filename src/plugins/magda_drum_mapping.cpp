#include "magda_drum_mapping.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <shlobj.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <pwd.h>
#include <unistd.h>
#endif

// Global instance
DrumMappingManager *g_drumMappingManager = nullptr;

// Helper to convert string to lowercase
static std::string ToLower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// Check if a plugin is a drum plugin by name
bool IsDrumPlugin(const std::string &plugin_name) {
  std::string name_lower = ToLower(plugin_name);

  // Common drum plugin indicators
  static const char *drum_indicators[] = {"drum",
                                          "drums",
                                          "drummer",
                                          "battery",
                                          "percussion",
                                          "beat",
                                          "groove",
                                          "kit",
                                          "snare",
                                          "cymbal",
                                          "addictive drums",
                                          "superior drummer",
                                          "ezdrummer",
                                          "bfd",
                                          "steven slate drums",
                                          "ssd",
                                          "getgood drums",
                                          "ggd",
                                          "toontrack",
                                          nullptr};

  for (int i = 0; drum_indicators[i] != nullptr; i++) {
    if (name_lower.find(drum_indicators[i]) != std::string::npos) {
      return true;
    }
  }

  return false;
}

// ============================================================================
// DrumMappingManager Implementation
// ============================================================================

DrumMappingManager::DrumMappingManager() {
  LoadFromCache();
}

DrumMappingManager::~DrumMappingManager() {}

const char *DrumMappingManager::GetCacheFilePath() {
  static char path[1024] = {0};
  if (path[0] == '\0') {
#ifdef _WIN32
    char *appdata = getenv("APPDATA");
    if (appdata) {
      snprintf(path, sizeof(path), "%s\\MAGDA\\drum_mappings.json", appdata);
    }
#else
    const char *home = getenv("HOME");
    if (!home) {
      struct passwd *pw = getpwuid(getuid());
      if (pw) {
        home = pw->pw_dir;
      }
    }
    if (home) {
      snprintf(path, sizeof(path), "%s/.magda/drum_mappings.json", home);
    }
#endif
  }
  return path;
}

bool DrumMappingManager::LoadFromCache() {
  const char *cache_path = GetCacheFilePath();
  if (!cache_path || cache_path[0] == '\0') {
    return false;
  }

  std::ifstream file(cache_path);
  if (!file.is_open()) {
    // No cache file - load presets as defaults
    m_mappings = GetPresetMappings();
    return true;
  }

  // Simple JSON parsing for drum mappings
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();

  m_mappings.clear();

  // Parse JSON array of mappings
  // Format: [{"id":"...", "name":"...", "plugin_key":"...", "is_preset":bool,
  // "notes":{"kick":36,...}}]
  size_t pos = content.find('[');
  if (pos == std::string::npos) {
    m_mappings = GetPresetMappings();
    return true;
  }

  // Find each mapping object
  size_t obj_start = content.find('{', pos);
  while (obj_start != std::string::npos) {
    size_t obj_end = content.find('}', obj_start);
    // Find the closing brace that matches (account for nested notes object)
    int brace_count = 1;
    size_t search_pos = obj_start + 1;
    while (brace_count > 0 && search_pos < content.length()) {
      if (content[search_pos] == '{')
        brace_count++;
      else if (content[search_pos] == '}')
        brace_count--;
      search_pos++;
    }
    obj_end = search_pos - 1;

    if (obj_end == std::string::npos)
      break;

    std::string obj = content.substr(obj_start, obj_end - obj_start + 1);

    DrumMapping mapping;

    // Parse id
    size_t id_pos = obj.find("\"id\"");
    if (id_pos != std::string::npos) {
      size_t val_start = obj.find(':', id_pos) + 1;
      while (val_start < obj.length() && obj[val_start] != '"')
        val_start++;
      val_start++;
      size_t val_end = obj.find('"', val_start);
      mapping.id = obj.substr(val_start, val_end - val_start);
    }

    // Parse name
    size_t name_pos = obj.find("\"name\"");
    if (name_pos != std::string::npos) {
      size_t val_start = obj.find(':', name_pos) + 1;
      while (val_start < obj.length() && obj[val_start] != '"')
        val_start++;
      val_start++;
      size_t val_end = obj.find('"', val_start);
      mapping.name = obj.substr(val_start, val_end - val_start);
    }

    // Parse plugin_key
    size_t pk_pos = obj.find("\"plugin_key\"");
    if (pk_pos != std::string::npos) {
      size_t val_start = obj.find(':', pk_pos) + 1;
      while (val_start < obj.length() && obj[val_start] != '"')
        val_start++;
      val_start++;
      size_t val_end = obj.find('"', val_start);
      mapping.plugin_key = obj.substr(val_start, val_end - val_start);
    }

    // Parse is_preset
    size_t preset_pos = obj.find("\"is_preset\"");
    if (preset_pos != std::string::npos) {
      mapping.is_preset = obj.find("true", preset_pos) != std::string::npos;
    }

    // Parse notes
    size_t notes_pos = obj.find("\"notes\"");
    if (notes_pos != std::string::npos) {
      size_t notes_start = obj.find('{', notes_pos);
      size_t notes_end = obj.find('}', notes_start);
      if (notes_start != std::string::npos && notes_end != std::string::npos) {
        std::string notes_obj = obj.substr(notes_start + 1, notes_end - notes_start - 1);

        // Parse each note: "drum_name":midi_note
        size_t note_pos = 0;
        while ((note_pos = notes_obj.find('"', note_pos)) != std::string::npos) {
          size_t key_start = note_pos + 1;
          size_t key_end = notes_obj.find('"', key_start);
          if (key_end == std::string::npos)
            break;

          std::string drum_name = notes_obj.substr(key_start, key_end - key_start);

          size_t colon_pos = notes_obj.find(':', key_end);
          if (colon_pos == std::string::npos)
            break;

          // Find the number
          size_t num_start = colon_pos + 1;
          while (num_start < notes_obj.length() &&
                 (notes_obj[num_start] == ' ' || notes_obj[num_start] == '\t'))
            num_start++;

          int note_num = atoi(notes_obj.c_str() + num_start);
          mapping.notes[drum_name] = note_num;

          note_pos = key_end + 1;
        }
      }
    }

    if (!mapping.id.empty()) {
      m_mappings.push_back(mapping);
    }

    obj_start = content.find('{', obj_end + 1);
  }

  // Ensure presets are included
  auto presets = GetPresetMappings();
  for (const auto &preset : presets) {
    bool found = false;
    for (const auto &m : m_mappings) {
      if (m.id == preset.id) {
        found = true;
        break;
      }
    }
    if (!found) {
      m_mappings.push_back(preset);
    }
  }

  return true;
}

bool DrumMappingManager::SaveToCache() const {
  const char *cache_path = GetCacheFilePath();
  if (!cache_path || cache_path[0] == '\0') {
    return false;
  }

  // Ensure directory exists
  std::string dir_path = cache_path;
  size_t last_slash = dir_path.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    dir_path = dir_path.substr(0, last_slash);
    mkdir(dir_path.c_str(), 0755);
  }

  std::ofstream file(cache_path);
  if (!file.is_open()) {
    return false;
  }

  file << "[\n";
  for (size_t i = 0; i < m_mappings.size(); i++) {
    const auto &m = m_mappings[i];
    file << "  {\n";
    file << "    \"id\": \"" << m.id << "\",\n";
    file << "    \"name\": \"" << m.name << "\",\n";
    file << "    \"plugin_key\": \"" << m.plugin_key << "\",\n";
    file << "    \"is_preset\": " << (m.is_preset ? "true" : "false") << ",\n";
    file << "    \"notes\": {\n";

    size_t note_idx = 0;
    for (const auto &note : m.notes) {
      file << "      \"" << note.first << "\": " << note.second;
      if (note_idx < m.notes.size() - 1)
        file << ",";
      file << "\n";
      note_idx++;
    }

    file << "    }\n";
    file << "  }";
    if (i < m_mappings.size() - 1)
      file << ",";
    file << "\n";
  }
  file << "]\n";

  file.close();
  return true;
}

const DrumMapping *DrumMappingManager::GetMapping(const std::string &id) const {
  for (const auto &m : m_mappings) {
    if (m.id == id) {
      return &m;
    }
  }
  return nullptr;
}

const DrumMapping *DrumMappingManager::GetMappingForPlugin(const std::string &plugin_key) const {
  for (const auto &m : m_mappings) {
    if (m.plugin_key == plugin_key) {
      return &m;
    }
  }
  return nullptr;
}

void DrumMappingManager::SetMapping(const DrumMapping &mapping) {
  // Update existing or add new
  for (auto &m : m_mappings) {
    if (m.id == mapping.id) {
      m = mapping;
      SaveToCache();
      return;
    }
  }
  m_mappings.push_back(mapping);
  SaveToCache();
}

void DrumMappingManager::RemoveMapping(const std::string &id) {
  m_mappings.erase(std::remove_if(m_mappings.begin(), m_mappings.end(),
                                  [&id](const DrumMapping &m) { return m.id == id; }),
                   m_mappings.end());
  SaveToCache();
}

bool DrumMappingManager::HasMappingForPlugin(const std::string &plugin_key) const {
  return GetMappingForPlugin(plugin_key) != nullptr;
}

DrumMapping DrumMappingManager::CreateMappingFromPreset(const std::string &preset_id,
                                                        const std::string &plugin_key,
                                                        const std::string &name) {
  DrumMapping mapping;
  mapping.id = plugin_key; // Use plugin key as unique ID
  mapping.name = name;
  mapping.plugin_key = plugin_key;
  mapping.is_preset = false;

  // Copy notes from preset
  const DrumMapping *preset = GetMapping(preset_id);
  if (preset) {
    mapping.notes = preset->notes;
  } else {
    // Default to General MIDI
    auto presets = GetPresetMappings();
    for (const auto &p : presets) {
      if (p.id == "general_midi") {
        mapping.notes = p.notes;
        break;
      }
    }
  }

  return mapping;
}

std::vector<DrumMapping> DrumMappingManager::GetPresetMappings() {
  std::vector<DrumMapping> presets;

  // General MIDI Drums
  {
    DrumMapping gm;
    gm.id = "general_midi";
    gm.name = "General MIDI";
    gm.is_preset = true;
    gm.notes = {{CanonicalDrums::KICK, 36},         {CanonicalDrums::SNARE, 38},
                {CanonicalDrums::SNARE_RIM, 40},    {CanonicalDrums::SNARE_XSTICK, 37},
                {CanonicalDrums::HI_HAT, 42},       {CanonicalDrums::HI_HAT_OPEN, 46},
                {CanonicalDrums::HI_HAT_PEDAL, 44}, {CanonicalDrums::TOM_HIGH, 50},
                {CanonicalDrums::TOM_MID, 47},      {CanonicalDrums::TOM_LOW, 45},
                {CanonicalDrums::CRASH, 49},        {CanonicalDrums::RIDE, 51},
                {CanonicalDrums::RIDE_BELL, 53}};
    presets.push_back(gm);
  }

  // Addictive Drums 2
  {
    DrumMapping ad2;
    ad2.id = "addictive_drums_v2";
    ad2.name = "Addictive Drums 2";
    ad2.is_preset = true;
    ad2.notes = {{CanonicalDrums::KICK, 36},         {CanonicalDrums::SNARE, 38},
                 {CanonicalDrums::SNARE_RIM, 40},    {CanonicalDrums::SNARE_XSTICK, 37},
                 {CanonicalDrums::HI_HAT, 42},       {CanonicalDrums::HI_HAT_OPEN, 46},
                 {CanonicalDrums::HI_HAT_PEDAL, 44}, {CanonicalDrums::TOM_HIGH, 50},
                 {CanonicalDrums::TOM_MID, 47},      {CanonicalDrums::TOM_LOW, 45},
                 {CanonicalDrums::CRASH, 49},        {CanonicalDrums::RIDE, 51},
                 {CanonicalDrums::RIDE_BELL, 53}};
    presets.push_back(ad2);
  }

  // Superior Drummer 3
  {
    DrumMapping sd3;
    sd3.id = "superior_drummer_v3";
    sd3.name = "Superior Drummer 3";
    sd3.is_preset = true;
    sd3.notes = {{CanonicalDrums::KICK, 36},         {CanonicalDrums::SNARE, 38},
                 {CanonicalDrums::SNARE_RIM, 40},    {CanonicalDrums::SNARE_XSTICK, 37},
                 {CanonicalDrums::HI_HAT, 42},       {CanonicalDrums::HI_HAT_OPEN, 46},
                 {CanonicalDrums::HI_HAT_PEDAL, 44}, {CanonicalDrums::TOM_HIGH, 50},
                 {CanonicalDrums::TOM_MID, 47},      {CanonicalDrums::TOM_LOW, 41},
                 {CanonicalDrums::CRASH, 49},        {CanonicalDrums::RIDE, 51},
                 {CanonicalDrums::RIDE_BELL, 53}};
    presets.push_back(sd3);
  }

  // EZdrummer 3
  {
    DrumMapping ezd;
    ezd.id = "ezdrummer_v3";
    ezd.name = "EZdrummer 3";
    ezd.is_preset = true;
    ezd.notes = {{CanonicalDrums::KICK, 36},         {CanonicalDrums::SNARE, 38},
                 {CanonicalDrums::SNARE_RIM, 40},    {CanonicalDrums::SNARE_XSTICK, 37},
                 {CanonicalDrums::HI_HAT, 42},       {CanonicalDrums::HI_HAT_OPEN, 46},
                 {CanonicalDrums::HI_HAT_PEDAL, 44}, {CanonicalDrums::TOM_HIGH, 48},
                 {CanonicalDrums::TOM_MID, 47},      {CanonicalDrums::TOM_LOW, 45},
                 {CanonicalDrums::CRASH, 49},        {CanonicalDrums::RIDE, 51},
                 {CanonicalDrums::RIDE_BELL, 53}};
    presets.push_back(ezd);
  }

  return presets;
}
