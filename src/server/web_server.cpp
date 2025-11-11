#include "server/web_server.hpp"
#include "DServer.pb.h"
#include "static.hpp"
#include "xapian_processor/xapian_processor.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <drogon/drogon.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace drogon;

namespace {
std::shared_ptr<XapianLayer> g_layer;
std::atomic<bool> g_running{false};
// Prometheus-style counters (cumulative; Prometheus will compute RPS via rate())
std::atomic<uint64_t> g_search_requests_total{0};
std::atomic<uint64_t> g_index_requests_total{0};
std::atomic<bool> g_log_requests{false};

bool EnvFlagEnabled(const char *name) {
  const char *val = std::getenv(name);
  if (!val)
    return false;
  std::string lower(val);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

std::string TruncateForLog(std::string_view body) {
  constexpr std::size_t kMaxBodyLog = 512;
  if (body.size() <= kMaxBodyLog) {
    return std::string(body);
  }
  std::string truncated(body.substr(0, kMaxBodyLog));
  truncated += "...[truncated]";
  return truncated;
}

DSIndexTask MakeTaskFromJson(const Json::Value &json) {
  DSIndexTask task;
  if (json.isMember("task_name"))
    task.set_task_name(json["task_name"].asString());
  if (json.isMember("task_desc"))
    task.set_task_desc(json["task_desc"].asString());
  if (json.isMember("geo_data"))
    task.set_geo_data(json["geo_data"].asString());
  if (json.isMember("task_id"))
    task.set_task_id(json["task_id"].asString());
  if (json.isMember("task_type"))
    task.set_task_type(json["task_type"].asString());
  if (json.isMember("task_tags") && json["task_tags"].isArray()) {
    for (const auto &t : json["task_tags"]) {
      task.add_task_tags(t.asString());
    }
  }
  return task;
}

DSearchRequest MakeSearchFromJson(const Json::Value &json) {
  DSearchRequest req;
  if (json.isMember("user_query"))
    req.set_user_query(json["user_query"].asString());
  if (json.isMember("geo_data"))
    req.set_geo_data(json["geo_data"].asString());
  if (json.isMember("query_type"))
    req.set_query_type(json["query_type"].asString());
  if (json.isMember("user_tags") && json["user_tags"].isArray()) {
    for (const auto &t : json["user_tags"]) {
      req.add_user_tags(t.asString());
    }
  }
  return req;
}

Json::Value ToJson(const DSearchResult &res) {
  Json::Value j;
  j["status"] = res.status();
  Json::Value ids(Json::arrayValue);
  for (int i = 0; i < res.task_id_size(); ++i) {
    ids.append(res.task_id(i));
  }
  j["task_id"] = std::move(ids);
  return j;
}
} // namespace

void start_server_blocking(const DobrikaServerConfig &cfg,
                           const std::string &address, uint16_t port) {
  g_layer = std::make_shared<XapianLayer>(cfg);
  g_log_requests.store(EnvFlagEnabled("DOBRIKA_LOG_REQUESTS"),
                       std::memory_order_relaxed);

  // Basic HTTP metrics endpoint (Prometheus exposition format)
  app().registerHandler(
      "/metrics",
      [](const HttpRequestPtr &,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        std::string body;
        body.reserve(256);
        body += "# HELP dobrika_search_requests_total Total search requests\n";
        body += "# TYPE dobrika_search_requests_total counter\n";
        body += "dobrika_search_requests_total ";
        body += std::to_string(g_search_requests_total.load());
        body += "\n";
        body += "# HELP dobrika_index_requests_total Total index requests\n";
        body += "# TYPE dobrika_index_requests_total counter\n";
        body += "dobrika_index_requests_total ";
        body += std::to_string(g_index_requests_total.load());
        body += "\n";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        resp->setBody(std::move(body));
        callback(resp);
      },
      {Get});

  app().registerHandler(
      "/healthz",
      [](const HttpRequestPtr &,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        resp->setBody(GetSearchStatus(DSearchStatus::DSHealthOk));
        callback(resp);
      },
      {Get});

  app().registerHandler(
      "/index",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto t0 = std::chrono::steady_clock::now();
        auto json = req->getJsonObject();
        if (!json) {
          Json::Value v;
          v["error"] = GetSearchStatus(DSearchStatus::DSInvalidJson);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          auto t1 = std::chrono::steady_clock::now();
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          std::string peer = req->peerAddr().toIpPort();
          std::string ua = req->getHeader("user-agent");
          size_t req_size = req->getBody().size();
          LOG_INFO << peer << " \"POST /index\" 400 "
                   << ms << "ms req_bytes=" << req_size
                   << " ua=\"" << ua << "\"";
          return;
        }
        DSIndexTask task = MakeTaskFromJson(*json);
        try {
          g_layer->AddTaskToDB(task);
          Json::Value v;
          v["ok"] = true;
          v["status"] = GetSearchStatus(DSearchStatus::DSIndexOk);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k200OK);
          callback(resp);
          auto t1 = std::chrono::steady_clock::now();
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          std::string peer = req->peerAddr().toIpPort();
          std::string ua = req->getHeader("user-agent");
          size_t req_size = req->getBody().size();
          LOG_INFO << peer << " \"POST /index\" 200 "
                   << ms << "ms req_bytes=" << req_size
                   << " task_id=\"" << task.task_id() << "\""
                   << " ua=\"" << ua << "\"";
        } catch (...) {
          Json::Value v;
          v["ok"] = true;
          v["status"] = GetSearchStatus(DSearchStatus::DSIndexFall);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k500InternalServerError);
          callback(resp);
          auto t1 = std::chrono::steady_clock::now();
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          std::string peer = req->peerAddr().toIpPort();
          std::string ua = req->getHeader("user-agent");
          size_t req_size = req->getBody().size();
          LOG_INFO << peer << " \"POST /index\" 500 "
                   << ms << "ms req_bytes=" << req_size
                   << " task_id=\"" << task.task_id() << "\""
                   << " ua=\"" << ua << "\"";
        }
      },
      {Post});

  // Search
  app().registerHandler(
      "/search",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto t0 = std::chrono::steady_clock::now();
        auto json = req->getJsonObject();
        if (!json) {
          Json::Value v;
          v["error"] = GetSearchStatus(DSearchStatus::DSInvalidJson);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          auto t1 = std::chrono::steady_clock::now();
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          std::string peer = req->peerAddr().toIpPort();
          std::string ua = req->getHeader("user-agent");
          size_t req_size = req->getBody().size();
          LOG_INFO << peer << " \"POST /search\" 400 "
                   << ms << "ms req_bytes=" << req_size
                   << " ua=\"" << ua << "\"";
          return;
        }
        DSearchRequest sreq = MakeSearchFromJson(*json);
        DSearchResult sres = g_layer->DoSearch(sreq);
        auto resp = HttpResponse::newHttpJsonResponse(ToJson(sres));
        resp->setStatusCode(k200OK);
        callback(resp);
        auto t1 = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::string peer = req->peerAddr().toIpPort();
        std::string ua = req->getHeader("user-agent");
        size_t req_size = req->getBody().size();
        LOG_INFO << peer << " \"POST /search\" 200 "
                 << ms << "ms req_bytes=" << req_size
                 << " results=" << sres.task_id_size()
                 << " ua=\"" << ua << "\"";
      },
      {Post});

  // Request logging + metrics increment after handlers produce a response
  app().registerPostHandlingAdvice([](const HttpRequestPtr &req,
                                      const HttpResponsePtr &resp) {
    const std::string path = req->path();
    if (path == "/search") {
      g_search_requests_total.fetch_add(1, std::memory_order_relaxed);
    } else if (path == "/index") {
      g_index_requests_total.fetch_add(1, std::memory_order_relaxed);
    }
    // Basic access log for other endpoints only (avoid duplicate logs for /search and /index)
    if (path != "/search" && path != "/index") {
      int code = static_cast<int>(resp->statusCode());
      std::string peer = req->peerAddr().toIpPort();
      LOG_INFO << peer << " \"" << req->methodString() << " " << path << "\" " << code;
    }
    if (g_log_requests.load(std::memory_order_relaxed)) {
      const auto peer = req->peerAddr().toIpPort();
      const auto method = req->methodString();
      const auto status = static_cast<int>(resp->statusCode());
      const auto &body = req->getBody();
      const std::string ua = req->getHeader("user-agent");
      std::ostringstream oss;
      oss << "[REQ] " << peer << " \"" << method << " " << path << "\" "
          << status << " req_bytes=" << body.size()
          << " ua=\"" << ua << "\" body=\"" << TruncateForLog(body) << "\"";
      const auto line = oss.str();
      LOG_INFO << line;
      std::cout << line << std::endl;
    }
  });

  app().addListener(address, port);
  g_running.store(true);
  app().run();
  g_running.store(false);
}

std::thread start_server_background(const DobrikaServerConfig &cfg,
                                    const std::string &address, uint16_t port) {
  return std::thread(
      [cfg, address, port]() { start_server_blocking(cfg, address, port); });
}

void stop_server() {
  if (g_running.load()) {
    app().quit();
  }
}
