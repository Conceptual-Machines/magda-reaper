#include "magda_http_server.h"
#include "../WDL/WDL/jnetlib/webserver.h"
#include "../WDL/WDL/wdlstring.h"
#include "magda_state.h"
#include <cstring>

// Simple page generator for JSON responses
class JSONPageGenerator : public IPageGenerator {
public:
  JSONPageGenerator(const char *json) {
    if (json) {
      m_json.Set(json);
    }
    m_pos = 0;
  }

  virtual ~JSONPageGenerator() {}

  virtual int GetData(char *buf, int size) {
    int remaining = m_json.GetLength() - m_pos;
    if (remaining <= 0)
      return -1; // Done

    int to_copy = remaining < size ? remaining : size;
    memcpy(buf, m_json.Get() + m_pos, to_copy);
    m_pos += to_copy;
    return to_copy;
  }

private:
  WDL_FastString m_json;
  int m_pos;
};

MagdaHTTPServer::MagdaHTTPServer() : m_running(false), m_port(8081) {}

MagdaHTTPServer::~MagdaHTTPServer() { Stop(); }

bool MagdaHTTPServer::Start(int port) {
  if (m_running) {
    Stop();
  }

  m_port = port;
  if (addListenPort(port) < 0) {
    return false;
  }

  m_running = true;
  return true;
}

void MagdaHTTPServer::Stop() {
  if (!m_running)
    return;

  // Remove all listen ports
  int idx = 0;
  while (getListenPort(idx) >= 0) {
    removeListenIdx(idx);
  }

  m_running = false;
}

int MagdaHTTPServer::GetPort() const { return m_port; }

void MagdaHTTPServer::SendJSONResponse(JNL_HTTPServ *serv, const char *json, int status) {
  if (status == 200) {
    serv->set_reply_string("HTTP/1.1 200 OK");
  } else {
    char buf[64];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d", status);
    serv->set_reply_string(buf);
  }

  serv->set_reply_header("Content-Type: application/json");
  serv->set_reply_header("Access-Control-Allow-Origin: *"); // CORS
  serv->set_reply_size((int)strlen(json));
  serv->send_reply();
}

void MagdaHTTPServer::SendErrorResponse(JNL_HTTPServ *serv, const char *message, int status) {
  char json[512];
  snprintf(json, sizeof(json), "{\"error\":\"%s\"}", message);
  SendJSONResponse(serv, json, status);
}

IPageGenerator *MagdaHTTPServer::HandleGetState(JNL_HTTPServ *serv) {
  char *state_json = MagdaState::GetStateSnapshot();
  if (!state_json) {
    SendErrorResponse(serv, "Failed to get state", 500);
    return nullptr;
  }

  SendJSONResponse(serv, state_json, 200);
  JSONPageGenerator *gen = new JSONPageGenerator(state_json);
  free(state_json);
  return gen;
}

IPageGenerator *MagdaHTTPServer::HandleGetTracks(JNL_HTTPServ *serv) {
  WDL_FastString json;
  json.Append("{\"tracks\":");
  MagdaState::GetTracksInfo(json);
  json.Append("}");

  SendJSONResponse(serv, json.Get(), 200);
  return new JSONPageGenerator(json.Get());
}

IPageGenerator *MagdaHTTPServer::HandleGetPlayState(JNL_HTTPServ *serv) {
  WDL_FastString json;
  json.Append("{");
  MagdaState::GetPlayState(json);
  json.Append("}");

  SendJSONResponse(serv, json.Get(), 200);
  return new JSONPageGenerator(json.Get());
}

IPageGenerator *MagdaHTTPServer::onConnection(JNL_HTTPServ *serv, int port) {
  // Set CORS headers
  serv->set_reply_header("Access-Control-Allow-Origin: *");
  serv->set_reply_header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
  serv->set_reply_header("Access-Control-Allow-Headers: Content-Type");

  // Get request file (path)
  const char *request_file = serv->get_request_file();
  if (!request_file) {
    SendErrorResponse(serv, "Invalid request", 400);
    return nullptr;
  }

  // Route requests
  if (strcmp(request_file, "/api/state") == 0) {
    return HandleGetState(serv);
  } else if (strcmp(request_file, "/api/tracks") == 0) {
    return HandleGetTracks(serv);
  } else if (strcmp(request_file, "/api/play-state") == 0) {
    return HandleGetPlayState(serv);
  } else if (strcmp(request_file, "/health") == 0) {
    const char *health = "{\"status\":\"ok\"}";
    SendJSONResponse(serv, health, 200);
    return new JSONPageGenerator(health);
  } else {
    SendErrorResponse(serv, "Not found", 404);
    return nullptr;
  }
}
