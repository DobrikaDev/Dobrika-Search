#include "server/web_server.hpp"
#include "DServer.pb.h"
#include "static.hpp"
#include "xapian_processor/xapian_processor.hpp"
#include <atomic>
#include <chrono>
#include <drogon/drogon.h>
#include <utility>

using namespace drogon;

namespace {
std::shared_ptr<XapianLayer> g_layer;
std::atomic<bool> g_running{false};

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
        auto json = req->getJsonObject();
        if (!json) {
          Json::Value v;
          v["error"] = GetSearchStatus(DSearchStatus::DSInvalidJson);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
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
        } catch (...) {
          Json::Value v;
          v["ok"] = true;
          v["status"] = GetSearchStatus(DSearchStatus::DSIndexFall);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k500InternalServerError);
          callback(resp);
        }
      },
      {Post});

  // Search
  app().registerHandler(
      "/search",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto json = req->getJsonObject();
        if (!json) {
          Json::Value v;
          v["error"] = GetSearchStatus(DSearchStatus::DSInvalidJson);
          auto resp = HttpResponse::newHttpJsonResponse(v);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          return;
        }
        DSearchRequest sreq = MakeSearchFromJson(*json);
        DSearchResult sres = g_layer->DoSearch(sreq);
        auto resp = HttpResponse::newHttpJsonResponse(ToJson(sres));
        resp->setStatusCode(k200OK);
        callback(resp);
      },
      {Post});

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
