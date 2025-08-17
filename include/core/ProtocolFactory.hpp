#pragma once
/** @file  ProtocolFactory.hpp
 *  @brief Runtime registry that maps protocol names to creators.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace milo::protocols {
  class ExperimentProtocol;
}

namespace milo::core {

  /**
 * @class ProtocolFactory
 * @brief Register & instantiate protocol objects by string key.
 *
 *  * Keeps SystemCoordinator decoupled from concrete protocols.
 *  * Creators are lambdas returning `unique_ptr<ExperimentProtocol>`.
 */
  class ProtocolFactory {
  public:
    using Creator = std::function<std::unique_ptr<protocols::ExperimentProtocol>()>;

    /// Register a protocol under \p name.  Returns false on duplicate.
    bool registerProtocol(const std::string &name, Creator maker);

    /// Create a fresh instance or throw `std::out_of_range` if unknown.
    std::unique_ptr<protocols::ExperimentProtocol> create(const std::string &name) const;

  private:
    std::unordered_map<std::string, Creator> creators_;
  };

} // namespace milo::core
