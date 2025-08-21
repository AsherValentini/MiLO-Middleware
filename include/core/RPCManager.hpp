#pragma once
/** @file  RPCManager.hpp
 *  @brief Non-blocking serial RPC multiplexer.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

// STL headers
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>

//MiLO headers
#include "core/ErrorMonitor.hpp" // RPCManager will be a client to the error monitor
#include "io/SerialChannel.hpp" // RPCManager will own SerialChannels and requires full type knowledge
#include "protocols/Command.hpp" // TODO: impl for the header stub
namespace milo {
  namespace core {

    struct Response;
    enum class Device : std::uint8_t { PG, PSU, Pump, Count };
    static_assert(static_cast<std::uint8_t>(Device::Count) == 3,
                  "Device count changed please update code that depends on it");
    inline const char* toString(Device d) {
      switch (d) {
      case Device::PG:
        return "PG";
      case Device::PSU:
        return "PSU";
      case Device::Pump:
        return "Pump";
      default:
        return "Unknown";
      }
    }
    class RPCManager {
    public:
      explicit RPCManager(std::shared_ptr<ErrorMonitor> errMonitor);
      ~RPCManager() = default;
      //---public APIs------------------------------------------------------
      void connect(); ///<- opens all SerialChannel objects
      void sendCommand(Device dev, const protocols::Command& cmd);
      Response awaitResponse(Device dev, std::chrono::milliseconds timeout);

    private:
      static constexpr speed_t kDefaultBaud = B115200;
      std::shared_ptr<ErrorMonitor> errorMonitor_;
      std::unordered_map<Device, io::SerialChannel> channels_;
      static constexpr std::array<std::pair<Device, const char*>, 3> symlinks_{
        { { Device::PSU, "/dev/psu1" }, { Device::PG, "/dev/pg1" }, { Device::Pump, "/dev/pump1" } }
      };
      bool connected_{ false };
    };

  } // namespace core
} // namespace milo