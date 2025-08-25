#pragma once
/** @file  Response.hpp
 *  @brief stub with fromWire.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

// STL headers
#include <optional>
#include <string>

namespace milo {
  namespace protocols {
    struct Response {
      static std::optional<Response> fromWire(const std::string&) {
        // TODO: parsing impl and if failed return std::nullopt
        Response response;
        return response;
      }
    };
  } // namespace protocols
} // namespace milo