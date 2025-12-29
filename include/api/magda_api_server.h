#pragma once

#include "../WDL/WDL/jnetlib/webserver.h"
#include "../WDL/WDL/jsonparse.h"
#include "reaper_plugin.h"

// HTTP API server for MAGDA extension
// Provides GET /api/state and POST /api/actions endpoints
class MagdaAPIServer : public WebServerBaseClass {
public:
  MagdaAPIServer();
  ~MagdaAPIServer();

  // Start the server on the specified port
  bool Start(int port = 8080);

  // Stop the server
  void Stop();

  // Check if server is running
  bool IsRunning() const { return m_running; }

  // Get the port the server is listening on
  int GetPort() const;

  // Override from WebServerBaseClass
  virtual IPageGenerator *onConnection(JNL_HTTPServ *serv, int port) override;

private:
  bool m_running;
  int m_port;

  // Handle GET /api/state
  IPageGenerator *HandleGetState(JNL_HTTPServ *serv);

  // Handle POST /api/actions
  IPageGenerator *HandlePostActions(JNL_HTTPServ *serv);

  // Helper to read POST body
  bool ReadPostBody(JNL_HTTPServ *serv, WDL_FastString &body);

  // Helper to send JSON response
  void SendJSONResponse(JNL_HTTPServ *serv, const char *json, int status = 200);

  // Helper to send error response
  void SendErrorResponse(JNL_HTTPServ *serv, const char *message, int status = 400);
};
