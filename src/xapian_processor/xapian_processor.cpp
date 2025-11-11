#include "xapian_processor.hpp"

#include "static.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
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
    const std::string task_id = GetField(data, 2); // task_id is at index 2
    if (!task_id.empty()) {
      result.add_task_id(task_id);
    }
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
  case DSQueryTypeEnum::STagTasks:
    return DoTagSearch(user_request);
  case DSQueryTypeEnum::SUnknown:
    // Fallback: if user provided a textual query, perform text search
    if (!user_request.user_query().empty()) {
      return DoTextSearch(user_request);
    }
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }
  result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
  return result;
}

DSearchResult XapianLayer::DoTextSearch(const DSearchRequest &user_request) {
  DSearchResult result;
  if (user_request.user_query().empty()) {
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }

  try {
    Xapian::Enquire enq(database);
    // Ensure BM25 is used explicitly
    enq.set_weighting_scheme(Xapian::BM25Weight());

    Xapian::QueryParser qp;
    qp.set_stemmer(Xapian::Stem("russian"));
    qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);
    qp.set_database(database);

    Xapian::Query query =
        qp.parse_query(user_request.user_query(), Xapian::QueryParser::FLAG_DEFAULT);
    enq.set_query(query);

    Xapian::MSet mset = enq.get_mset(SearchConfigProto.search_offset(),
                                     SearchConfigProto.search_limit());

    for (Xapian::MSetIterator mit = mset.begin(); mit != mset.end(); ++mit) {
      const std::string &data = mit.get_document().get_data();
      const std::string task_id = GetField(data, 2); // task_id is at index 2
      if (!task_id.empty()) {
        result.add_task_id(task_id);
      }
    }

    result.set_status(GetSearchStatus(DSearchStatus::DSOk));
    return result;
  } catch (...) {
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }
}

DSearchResult XapianLayer::DoTagSearch(const DSearchRequest &user_request) {
  DSearchResult result;

  if (user_request.user_tags().empty()) {
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }

  try {
    Xapian::Enquire enq(database);

    std::vector<Xapian::Query> queries;

    for (const auto &tag : user_request.user_tags()) {
      if (!tag.empty()) {
        queries.push_back(Xapian::Query("TAG" + tag));
      }
    }

    if (queries.empty()) {
      result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
      return result;
    }

    Xapian::Query combined_query(Xapian::Query::OP_OR, queries.begin(),
                                 queries.end());
    enq.set_query(combined_query);

    Xapian::MSet mset = enq.get_mset(SearchConfigProto.search_offset(),
                                     SearchConfigProto.search_limit());

    // Use a set to deduplicate results
    std::set<std::string> seen_task_ids;
    for (Xapian::MSetIterator mit = mset.begin(); mit != mset.end(); ++mit) {
      const std::string &data = mit.get_document().get_data();
      const std::string task_id = GetField(data, 2); // task_id is at index 2
      if (!task_id.empty() && seen_task_ids.find(task_id) == seen_task_ids.end()) {
        result.add_task_id(task_id);
        seen_task_ids.insert(task_id);
      }
    }

    result.set_status(GetSearchStatus(DSearchStatus::DSOk));
    return result;
  } catch (...) {
    result.set_status(GetSearchStatus(DSearchStatus::DSUnknownTaskType));
    return result;
  }
}

void XapianLayer::AddTaskToDB(const DSIndexTask &task) {
  std::unique_lock<std::shared_mutex> lock(db_mutex);
  Xapian::WritableDatabase wdb(SearchConfigProto.db_file_name(),
                               Xapian::DB_CREATE_OR_OPEN);

  Xapian::Document doc;
  {
    std::ostringstream data;
    // data fields are newline-separated. Index 2 should be task_id for
    // retrieval. Index 3 onwards are task tags.
    data << task.task_name() << '\n'
         << task.task_desc() << '\n'
         << task.task_id();
    for (const auto &tag : task.task_tags()) {
      data << '\n' << tag;
    }
    doc.set_data(data.str());
  }

  Xapian::TermGenerator termgen;
  termgen.set_document(doc);
  termgen.set_stemmer(Xapian::Stem("russian"));

  if (!task.task_name().empty()) {
    termgen.index_text(task.task_name(), 1, "T");
    // Unprefixed indexing to enable default text search across fields
    termgen.index_text(task.task_name());
  }

  if (!task.task_desc().empty()) {
    // Index description as well (both prefixed and unprefixed)
    termgen.index_text(task.task_desc(), 1, "T");
    termgen.index_text(task.task_desc());
  }

  for (const auto &tag : task.task_tags()) {
    if (!tag.empty()) {
      doc.add_term("TAG" + tag);
    }
  }

  Xapian::LatLongCoords coords;
  if (OptionalGeoData geo = ParseGeo(task.geo_data())) {
    coords.append(Xapian::LatLongCoord(geo->first, geo->second));
  } else {
    coords.append(Xapian::LatLongCoord(55.45, 37.65));
  }
  doc.add_value(SearchConfigProto.search_geo_index(), coords.serialise());

  // Use replace_document with task_id as unique identifier to avoid duplicates
  wdb.replace_document("ID" + task.task_id(), doc);
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
