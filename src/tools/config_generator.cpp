#include "config_generator.hpp"

DobrikaServerConfig MakeServerConfig(const std::string &db_path,
                                     int cold_backup_timer_min,
                                     int hot_backup_timer_min,
                                     int search_offset, int search_limit,
                                     int search_geo_index) {
  DobrikaServerConfig cfg;
  auto *sc = cfg.mutable_sc();
  sc->set_db_file_name(db_path);
  sc->set_cold_backup_timer_min(cold_backup_timer_min);
  sc->set_hot_backup_timer_min(hot_backup_timer_min);
  sc->set_search_offset(search_offset);
  sc->set_search_limit(search_limit);
  sc->set_search_geo_index(search_geo_index);
  return cfg;
}
