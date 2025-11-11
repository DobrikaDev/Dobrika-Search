#include "server/web_server.hpp"
#include "tools/config_generator.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

// Environment variables for configuration:
//  - DOBRIKA_ADDR (default "127.0.0.1")
//  - DOBRIKA_PORT (default 8088)
//  - DOBRIKA_DB_PATH (default "db")
//  - DOBRIKA_COLD_MIN (default 30)
//  - DOBRIKA_HOT_MIN (default 15)
//  - DOBRIKA_SEARCH_OFFSET (default 0)
//  - DOBRIKA_SEARCH_LIMIT (default 20)
//  - DOBRIKA_GEO_INDEX (default 2)
static std::string envOr(const char *name, const std::string &def) {
  const char *v = std::getenv(name);
  return v ? std::string(v) : def;
}

static int envOrInt(const char *name, int def) {
  const char *v = std::getenv(name);
  if (!v)
    return def;
  try {
    return std::stoi(v);
  } catch (...) {
    return def;
  }
}

int main() {
  const std::string addr = envOr("DOBRIKA_ADDR", "127.0.0.1");
  const uint16_t port = static_cast<uint16_t>(envOrInt("DOBRIKA_PORT", 8088));
  const std::string db = envOr("DOBRIKA_DB_PATH", "db");
  const int cold = envOrInt("DOBRIKA_COLD_MIN", 30);
  const int hot = envOrInt("DOBRIKA_HOT_MIN", 15);
  const int off = envOrInt("DOBRIKA_SEARCH_OFFSET", 0);
  const int lim = envOrInt("DOBRIKA_SEARCH_LIMIT", 20);
  const int gidx = envOrInt("DOBRIKA_GEO_INDEX", 9);

  DobrikaServerConfig cfg = MakeServerConfig(db, cold, hot, off, lim, gidx);

  std::cerr << "Dobrika web server configuration:\n";
  std::cerr << cfg.DebugString() << std::endl;
  std::cerr << "Dobrika web server listening on " << addr << ":" << port
            << std::endl;
  start_server_blocking(cfg, addr, port);
  return 0;
}
