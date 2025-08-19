#pragma once
/** @file  FileLogger.hpp
 *  @brief Buffered CSV writer for SD-card or host FS.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <string>
#include <vector>

namespace milo {
  namespace io {

    /**
 * @class FileLogger
 * @brief RAII wrapper that opens a file, buffers writes, and flushes on demand.
 *
 *  * Intended for large run logs (10 kB – 1 MB).
 *  * Uses `std::fwrite` in 4 kB chunks (impl TBD).
 */
    class FileLogger {
    public:
      FileLogger() = default;
      ~FileLogger(); ///< flush + fclose

      //---public API------------------------------------------------------
      /** @returns false if path cannot be opened writable. */
      bool open(const std::string& path);

      /** Queues one CSV line (caller includes trailing '\n'). */
      void write(const std::string& csv);

      /** Force-flush buffer to disk; returns true on success. */
      bool flush();

      void close();

      //---non-copyable, move-enabled---------------------------------------
      FileLogger(const FileLogger&) = delete;
      FileLogger& operator=(const FileLogger&) = delete;
      FileLogger(FileLogger&&) = default;
      FileLogger& operator=(FileLogger&&) = default;

    private:
      FILE* fp_{ nullptr };
      std::vector<char> buffer_;
    };

  } // namespace io
} // namespace milo
