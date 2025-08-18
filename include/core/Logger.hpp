#pragma once
/** @file  Logger.hpp
 *  @brief Asynchronous CSV logger (runs its own worker thread).
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <atomic>
#include <fstream>
#include <memory>
#include <thread>

namespace milo {
  namespace core {

    struct LogEvent;                        // defined later in src
    template <typename T> class RingBuffer; // forward decl to avoid heavy include

    class Logger {

    public:
      Logger() = default;
      ~Logger() = default;

      // --- public API ---
      void startNewRun();              ///< open file + launch worker thread
      void log(const LogEvent& event); ///< enqueue event (non-blocking)
      void finishRun();                ///< flush + join worker thread

    private:
      std::ofstream csvFile_;
      std::unique_ptr<RingBuffer<LogEvent>> buffer_;
      std::thread worker_;
      std::atomic<bool> running_{ false };
    };

  } // namespace core
} // namespace milo
