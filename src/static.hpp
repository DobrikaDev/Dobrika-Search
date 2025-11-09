#pragma once
#include "DSRequest.pb.h"
#include <map>
#include <string>

enum DSQueryTypeEnum { SGeoTasks, SOnlyOnlineTasks, SRandomTasks, SUnknown };

inline const std::map<std::string, DSQueryTypeEnum> kQueryTypeByString = {
    {
        "QT_OnlineTasks",
        DSQueryTypeEnum::SOnlyOnlineTasks,
    },
    {"QT_GeoTasks", DSQueryTypeEnum::SGeoTasks},
    {"QT_RandomTasks", DSQueryTypeEnum::SRandomTasks}};

inline DSQueryTypeEnum GetTaskType(const DSearchRequest &request) {
  const auto it = kQueryTypeByString.find(request.query_type());
  return it == kQueryTypeByString.end() ? DSQueryTypeEnum::SUnknown
                                        : it->second;
};
/*------------------------------Task type enums-----------------------*/
enum DSTaskTypeEnum { TOnlineTask, TOfflineTask, TUnknown };

inline const std::map<std::string, DSTaskTypeEnum> kTaskTypeByString = {
    {"TT_OnlineTask", DSTaskTypeEnum::TOnlineTask},
    {"TT_OfflineTask", DSTaskTypeEnum::TOfflineTask},
};

inline DSTaskTypeEnum GetTaskFromRequest(const DSIndexTask &request) {
  const auto it = kTaskTypeByString.find(request.task_type());
  return it == kTaskTypeByString.end() ? DSTaskTypeEnum::TUnknown : it->second;
};

/*Search Status*/

enum DSearchStatus { DSOk, DSUnknownTaskType, DSNotImplemented };
inline const std::map<DSearchStatus, std::string> kStatusToString = {
    {DSearchStatus::DSOk, "SearchOk"},
    {DSearchStatus::DSUnknownTaskType, "SearchUnknownType"},
    {DSearchStatus::DSNotImplemented, "DSNotImplemented"}};

inline std::string GetSearchStatus(DSearchStatus status) {
  const auto it = kStatusToString.find(status);
  return it == kStatusToString.end() ? std::string{} : it->second;
}