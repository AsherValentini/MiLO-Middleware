#pragma once
/** @file  ErrorMonitor.hpp
 *  @brief Central fault aggregator & escalation helper.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace milo::core {

  /**
 * @class ErrorMonitor
 * @brief Other threads call `notifyFailure()`; we call the registered
 *        escalation callback exactly once per unique error.
 *
 * * Thread-safe (mutex-protected vector).
 * * Debounces duplicate failures so SystemCoordinator doesn’t get spammed.
 */
  class ErrorMonitor {
  public:
    ErrorMonitor() = default;
    ~ErrorMonitor() = default;

    /// Register a lambda that escalates a fatal fault to SystemCoordinator.
    void registerEscalation(std::function<void(const std::string&)> cb);

    /// Called by subsystems on fault; will forward to the escalation callback.
    void notifyFailure(const std::string& message);

  private:
    void forwardIfNew(const std::string& message);

    std::function<void(const std::string&)> escalation_{};
    std::vector<std::string> seen_; ///< de-dupe list
    mutable std::mutex mtx_;
  };

} // namespace milo::core
