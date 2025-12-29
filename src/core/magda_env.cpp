#include "magda_env.h"
#include "../WDL/WDL/wdlstring.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#include <shlobj.h>
#include <windows.h>
#endif

static WDL_FastString s_email;
static WDL_FastString s_password;
static WDL_FastString s_backend_url;
bool MagdaEnv::s_envLoaded = false;

void MagdaEnv::LoadEnvFile() {
  if (s_envLoaded)
    return;
  s_envLoaded = true;

  // Try to find .env file - look in current directory and parent directories
  const char *envPaths[] = {".env", "../.env", "../../.env", "../../../.env", nullptr};

  FILE *fp = nullptr;
  for (int i = 0; envPaths[i]; i++) {
    fp = fopen(envPaths[i], "r");
    if (fp) {
      break;
    }
  }

  if (!fp) {
    // Try in Reaper UserPlugins directory (where the extension is loaded from)
    const char *home = getenv("HOME");
    if (!home) {
#ifdef _WIN32
      char path[MAX_PATH];
      if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, path))) {
        home = path;
      }
#endif
    }
    if (home) {
      // macOS: ~/Library/Application Support/REAPER/UserPlugins/.env
      // Linux: ~/.config/REAPER/UserPlugins/.env
      // Windows: %APPDATA%\REAPER\UserPlugins\.env
#ifdef _WIN32
      char envPath[MAX_PATH];
      snprintf(envPath, sizeof(envPath), "%s\\AppData\\Roaming\\REAPER\\UserPlugins\\.env", home);
#elif defined(__APPLE__)
      char envPath[512];
      snprintf(envPath, sizeof(envPath), "%s/Library/Application Support/REAPER/UserPlugins/.env",
               home);
#else
      char envPath[512];
      snprintf(envPath, sizeof(envPath), "%s/.config/REAPER/UserPlugins/.env", home);
#endif
      fp = fopen(envPath, "r");
    }
  }

  if (!fp) {
    // Try in home directory as fallback
    const char *home = getenv("HOME");
    if (!home) {
#ifdef _WIN32
      char path[MAX_PATH];
      if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, path))) {
        home = path;
      }
#endif
    }
    if (home) {
      char envPath[512];
      snprintf(envPath, sizeof(envPath), "%s/.env", home);
      fp = fopen(envPath, "r");
    }
  }

  if (!fp) {
    return; // No .env file found
  }

  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    // Remove newline
    int len = (int)strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }
    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
      len--;
    }

    // Skip empty lines and comments
    if (len == 0 || line[0] == '#') {
      continue;
    }

    // Find equals sign
    char *equals = strchr(line, '=');
    if (!equals) {
      continue;
    }

    *equals = '\0';
    char *key = line;
    char *value = equals + 1;

    // Trim whitespace from key
    while (*key == ' ' || *key == '\t')
      key++;
    char *keyEnd = key + strlen(key) - 1;
    while (keyEnd > key && (*keyEnd == ' ' || *keyEnd == '\t')) {
      *keyEnd = '\0';
      keyEnd--;
    }

    // Trim whitespace and quotes from value
    while (*value == ' ' || *value == '\t')
      value++;
    char *valueEnd = value + strlen(value) - 1;
    while (valueEnd > value && (*valueEnd == ' ' || *valueEnd == '\t')) {
      *valueEnd = '\0';
      valueEnd--;
    }
    // Remove quotes if present
    if ((value[0] == '"' && valueEnd > value && *valueEnd == '"') ||
        (value[0] == '\'' && valueEnd > value && *valueEnd == '\'')) {
      value++;
      *valueEnd = '\0';
    }

    // Store specific keys we care about
    if (strcmp(key, "MAGDA_EMAIL") == 0) {
      s_email.Set(value);
    } else if (strcmp(key, "MAGDA_PASSWORD") == 0) {
      s_password.Set(value);
    } else if (strcmp(key, "MAGDA_BACKEND_URL") == 0) {
      s_backend_url.Set(value);
    }
  }

  fclose(fp);
}

const char *MagdaEnv::Get(const char *key, const char *defaultValue) {
  if (!key) {
    return defaultValue;
  }

  // First check actual environment variable
  const char *envVal = getenv(key);
  if (envVal && strlen(envVal) > 0) {
    return envVal;
  }

  // Load .env file if not already loaded
  LoadEnvFile();

  // Check .env file for specific keys
  if (strcmp(key, "MAGDA_EMAIL") == 0) {
    return s_email.GetLength() > 0 ? s_email.Get() : defaultValue;
  } else if (strcmp(key, "MAGDA_PASSWORD") == 0) {
    return s_password.GetLength() > 0 ? s_password.Get() : defaultValue;
  } else if (strcmp(key, "MAGDA_BACKEND_URL") == 0) {
    return s_backend_url.GetLength() > 0 ? s_backend_url.Get() : defaultValue;
  }

  return defaultValue;
}
