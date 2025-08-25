#pragma once
/** @file  FakeSerialChannel.hpp
 *  @brief SerialChannel derivative with controlled method outputs for RPCManager testing.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include "io/SerialChannel.hpp"

namespace milo {
  namespace test {

    /**
 * @class FakeSerialChannel
 * @brief .
 */
    class FakeSerialChannel : public milo::io::SerialChannel {
    public:
      bool open_called = false;
      bool write_sucess = true;
      std::optional<std::string> next_read_line = "OK\r\n";

      bool open(const std::string&, speed_t) override {
        open_called = true;
        return true;
      }

      bool writeLine(const std::string& line) override {
        last_written = line;
        return write_sucess;
      }

      std::optional<std::string> readLine(std::chrono::milliseconds) override {
        return next_read_line;
      }

    private:
      std::string last_written;
    };

  } // namespace test
} // namespace milo