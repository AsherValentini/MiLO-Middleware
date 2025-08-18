/* @file SerialChannel.cpp
 * @brief stub impl
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

#include "io/SerialChannel.hpp"

using namespace milo::io;

SerialChannel::~SerialChannel() = default; ///< stubbing //TODO: close dev/tty fs

bool SerialChannel::open(const std::string&, int) {
  return true; // mock success in stub impl
}

bool SerialChannel::writeLine(const std::string&) {
  return true; // mock success in impl
}

std::optional<std::string> SerialChannel::readLine(std::chrono::milliseconds) {
  return std::nullopt; // mock no line in impl
}

void SerialChannel::close() {} // TODO: close dev//tty fs

