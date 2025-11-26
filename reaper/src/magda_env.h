#pragma once

// Simple .env file reader for development
// Reads AIDEAS_EMAIL and AIDEAS_PASSWORD from .env file in the project root
class MagdaEnv {
public:
  // Get environment variable value, with optional default
  // First checks actual environment variables, then tries to read from .env file
  static const char *Get(const char *key, const char *defaultValue = "");

private:
  // Load .env file once
  static void LoadEnvFile();
  static bool s_envLoaded;
};
