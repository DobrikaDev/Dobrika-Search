#include "tools/dse_tools.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string GetTimeNow() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
  return ss.str();
}

bool CopyDirRecursive(const fs::path &src, const fs::path &dst) {
  std::error_code ec;
  if (!fs::exists(src, ec))
    return false;
  // Ensure parent for destination exists
  fs::create_directories(dst.parent_path(), ec);
  // Replace the destination entirely to avoid stale files
  ec.clear();
  fs::remove_all(dst, ec);
  ec.clear();
  fs::copy(src, dst,
           fs::copy_options::recursive | fs::copy_options::copy_symlinks |
               fs::copy_options::overwrite_existing,
           ec);
  return !ec;
}

std::optional<std::pair<double, double>> ParseGeo(const std::string &geo) {
  if (geo.empty())
    return std::nullopt;
  auto comma = geo.find(',');
  if (comma == std::string::npos)
    return std::nullopt;
  try {
    double lat = std::stod(geo.substr(0, comma));
    double lon = std::stod(geo.substr(comma + 1));
    return std::make_pair(lat, lon);
  } catch (...) {
    return std::nullopt;
  }
}

std::string GetField(const std::string &data, size_t field) {
  size_t start = 0;
  while (true) {
    size_t end = data.find('\n', start);
    if (field == 0) {
      return std::string(data, start,
                         end == std::string::npos ? data.size() - start
                                                  : end - start);
    }
    if (end == std::string::npos) {
      // Requested field index is out of range; return empty.
      return std::string();
    }
    start = end;
    ++start;
    --field;
  }
}