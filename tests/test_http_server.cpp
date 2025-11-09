#include "server/web_server.hpp"
#include "tools/config_generator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <drogon/drogon.h>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static void clean_db(const std::string &db_path) {
  const fs::path db{db_path};
  std::error_code ec;
  if (fs::exists(db, ec)) {
    fs::remove_all(db, ec);
  }
}

TEST_CASE("HTTP index and geo search round-trip", "[http][server]") {
  const std::string db = "test_db";
  clean_db(db);
  const uint16_t port = 18080;
  const std::string addr = "127.0.0.1";
  DobrikaServerConfig cfg = MakeServerConfig(db, 30, 15, 0, 5, 2);

  auto th = start_server_background(cfg, addr, port);
  // Give the server a moment to start
  std::this_thread::sleep_for(200ms);

  auto client = drogon::HttpClient::newHttpClient("http://" + addr + ":" +
                                                  std::to_string(port));

  // Index a task
  {
    auto req = drogon::HttpRequest::newHttpJsonRequest(Json::Value{
        {"task_name", "HTTP milk"},
        {"task_desc", "desc"},
        {"geo_data", "55.0000,37.0000"},
        {"task_id", "http-1"},
        {"task_type", "TT_OfflineTask"},
    });
    req->setMethod(drogon::Post);
    req->setPath("/index");
    auto resp = client->sendRequest(req);
    REQUIRE(resp.first == drogon::ReqResult::Ok);
    REQUIRE(resp.second != nullptr);
    REQUIRE(resp.second->getStatusCode() == drogon::k200OK);
  }

  // Search near the same location
  {
    auto req = drogon::HttpRequest::newHttpJsonRequest(Json::Value{
        {"query_type", "QT_GeoTasks"},
        {"geo_data", "55.0000,37.0000"},
    });
    req->setMethod(drogon::Post);
    req->setPath("/search");
    auto resp = client->sendRequest(req);
    REQUIRE(resp.first == drogon::ReqResult::Ok);
    REQUIRE(resp.second != nullptr);
    REQUIRE(resp.second->getStatusCode() == drogon::k200OK);
    const auto *json = resp.second->getJsonObject();
    REQUIRE(json != nullptr);
    REQUIRE((*json)["status"].asString() == "SearchOk");
    REQUIRE((*json)["task_id"].isArray());
    REQUIRE((*json)["task_id"].size() >= 1);
    bool found = false;
    for (const auto &v : (*json)["task_id"]) {
      if (v.asString() == "http-1") {
        found = true;
        break;
      }
    }
    REQUIRE(found);
  }

  stop_server();
  th.join();
}
