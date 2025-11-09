#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace fs = std::filesystem;

[[maybe_unused]] std::string GetTimeNow();
bool CopyDirRecursive(const fs::path &src, const fs::path &dst);
std::optional<std::pair<double, double>> ParseGeo(const std::string &geo);
std::string GetField(const std::string &data, size_t field);
