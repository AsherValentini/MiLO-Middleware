/* @file SerialChannel.cpp
 * @brief IO abstraction layer that wraps ttyUSBx - handles file descriptor, framing, line io and RAII - POSIX compliant
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

// STL headers
#include <cstddef>
#include <cstring> // for strerror
#include <iostream>

// Linux headers
#include <errno.h> // Error integer and strerror() function
#include <fcntl.h> // Contains file controls like O_RDWR
#include <poll.h>
#include <unistd.h> // write(), read(), close()

// MiLO headers
#include "io/SerialChannel.hpp"

using namespace milo::io;

SerialChannel::~SerialChannel() { close(); }

bool SerialChannel::open(const std::string& dev, speed_t baud) {
  // open non-blocking, dont become ctrl-TTY
  fd_ = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::cerr << "Error " << errno << " from open: " << strerror(errno) << "\n";
    return false;
  }

  // fetch current attrs
  struct termios tty;
  if (tcgetattr(fd_, &tty) != 0) {
    std::cerr << "Error " << errno << " from tcgetattr: " << strerror(errno) << "\n";
    close();
    return false;
  }

  cfmakeraw(&tty);
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);

  cfsetispeed(&tty, baud);
  cfsetospeed(&tty, baud);

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    std::cerr << "Error " << errno << " from tcsetattr: " << strerror(errno) << "\n";
    close();
    return false;
  }
  return true;
}

bool SerialChannel::writeLine(const std::string& line) {

  if (fd_ < 0) {
    return false;
  }

  std::string out = line;
  if (!out.ends_with("\r\n")) {
    out += "\r\n";
  }

  // Good Pattern for POSIX write loop (required if the tty blocks for instance)
  std::size_t total = 0;
  while (total < out.size()) {
    ssize_t written = ::write(fd_, out.data() + total, out.size() - total);
    if (written > 0) {
      total += written;
    } else if (written == -1 && errno == EINTR) {
      continue; // try again
    } else if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // TODO: wait on epoll or select; for now just retry
      continue;
    } else {
      std::cerr << "Error: " << errno << " from write: " << strerror(errno) << "\n";
      return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------
// SerialChannel::readLine
// Non-blocking line reader with timeout and internal buffer.
// Returns std::nullopt on timeout, disconnect, or error.
// -------------------------------------------------------------------
std::optional<std::string> SerialChannel::readLine(std::chrono::milliseconds timeout) {
  if (fd_ < 0)
    return std::nullopt;

  char temp[256];
  pollfd pfd{ fd_, POLLIN, 0 };

  const auto deadline = std::chrono::steady_clock::now() + timeout;

  while (std::chrono::steady_clock::now() < deadline) {

    auto ms_left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    int ms = static_cast<int>(ms_left.count());

    int rc = ::poll(&pfd, 1, ms);
    if (rc == -1) {
      if (errno == EINTR)
        continue; // interrupted → retry
      std::cerr << "poll: " << strerror(errno) << '\n';
      return std::nullopt;
    }
    if (rc == 0)
      break; // timeout

    if (pfd.revents & POLLIN) {
      ssize_t n = ::read(fd_, temp, sizeof(temp));
      if (n > 0) {
        rx_buffer_.append(temp, n);
      } else if (n == 0) { // EOF / disconnect
        close();
        return std::nullopt;
      } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue; // transient → retry
      } else {
        std::cerr << "read: " << strerror(errno) << '\n';
        return std::nullopt;
      }

      // Check for complete line
      if (auto pos = rx_buffer_.find("\r\n"); pos != std::string::npos) {
        std::string line = rx_buffer_.substr(0, pos);
        rx_buffer_.erase(0, pos + 2); // remove line + CRLF
        return line;
      }
    }
  }
  return std::nullopt; // timeout/partial
}

void SerialChannel::close() {
  if (fd_ >= 0)
    ::close(fd_);
  fd_ = -1;
}

