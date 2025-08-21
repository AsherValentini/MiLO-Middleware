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
  //TODO: Logger hook
  connected_ = true;
}

void RPCManager::sendCommand(Device dev, const protocols::Command& cmd) {

  if (!connected_)
    throw std::runtime_error("[RPCManager] RPCmanager not connected");

  auto it = channels_.find(dev);
  if (it == channels_.end())
    throw std::invalid_argument("[RPCManager] send failed: unknown serial device");
  auto cmdStr = cmd.toWire();

  assert(cmdStr.size() <= 256 && "[RPCManager] command exceeds 256 byte threashold");

  if (!it->second.writeLine(cmdStr)) {
    std::string errMsg =
        "[RPCManager] failed to write to serial device: " + std::string(toString(it->first));
    errorMonitor_->notifyFailure(errMsg);
    throw std::runtime_error(errMsg);
  }

  //TODO: (Week 3) - recored an "in-flight" entry (device->timestamp) so that awaitResponse() can enforce round trip timeouts for now we skip this ashy
}

