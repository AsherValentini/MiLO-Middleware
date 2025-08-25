#pragma once
/** @file  Command.hpp
 *  @brief stub with toWire.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

// STL headers
#include <string>

namespace milo {
  namespace protocols {
    struct Command {
      std::string payload;
      std::string toWire() const { return payload + "\r\n"; }
    };

  } // namespace protocols
} // namespace milo