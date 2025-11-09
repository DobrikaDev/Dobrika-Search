#pragma once
#include <string>

#include "DServer.pb.h"

DobrikaServerConfig MakeServerConfig(const std::string &db_path,
                                     int cold_backup_timer_min,
                                     int hot_backup_timer_min,
                                     int search_offset, int search_limit,
                                     int search_geo_index);