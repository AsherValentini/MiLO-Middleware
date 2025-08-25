#pragma once
/** @file  SerialChannel.hpp
 *  @brief Non-blocking UART line I/O wrapper (uses epoll/termios under the hood).
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <chrono>
#include <optional>
#include <string>

// Linux header
#include <termios.h> // for speed_t types e.g., B115200

namespace milo {
  namespace io {

    /**
 * @class SerialChannel
 * @brief RAII wrapper around a single /dev/tty* file descriptor.
 *
 *  * Frames I/O as ASCII lines (`\r\n`), CRC placeholder for now.
 *  * *Non-copyable*, but move-constructible.
 */

    class SerialChannel {

    public:
      //---ctr / dtr--------------------------------------------
      SerialChannel() = default;
      virtual ~SerialChannel(); // close the /dev/tty fs at destruction

      //---public API-------------------------------------------
      virtual bool open(const std::string& dev, speed_t baud);
      virtual bool writeLine(const std::string& line); // returns false on EIO
      virtual std::optional<std::string> readLine(std::chrono::milliseconds timeout);
      void close();

      //---non-copyable-----------------------------------------
      SerialChannel(const SerialChannel&) = delete;
      SerialChannel& operator=(const SerialChannel&) = delete;

      //---mv and mv assign-------------------------------------
      SerialChannel(SerialChannel&&) = default;
      SerialChannel& operator=(SerialChannel&&) = default;

    private:
      int fd_{ -1 };            ///< POSIX fs (-1==closed)
      std::string rx_buffer_{}; ///< buffer to store readLine content
    };
  } // namespace io
} // namespace milo