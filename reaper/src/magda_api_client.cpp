#include "magda_api_client.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/timing.h"
#include "magda_actions.h"
#include "magda_state.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

MagdaHTTPClient::MagdaHTTPClient() : m_http_get(nullptr), m_dns(nullptr) {
  m_dns = new JNL_AsyncDNS;
  m_backend_url.Set("http://localhost:8080");
  // JNL_HTTPGet will use the DNS object
}

MagdaHTTPClient::~MagdaHTTPClient() {
  if (m_http_get) {
    delete m_http_get;
  }
  if (m_dns) {
    delete m_dns;
  }
}

void MagdaHTTPClient::SetBackendURL(const char *url) {
  if (url) {
    m_backend_url.Set(url);
  }
}

void MagdaHTTPClient::SetJWTToken(const char *token) {
  if (token) {
    m_jwt_token.Set(token);
  } else {
    m_jwt_token.Set("");
  }
}

char *MagdaHTTPClient::BuildRequestJSON(const char *question) {
  WDL_FastString json;
  json.Append("{\"question\":");

  // Escape question string
  json.Append("\"");
  const char *p = question;
  while (p && *p) {
    switch (*p) {
    case '"':
      json.Append("\\\"");
      break;
    case '\\':
      json.Append("\\\\");
      break;
    case '\n':
      json.Append("\\n");
      break;
    case '\r':
      json.Append("\\r");
      break;
    case '\t':
      json.Append("\\t");
      break;
    default:
      json.Append(p, 1);
      break;
    }
    p++;
  }
  json.Append("\"");

  // Add REAPER state snapshot
  json.Append(",\"state\":");
  char *state_json = MagdaState::GetStateSnapshot();
  if (state_json) {
    json.Append(state_json);
    free(state_json);
  } else {
    json.Append("{}");
  }

  json.Append("}");

  // Allocate and return
  int len = json.GetLength();
  char *result = (char *)malloc(len + 1);
  if (result) {
    memcpy(result, json.Get(), len);
    result[len] = '\0';
  }
  return result;
}

char *MagdaHTTPClient::ExtractActionsJSON(const char *json_str, int json_len) {
  if (!json_str || json_len <= 0) {
    return nullptr;
  }

  // Find "actions" key in JSON
  const char *actions_key = "\"actions\"";
  int actions_key_len = (int)strlen(actions_key);
  const char *p = json_str;
  const char *end = json_str + json_len;

  // Search for "actions" key
  while (p < end - actions_key_len) {
    if (strncmp(p, actions_key, actions_key_len) == 0) {
      p += actions_key_len;
      // Skip whitespace
      while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
      }
      // Find colon
      if (p < end && *p == ':') {
        p++;
        // Skip whitespace after colon
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
          p++;
        }
        // Now p points to the start of the actions value
        const char *value_start = p;

        // Find the end of the JSON value (array or object)
        // We need to match brackets/braces to find the end
        if (p < end && (*p == '[' || *p == '{')) {
          char open_char = *p;
          char close_char = (open_char == '[') ? ']' : '}';
          int depth = 1;
          p++;

          while (p < end && depth > 0) {
            if (*p == '\\' && p + 1 < end) {
              // Skip escaped character
              p += 2;
              continue;
            }
            if (*p == '"') {
              // Skip string
              p++;
              while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) {
                  p += 2;
                } else {
                  p++;
                }
              }
              if (p < end)
                p++;
              continue;
            }
            if (*p == open_char) {
              depth++;
            } else if (*p == close_char) {
              depth--;
            }
            if (depth > 0) {
              p++;
            }
          }

          if (depth == 0 && p > value_start) {
            // Extract the actions JSON
            int actions_len = (int)(p - value_start);
            char *result = (char *)malloc(actions_len + 1);
            if (result) {
              memcpy(result, value_start, actions_len);
              result[actions_len] = '\0';
              return result;
            }
          }
        }
      }
      break;
    }
    p++;
  }

  return nullptr;
}

bool MagdaHTTPClient::SendQuestion(const char *question, WDL_FastString &response_json,
                                   WDL_FastString &error_msg) {
  if (!question || !question[0]) {
    error_msg.Set("Empty question");
    return false;
  }

  // Build request JSON
  char *request_json = BuildRequestJSON(question);
  if (!request_json) {
    error_msg.Set("Failed to build request JSON");
    return false;
  }

  int request_json_len = (int)strlen(request_json);

  // Build URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/api/v1/magda/chat"); // Backend endpoint

  // Create HTTP client
  if (m_http_get) {
    delete m_http_get;
  }
  m_http_get = new JNL_HTTPGet((JNL_IAsyncDNS *)m_dns);

  // Set headers
  char content_length_header[128];
  snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %d\r\n",
           request_json_len);
  m_http_get->addheader("Content-Type: application/json");
  m_http_get->addheader(content_length_header);

  // Add JWT token if set
  if (m_jwt_token.GetLength() > 0) {
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_jwt_token.Get());
    m_http_get->addheader(auth_header);
  }

  // Connect with POST method
  m_http_get->connect(url.Get(), 1, "POST");

  // Get connection and send POST body
  JNL_IConnection *con = m_http_get->get_con();
  if (!con) {
    free(request_json);
    error_msg.Set("Failed to get connection");
    return false;
  }

  // Run connection until connected
  while (con->get_state() == JNL_Connection::STATE_CONNECTING ||
         con->get_state() == JNL_Connection::STATE_RESOLVING) {
    con->run();
    SLEEP_MS(10); // Small delay
  }

  if (con->get_state() != JNL_Connection::STATE_CONNECTED) {
    free(request_json);
    error_msg.Set("Failed to connect");
    return false;
  }

  // Send POST body
  int sent = 0;
  while (sent < request_json_len) {
    con->run();
    int available = con->send_bytes_available();
    if (available > 0) {
      int to_send = request_json_len - sent;
      if (to_send > available)
        to_send = available;
      int result = con->send(request_json + sent, to_send);
      if (result < 0) {
        free(request_json);
        error_msg.Set("Failed to send POST body");
        return false;
      }
      sent += result;
    } else {
      SLEEP_MS(10);
    }
  }

  free(request_json);

  // Run HTTP client to receive response
  while (m_http_get->get_status() < 2) {
    int result = m_http_get->run();
    if (result < 0) {
      error_msg.Set(m_http_get->geterrorstr() ? m_http_get->geterrorstr() : "HTTP request failed");
      return false;
    }
    if (result == 1) {
      // Connection closed
      break;
    }
    SLEEP_MS(10);
  }

  // Check response code
  int reply_code = m_http_get->getreplycode();
  if (reply_code != 200) {
    char buf[256];
    snprintf(buf, sizeof(buf), "HTTP error %d", reply_code);
    error_msg.Set(buf);
    return false;
  }

  // Read response body
  char buffer[4096];
  int total_read = 0;
  while (m_http_get->get_status() == 2) {
    int available = m_http_get->bytes_available();
    if (available > 0) {
      int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
      int read = m_http_get->get_bytes(buffer, to_read);
      if (read > 0) {
        buffer[read] = '\0';
        response_json.Append(buffer, read);
        total_read += read;
      }
    } else {
      int result = m_http_get->run();
      if (result < 0) {
        break;
      }
      SLEEP_MS(10);
    }
  }

  // Read any remaining data
  while (m_http_get->bytes_available() > 0) {
    int available = m_http_get->bytes_available();
    int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
    int read = m_http_get->get_bytes(buffer, to_read);
    if (read > 0) {
      buffer[read] = '\0';
      response_json.Append(buffer, read);
    } else {
      break;
    }
  }

  // Execute actions from the response
  // The backend returns JSON with an "actions" field containing an array of actions
  if (response_json.GetLength() > 0) {
    WDL_FastString execution_result, execution_error;

    // Try to extract actions JSON from response
    char *actions_json = ExtractActionsJSON(response_json.Get(), (int)response_json.GetLength());

    if (actions_json) {
      // Execute the extracted actions
      if (!MagdaActions::ExecuteActions(actions_json, execution_result, execution_error)) {
        // Log error but don't fail the request
        // Could append execution_error to error_msg if needed
      }
      free(actions_json);
    } else {
      // No "actions" field found - maybe the response IS the actions array/object
      // Try executing the whole response
      wdl_json_parser parser;
      wdl_json_element *root = parser.parse(response_json.Get(), (int)response_json.GetLength());
      if (!parser.m_err && root && (root->is_array() || root->is_object())) {
        if (!MagdaActions::ExecuteActions(response_json.Get(), execution_result, execution_error)) {
          // Log error but don't fail the request
        }
      }
    }
  }

  return true;
}
