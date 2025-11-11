#include <xapian.h>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <vector>

#include "DSRequest.pb.h"
#include "DSResponse.pb.h"
#include "DServer.pb.h"

#include "tools/config_generator.hpp"
#include "xapian_processor/xapian_processor.hpp"

namespace fs = std::filesystem;

static void clean_db(const std::string &db_path) {
  const fs::path db{db_path};
  std::error_code ec;
  if (fs::exists(db, ec)) {
    fs::remove_all(db, ec);
  }
}

TEST_CASE("Index single task and verify presence via Xapian",
          "[xapian][index]") {

  clean_db("test_db");
  DobrikaServerConfig cfg = MakeServerConfig("test_db", 30, 15, 0, 20, 9);
  XapianLayer layer(cfg);

  DSIndexTask task;
  task.set_task_name("Купить молоко");
  task.set_task_desc("Магазин у дома");
  task.set_geo_data("55.7558,37.6173");
  task.set_task_id("123");
  task.set_task_type("TT_OfflineTask");

  REQUIRE_NOTHROW(layer.AddTaskToDB(task));

  DSearchRequest request;
  request.set_user_query("Молоко");
  request.set_geo_data("55.7558,37.6173");
  request.set_query_type("QT_GeoTasks");
  const auto result = layer.DoSearch(request);
  REQUIRE(result.status() == "SearchOk");
}

TEST_CASE("Geo search returns the same task_id that was indexed",
          "[xapian][geo][task_id]") {
  clean_db("geo_id_db");
  // search_limit = 5, geo index points to 3rd field (task_id) in stored data
  DobrikaServerConfig cfg = MakeServerConfig("geo_id_db", 30, 15, 0, 5, 9);
  XapianLayer layer(cfg);

  DSIndexTask task;
  task.set_task_name("ID Check");
  task.set_task_desc("Doc with id");
  task.set_geo_data("55.0000,37.0000");
  task.set_task_id("task-42");
  task.set_task_type("TT_OfflineTask");
  layer.AddTaskToDB(task);

  DSearchRequest req;
  req.set_query_type("QT_GeoTasks");
  req.set_geo_data("55.0000,37.0000");
  auto res = layer.DoSearch(req);
  REQUIRE(res.status() == "SearchOk");
  REQUIRE(res.task_id_size() >= 1);
  REQUIRE(res.task_id(0) == "task-42");
}

TEST_CASE("Geo search returns nearest N task_ids (limited by search_limit)",
          "[xapian][geo][nearest]") {
  clean_db("geo_nearest_db");
  const int search_limit = 3;
  DobrikaServerConfig cfg =
      MakeServerConfig("geo_nearest_db", 30, 15, 0, search_limit, 9);
  XapianLayer layer(cfg);

  // Seed N tasks at increasing distance from (55.0000, 37.0000)
  const std::vector<std::pair<std::string, std::pair<double, double>>> tasks = {
      {"id0", {55.0000, 37.0000}}, {"id1", {55.0050, 37.0000}},
      {"id2", {55.0100, 37.0000}}, {"id3", {55.0200, 37.0000}},
      {"id4", {55.0300, 37.0000}},
  };
  for (const auto &t : tasks) {
    DSIndexTask item;
    item.set_task_name("Nearest");
    item.set_task_desc("Check");
    item.set_geo_data(std::to_string(t.second.first) + "," +
                      std::to_string(t.second.second));
    item.set_task_id(t.first);
    item.set_task_type("TT_OfflineTask");
    layer.AddTaskToDB(item);
  }

  // Query close to id0 so we expect ids: id0, id1, id2 (search_limit = 3)
  DSearchRequest req;
  req.set_query_type("QT_GeoTasks");
  req.set_geo_data("55.0000,37.0000");
  auto res = layer.DoSearch(req);
  REQUIRE(res.status() == "SearchOk");
  REQUIRE(res.task_id_size() == search_limit);
  REQUIRE(res.task_id(0) == "id0");
  REQUIRE(res.task_id(1) == "id1");
  REQUIRE(res.task_id(2) == "id2");
}
