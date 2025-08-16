#pragma once

/** @file  SystemCoordinator.hpp
 *  @brief Public API for milo::core::SystemCoordinator.
 *
 *  © 2025 Milo Medical — licensed under MIT.
 */

#include <memory>
#include <string>

namespace milo {
  namespace core {

    class SystemCoordinator {

    public:
      SystemCoordinator() = default;
      ~SystemCoordinator() = default;

      // ---- Public API stubs (signatures only; no includes of other project headers) ----
      void initialize();  ///< Mount SD, load config, init subsystems
      void run();         ///< Main FSM loop
      void handleStart(); ///< User pressed “Start”
      void handleAbort(); ///< Emergency stop
      void handleError(const std::string &reason);

    private:
      enum class State { BOOT, INIT, IDLE, RUNNING, FINISHED, ERROR };

      void transitionTo(State next);

      State currentState_{ State::BOOT };
    };

  } // namespace core
} // namespace milo