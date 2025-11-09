#include <xapian.h>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "xapian_processor/xapian_processor.hpp"
#include "DSRequest.pb.h"
#include "DSResponse.pb.h"
#include "DServer.pb.h"
#include "tools/config_generator.hpp"

namespace fs = std::filesystem;

static void clean_dir(const fs::path &p) {
  std::error_code ec;
  if (fs::exists(p, ec))
    fs::remove_all(p, ec);
}

TEST_CASE("Cold and hot backups create snapshot directories", "[backup]") {
  clean_dir("db");
  clean_dir("backups-testing");
  DobrikaServerConfig cfg = MakeServerConfig("db", 30, 15, 0, 20, 2);
  XapianLayer layer(cfg);

  DSIndexTask task;
  task.set_task_name("Backup Check");
  task.set_task_desc("Doc for backup");
  task.set_geo_data("55.7,37.6");
  layer.AddTaskToDB(task);

  REQUIRE(layer.PerformColdBackup("backups-testing"));
  REQUIRE(layer.PerformHotBackup("backups-testing"));

  REQUIRE(fs::exists("backups-testing/cold"));
  REQUIRE(fs::exists("backups-testing/hot"));
  bool cold_nonempty = false, hot_nonempty = false;
  for (auto &e : fs::directory_iterator("backups-testing/cold")) {
    cold_nonempty = true;
    break;
  }
  for (auto &e : fs::directory_iterator("backups-testing/hot")) {
    hot_nonempty = true;
    break;
  }
  REQUIRE(cold_nonempty);
  REQUIRE(hot_nonempty);
}
