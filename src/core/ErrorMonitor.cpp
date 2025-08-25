/* @file ErrorMonitor.cpp
 * @brief stub for tests to work.
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

#include "core/ErrorMonitor.hpp"

namespace milo {
  namespace core {
    ErrorMonitor::ErrorMonitor() = default;
    ErrorMonitor::~ErrorMonitor() = default;
    void ErrorMonitor::notifyFailure(const std::string&) {}; // for now so linking works in tests
  } // namespace core
} // namespace milo

