#pragma once

#include "../WDL/WDL/wdlstring.h"

// Authentication service for MAGDA
// Handles async login with background thread
class MagdaAuth {
public:
  // Callback type for login completion
  // Called from background thread - should use PostMessage to update UI
  typedef void (*LoginCallback)(bool success, const char *token, const char *error);

  // Perform login asynchronously in background thread
  // callback will be invoked from the background thread when login completes
  static void LoginAsync(const char *email, const char *password, LoginCallback callback);

  // Get stored JWT token
  static const char *GetStoredToken();

  // Store JWT token
  static void StoreToken(const char *token);

private:
  // Static storage for JWT token
  static WDL_FastString &GetTokenStorage();
};
