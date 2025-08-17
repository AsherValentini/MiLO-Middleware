#pragma once
/** @file  UIController.hpp
 *  @brief Rotary-encoder & OLED front-panel controller (runs its own thread).
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <functional>

namespace milo {
  namespace core { // forward decls so we don’t pull core headers in
    enum class Parameter;
    enum class SystemState;
  } // namespace core

  namespace ui {

    /** High-level user-interaction events emitted by the front panel. */
    enum class UIEvent {
      KnobLeft,
      KnobRight,
      ButtonPress,
      ButtonLongPress,
      // …
    };

    /**
 * @class UIController
 * @brief Owns GPIO polling for the rotary-encoder and pushes frames to the OLED.
 *
 * * Emits `UIEvent` callbacks for the application layer.
 * * Provides convenience setters so other modules can update the screen without
 *   knowing display internals.
 */
    class UIController {

    public:
      UIController() = default;
      ~UIController() = default;

      // ---- public API ----------------------------------------------------------
      void start(); ///< launch background worker thread (poll + draw)

      /// Register a lambda or free function to receive front-panel events.
      void registerCallback(std::function<void(UIEvent)> cb);

      /// Change the high-level screen (BOOT, IDLE, RUNNING…).
      void setDisplayState(core::SystemState state);

      /// Show a live numeric readout on the HUD.
      void showParameterValue(core::Parameter param, float value);

    private:
      // internal helpers — implementation lives in .cpp
      void pollEncoder();
      void flushOLED();

      std::function<void(UIEvent)> cb_{};
    };

  } // namespace ui
} // namespace milo
