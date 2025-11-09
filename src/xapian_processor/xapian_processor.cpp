#include "xapian_processor.hpp"

#include "static.hpp"
#include <chrono>
#include <filesystem>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;

using OptionalGeoData = std::optional<std::pair<double, double>>;

XapianLayer::XapianLayer(const DobrikaServerConfig &config) {
  SearchConfigProto = config.sc();

  try {
    database = Xapian::Database(SearchConfigProto.db_file_name());
  } catch (...) {
    Xapian::WritableDatabase wdb(SearchConfigProto.db_file_name(),
                                 Xapian::DB_CREATE_OR_OPEN);
    wdb.commit();
    database = Xapian::Database(SearchConfigProto.db_file_name());
  }
}

XapianLayer::~XapianLayer() { StopBackupScheduler(); }

std::unique_ptr<Xapian::LatLongDistanceKeyMaker>
XapianLayer::SetupGeoQuery(const std::pair<double, double> &userPos) {
  Xapian::LatLongCoord centre(userPos.first, userPos.second);
  Xapian::GreatCircleMetric metric;
  return std::make_unique<Xapian::LatLongDistanceKeyMaker>(
      SearchConfigProto.search_geo_index(), centre, metric);
}

DSearchResult XapianLayer::DoGeoSearch(const DSearchRequest &user_query) {
  DSearchResult result;
  Xapian::Enquire enq(database);
  OptionalGeoData geo = ParseGeo(user_query.geo_data());
  if (!geo.has_value()) {
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }
  enq.set_query(Xapian::Query::MatchAll);

  auto keymaker = SetupGeoQuery(*geo);
  enq.set_sort_by_key(keymaker.get(), false);
  Xapian::MSet mset = enq.get_mset(SearchConfigProto.search_offset(),
                                   SearchConfigProto.search_limit());
  for (Xapian::MSetIterator mit = mset.begin(); mit != mset.end(); ++mit) {
    const std::string &data = mit.get_document().get_data();
    const auto tid = GetField(data, SearchConfigProto.search_geo_index());
    result.add_task_id(tid);
  }
  result.set_status(GetSearchStatus(DSearchStatus::DSOk));
  return result;
}

DSearchResult XapianLayer::DoSearch(const DSearchRequest &user_request) {
  const auto query_type = GetTaskType(user_request);
  DSearchResult result;
  switch (query_type) {
  case DSQueryTypeEnum::SOnlyOnlineTasks:
    result.set_status(GetSearchStatus((DSearchStatus::DSNotImplemented)));
    return result;
  case DSQueryTypeEnum::SGeoTasks:
    return DoGeoSearch(user_request);
  case DSQueryTypeEnum::SRandomTasks:
    result.set_status(GetSearchStatus((DSearchStatus::DSNotImplemented)));
    return result;
  case DSQueryTypeEnum::SUnknown:
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }
  // Fallback, should be unreachable
  result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
  return result;
}

void XapianLayer::AddTaskToDB(const DSIndexTask &task) {
  std::unique_lock<std::shared_mutex> lock(db_mutex);
  Xapian::WritableDatabase wdb(SearchConfigProto.db_file_name(),
                               Xapian::DB_CREATE_OR_OPEN);

  Xapian::Document doc;
  {
    std::ostringstream data;
    // data fields are newline-separated. Index 2 should be task_id for
    // retrieval.
    data << task.task_name() << '\n'
         << task.task_desc() << '\n'
         << task.task_id();
    doc.set_data(data.str());
  }

  Xapian::TermGenerator termgen;
  termgen.set_document(doc);
  termgen.set_stemmer(Xapian::Stem("russian"));

  if (!task.task_name().empty()) {
    termgen.index_text(task.task_name(), 1, "T");
  }
  Xapian::LatLongCoords coords;
  if (OptionalGeoData geo = ParseGeo(task.geo_data())) {
    coords.append(Xapian::LatLongCoord(geo->first, geo->second));
  } else {
    coords.append(Xapian::LatLongCoord(55.45, 37.65));
  }
  doc.add_value(SearchConfigProto.search_geo_index(), coords.serialise());

  wdb.add_document(doc);
  wdb.commit();
  database.reopen();
}

bool XapianLayer::PerformColdBackup(const std::string &backup_root) {
  std::unique_lock<std::shared_mutex> lock(db_mutex);
  const fs::path src{SearchConfigProto.db_file_name()};
  fs::path dst = fs::path(backup_root) / "cold";
  return CopyDirRecursive(src, dst);
}

bool XapianLayer::PerformHotBackup(const std::string &backup_root) {
  std::unique_lock<std::shared_mutex> lock(db_mutex);
  const fs::path src{SearchConfigProto.db_file_name()};
  fs::path dst = fs::path(backup_root) / "hot";
  return CopyDirRecursive(src, dst);
}

void XapianLayer::StartBackupScheduler(const std::string &backup_root) {
  // Make repeated calls safe: stop existing threads if any
  StopBackupScheduler();
  {
    std::lock_guard<std::mutex> lk(sched_mutex);
    backup_root_path = backup_root;
    stop_backups = false;
  }
  cold_thread = std::thread([this]() {
    std::unique_lock<std::mutex> lk(this->sched_mutex);
    while (!this->stop_backups.load()) {
      if (this->sched_cv.wait_for(
              lk,
              std::chrono::minutes(SearchConfigProto.cold_backup_timer_min()),
              [this] { return this->stop_backups.load(); })) {
        break;
      }
      auto root = this->backup_root_path;
      lk.unlock();
      (void)this->PerformColdBackup(root);
      lk.lock();
    }
  });
  hot_thread = std::thread([this]() {
    std::unique_lock<std::mutex> lk(this->sched_mutex);
    while (!this->stop_backups.load()) {
      if (this->sched_cv.wait_for(
              lk,
              std::chrono::minutes(SearchConfigProto.hot_backup_timer_min()),
              [this] { return this->stop_backups.load(); })) {
        break;
      }
      auto root = this->backup_root_path;
      lk.unlock();
      (void)this->PerformHotBackup(root);
      lk.lock();
    }
  });
}

void XapianLayer::StopBackupScheduler() {
  {
    std::lock_guard<std::mutex> lk(sched_mutex);
    stop_backups = true;
  }
  sched_cv.notify_all();
  if (cold_thread.joinable())
    cold_thread.join();
  if (hot_thread.joinable())
    hot_thread.join();
}
