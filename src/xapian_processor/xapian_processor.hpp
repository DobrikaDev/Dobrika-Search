#pragma once
#include <xapian.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>

#include "DSRequest.pb.h"
#include "DSResponse.pb.h"
#include "DServer.pb.h"

#include "tools/dse_tools.hpp"

class XapianLayer {
public:
  XapianLayer() = delete;

public:
  XapianLayer(const DobrikaServerConfig &config);
  ~XapianLayer();

public:
  DSearchResult DoSearch(const DSearchRequest &user_query);
  DSearchResult DoGeoSearch(const DSearchRequest &user_query);
  DSearchResult DoTagSearch(const DSearchRequest &user_query);
  void AddTaskToDB(const DSIndexTask &task);

private:
  std::unique_ptr<Xapian::LatLongDistanceKeyMaker>
  SetupGeoQuery(const std::pair<double, double> &userPos);

public:
  bool PerformColdBackup(const std::string &backup_root);
  bool PerformHotBackup(const std::string &backup_root);
  void StartBackupScheduler(const std::string &backup_root);
  void StopBackupScheduler();

private:
  Xapian::Database database;
  mutable std::shared_mutex db_mutex;

  std::thread cold_thread;
  std::thread hot_thread;
  std::atomic<bool> stop_backups{false};
  std::string backup_root_path;
  std::mutex sched_mutex;
  std::condition_variable sched_cv;

  SearchConfig SearchConfigProto;
};