#pragma once
/** @file  ConfigLoader.hpp
 *  @brief Loads run-time configuration (JSON) from SD-card or host FS.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <string>

namespace nlohmann {
  class json;
} // namespace nlohmann

namespace milo::core {

  /**
 * @class ConfigLoader
 * @brief Thin helper that reads a JSON file, validates a checksum, and hands
 *        the parsed object to the caller.
 *
 *  * No caching — every call to `load()` re-reads the file (cheap, tiny file).
 *  * All schema validation lives in the calling layer (SystemCoordinator).
 */
  class ConfigLoader {
  public:
    /// @param configPath  Absolute or relative path on SD/host FS.
    explicit ConfigLoader(std::string configPath);

    /// Parse the file into a nlohmann::json object or throw `std::runtime_error`.
    nlohmann::json load() const;

  private:
    std::string path_;
  };

} // namespace milo::core
