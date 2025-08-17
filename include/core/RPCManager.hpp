#pragma once
/** @file  RPCManager.hpp
 *  @brief Non-blocking serial RPC multiplexer.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <unordered_map>

namespace milo {
  namespace core {

    struct Command; // forward dcls to keep header include tight
    struct Response;
    enum class Device;
    class SerialChannel; // coming up from the IO layer

    class RPCManager {
    public:
      RPCManager() = default;
      ~RPCManager() = default;
      //---public APIs------------------------------------------------------
      void     connect(); ///<- opens all SerialChannel objects
      void     sendCommand(Device dev, const Command &cmd);
      Response awaitResponse(Device dev, std::chrono::milliseconds timeout);

    private:
      std::unordered_map<Device, SerialChannel> channels_;
    };

  } // namespace core
} // namespace milo