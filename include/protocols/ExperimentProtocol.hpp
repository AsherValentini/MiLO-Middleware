#pragma once
/** @file  ExperimentProtocol.hpp
 *  @brief Abstract base class for all experiment-flow state-machines.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

namespace milo::core { // forward decls only—keeps dependency light
  class RPCManager;
  class Logger;
  class ParameterStore;
} // namespace milo::core

namespace milo::protocols {

  /**
 * @class ExperimentProtocol
 * @brief Common polymorphic interface that every concrete protocol
 *        (e.g. Lysis, PCR, Stain) must implement.
 *
 *  * Runs synchronously on the caller’s thread.
 *  * Owns no hardware—talks via RPCManager.
 *  * Logs key milestones to Logger.
 */
  class ExperimentProtocol {
  public:
    virtual ~ExperimentProtocol() = default;

    /**
     * @brief Execute the protocol’s finite-state machine.
     *
     * @param rpc   Handle to low-level RPC multiplexer.
     * @param log   CSV logger for high-level events / metrics.
     * @param store Global parameter cache (read-only access).
     */
    virtual void run(core::RPCManager &rpc, core::Logger &log,
                     const core::ParameterStore &store) = 0;
  };

} // namespace milo::protocols
