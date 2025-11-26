// Simple test program to test login HTTP call in isolation
// Compile with: g++ -o test_login test_login.cpp -I./WDL/WDL -I./reaper-sdk/sdk -L./WDL/WDL -ljnetlib -lwdlstring -ljsonparse

#include <iostream>
#include <cstring>
#include <cstdlib>

// Minimal includes for testing
#include "../WDL/WDL/jnetlib/httpget.h"
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/jnetlib/util.h"
#include "../WDL/WDL/wdlstring.h"
#include "../WDL/WDL/jsonparse.h"

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0] << " <email> <password>" << std::endl;
    return 1;
  }

  const char *email = argv[1];
  const char *password = argv[2];
  const char *backend_url = "http://localhost:8080";

  std::cout << "Testing login to " << backend_url << std::endl;
  std::cout << "Email: " << email << std::endl;

  // Initialize socket library
  JNL::open_socketlib();

  // Build JSON request
  WDL_FastString json;
  json.Append("{\"email\":\"");
  for (const char *p = email; *p; p++) {
    if (*p == '"' || *p == '\\') {
      json.Append("\\");
    }
    json.AppendFormatted(1, "%c", *p);
  }
  json.Append("\",\"password\":\"");
  for (const char *p = password; *p; p++) {
    if (*p == '"' || *p == '\\') {
      json.Append("\\");
    }
    json.AppendFormatted(1, "%c", *p);
  }
  json.Append("\"}");

  std::cout << "Request JSON: " << json.Get() << std::endl;

  // Create HTTP client
  JNL_AsyncDNS *dns = new JNL_AsyncDNS;
  JNL_HTTPGet *http_get = new JNL_HTTPGet(dns);

  WDL_FastString url;
  url.Set(backend_url);
  url.Append("/api/auth/login");

  char content_length[128];
  snprintf(content_length, sizeof(content_length), "Content-Length: %d", json.GetLength());
  http_get->addheader("Content-Type: application/json");
  http_get->addheader(content_length);

  std::cout << "Connecting to " << url.Get() << "..." << std::endl;
  // connect() with POST mode (1 = POST, 0 = GET)
  http_get->connect(url.Get(), 1, "POST");

  JNL_IConnection *con = http_get->get_con();
  if (!con) {
    std::cout << "ERROR: Failed to get connection" << std::endl;
    delete http_get;
    delete dns;
    return 1;
  }

  // Wait for connection with timeout
  int connection_timeout_ms = 5000;
  int elapsed_ms = 0;
  std::cout << "Waiting for connection (state=" << con->get_state() << ")..." << std::endl;

  while (con->get_state() == JNL_Connection::STATE_CONNECTING ||
         con->get_state() == JNL_Connection::STATE_RESOLVING) {
    con->run();
    SLEEP_MS(10);
    elapsed_ms += 10;
    if (elapsed_ms % 100 == 0) {
      std::cout << "  Still connecting... (" << elapsed_ms << "ms)" << std::endl;
    }
    if (elapsed_ms >= connection_timeout_ms) {
      std::cout << "ERROR: Connection timeout after " << elapsed_ms << "ms" << std::endl;
      delete http_get;
      delete dns;
      return 1;
    }
  }

  std::cout << "Connection state: " << con->get_state() << std::endl;

  if (con->get_state() != JNL_Connection::STATE_CONNECTED) {
    const char *error_str = http_get->geterrorstr();
    std::cout << "ERROR: Connection failed (state=" << con->get_state() << ")";
    if (error_str && error_str[0]) {
      std::cout << ": " << error_str;
    }
    std::cout << std::endl;
    delete http_get;
    delete dns;
    return 1;
  }

  std::cout << "Connected! Waiting for headers to be sent..." << std::endl;

  // Wait for HTTP headers to be sent (JNL_HTTPGet sends them via send_string in connect())
  // We need to run the connection a few times to let it send the headers
  int header_wait_ms = 0;
  while (header_wait_ms < 1000) {
    con->run();
    SLEEP_MS(10);
    header_wait_ms += 10;

    // Check if we can send now (send_bytes_available > 0 means buffer has space)
    int available = con->send_bytes_available();
    if (available > 0) {
      std::cout << "  Headers sent, ready to send POST body (available=" << available << ")" << std::endl;
      break;
    }
  }

  if (con->send_bytes_available() <= 0) {
    std::cout << "WARNING: Still no send buffer available, trying anyway..." << std::endl;
  }

  // Send POST body
  int sent = 0;
  int json_len = json.GetLength();
  int send_timeout_ms = 5000;
  int send_elapsed_ms = 0;

  std::cout << "Sending POST body (" << json_len << " bytes)..." << std::endl;

  while (sent < json_len) {
    con->run();

    // Check connection state
    int state = con->get_state();
    if (state != JNL_Connection::STATE_CONNECTED) {
      std::cout << "ERROR: Connection lost during send (state=" << state << ")" << std::endl;
      delete http_get;
      delete dns;
      return 1;
    }

    // Try to send - use send_string for the remaining data
    if (sent == 0) {
      // First time, send the whole JSON string
      con->send_string(json.Get());
      sent = json_len;
      std::cout << "  Sent entire JSON via send_string (" << sent << " bytes)" << std::endl;
      break;
    } else {
      // Shouldn't get here if send_string worked
      std::cout << "ERROR: Unexpected state - sent=" << sent << " but should be 0" << std::endl;
      delete http_get;
      delete dns;
      return 1;
    }
  }

  std::cout << "Request sent. Waiting for response..." << std::endl;

  // Receive response
  int response_timeout_ms = 10000;
  int response_elapsed_ms = 0;
  while (http_get->get_status() < 2) {
    int result = http_get->run();
    if (result < 0) {
      std::cout << "ERROR: HTTP request failed" << std::endl;
      delete http_get;
      delete dns;
      return 1;
    }
    if (result == 1) {
      break;
    }
    SLEEP_MS(10);
    response_elapsed_ms += 10;
    if (response_elapsed_ms % 500 == 0) {
      std::cout << "  Still waiting for response... (" << response_elapsed_ms << "ms)" << std::endl;
    }
    if (response_elapsed_ms >= response_timeout_ms) {
      std::cout << "ERROR: Response timeout after " << response_elapsed_ms << "ms" << std::endl;
      delete http_get;
      delete dns;
      return 1;
    }
  }

  int reply_code = http_get->getreplycode();
  std::cout << "Response code: " << reply_code << std::endl;

  if (reply_code != 200) {
    std::cout << "ERROR: Login failed (HTTP " << reply_code << ")" << std::endl;
    delete http_get;
    delete dns;
    return 1;
  }

  // Read response
  WDL_FastString response;
  char buffer[4096];
  int read_timeout_ms = 5000;
  int read_elapsed_ms = 0;
  while (http_get->get_status() == 2) {
    int available = http_get->bytes_available();
    if (available > 0) {
      int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
      int read = http_get->get_bytes(buffer, to_read);
      if (read > 0) {
        buffer[read] = '\0';
        response.Append(buffer, read);
        read_elapsed_ms = 0;
        std::cout << "  Received " << response.GetLength() << " bytes" << std::endl;
      }
    } else {
      int result = http_get->run();
      if (result < 0) {
        break;
      }
      SLEEP_MS(10);
      read_elapsed_ms += 10;
      if (read_elapsed_ms >= read_timeout_ms) {
        std::cout << "ERROR: Read timeout" << std::endl;
        delete http_get;
        delete dns;
        return 1;
      }
    }
  }

  std::cout << "Response received (" << response.GetLength() << " bytes):" << std::endl;
  std::cout << response.Get() << std::endl;

  // Parse response
  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(response.Get(), (int)response.GetLength());
  if (parser.m_err || !root) {
    std::cout << "ERROR: Failed to parse JSON response" << std::endl;
    delete http_get;
    delete dns;
    return 1;
  }

  wdl_json_element *token_elem = root->get_item_by_name("access_token");
  if (!token_elem || !token_elem->m_value_string) {
    std::cout << "ERROR: No access_token in response" << std::endl;
    delete http_get;
    delete dns;
    return 1;
  }

  const char *token = token_elem->m_value;
  std::cout << "SUCCESS! Access token: " << token << std::endl;

  delete http_get;
  delete dns;
  return 0;
}
