#pragma once
/** @file ButtonGPIO.hpp
 *brief Debounced push-button wrapper with short/long-press detection
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

#include "io/GPIOInput.hpp" // base class

#include <chrono>
#include <cstdint>

namespace milo {
  namespace io {

    /**
	 * @class ButtonGPIO
	 * @brief Concrete GPIOInput that classifies press types.
	 *
	 *  * Emits `ShortPress` after ≥ 50 ms but < 1 s.
	 *  * Emits `LongPress`  after ≥ 1 s hold.
	 *  * No copy, move-enabled (inherits fd_ ownership).
	 */

    class ButtonGPIO : public GPIOInput {

    public:
      enum class Event { ShortPress, LongPress };

      using Callback = std::function<void(Event)>;

      explicit ButtonGPIO(std::chrono::milliseconds longPressThresh = std::chrono::seconds{ 1 })
          : longThreshold_{ longPressThresh } {}

      void registerCallback(Callback cb) { cbButton_ = std::move(cb); }

      /** Called by owner loop (e.g., UI thread) every ~10 ms. */
      void poll(std::chrono::milliseconds now) override;

    private:
      void emitPress(Event e) {
        if (cbButton_) {
          if (cbButton_)
            cbButton_(e);
        }
      }

      Callback cbButton_{};

      std::chrono::milliseconds longThreshold_{ 1000 };
      bool presed_{ false };
      std::uint32_t pressTick_{ 0 };
    };

  } // namespace io
} // namespace milo