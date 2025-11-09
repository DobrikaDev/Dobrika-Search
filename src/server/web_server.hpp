#pragma once
#include <string>
#include <thread>

class DobrikaServerConfig;

// Starts a Drogon-based HTTP server exposing Dobrika search endpoints.
// Endpoints:
//  - GET  /healthz
//  - POST /index  {task_name, task_desc, geo_data, task_id, task_type}
//  - POST /search {user_query, geo_data, user_tags[], query_type}
//
// The server binds to the provided address and port and serves requests that
// are handled by XapianLayer with the supplied configuration.
void start_server_blocking(const DobrikaServerConfig &cfg,
                           const std::string &address, uint16_t port);

// Starts the server on a background thread. Returns the running thread.
// Call stop_server() to shut it down and then join the thread.
std::thread start_server_background(const DobrikaServerConfig &cfg,
                                    const std::string &address, uint16_t port);

// Requests the server to stop (non-blocking). Callers should join the thread
// returned by start_server_background afterwards.
void stop_server();
