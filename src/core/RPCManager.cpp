/* @file RPCManager.cpp
 * @brief manages asynchronous non-blocking coms with MCU's via serial over usb
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

// STL headers
#include <cassert>
#include <string>

// MiLO headers
#include "core/RPCManager.hpp"

using namespace milo::core;

RPCManager::RPCManager(std::shared_ptr<ErrorMonitor> errorMonitor)
    : errorMonitor_(std::move(errorMonitor)) {
  assert(errorMonitor_ && "[RPCManager] error monitor is nullptr");
}

void RPCManager::connect() {
  if (connected_)
    return;

  channels_.clear();

  for (auto& [dev, path] : symlinks_) {
    io::SerialChannel ch;
    if (!ch.open(path, kDefaultBaud)) {
      std::string errMsg =
          std::string("[RPCManager] serial device: ") + toString(dev) + " open failed";
      errorMonitor_->notifyFailure(errMsg);
      throw std::runtime_error(errMsg);
    }
    channels_.emplace(dev, std::move(ch));
  }
  //TODO: Logging hook
  connected_ = true;
}

