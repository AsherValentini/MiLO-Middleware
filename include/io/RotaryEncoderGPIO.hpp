#pragma once
/** @file  RotaryEncoderGPIO.hpp
 *  @brief Quadrature rotary-encoder wrapper that emits CW / CCW events.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include "io/GPIOInput.hpp" // base class (poll loop & edge helpers)

#include <functional>
#include <memory>

namespace milo {
  namespace io {

    /**
 * @class RotaryEncoderGPIO
 * @brief Uses two GPIOInput channels (A & B) to detect detents and direction.
 *
 *  * Non-copyable, move-enabled (owns both line FDs).
 *  * Simple state-machine: 2-bit Gray code → Direction event.
 */
    class RotaryEncoderGPIO {
    public:
      enum class Direction { CW, CCW };

      using Callback = std::function<void(Direction)>;

      RotaryEncoderGPIO() = default;
      ~RotaryEncoderGPIO() = default;

      /** Open the two GPIO pins for channels A & B. */
      bool open(const std::string& chip, unsigned int lineA, unsigned int lineB,
                bool activeLow = false);

      /** Poll both lines; call regularly (e.g., every 5–10 ms). */
      void poll(std::chrono::milliseconds now);

      void registerCallback(Callback cb) { cb_ = std::move(cb); }

      void close();

      // ─── non-copyable / move-enabled ────────────────────────────────────────
      RotaryEncoderGPIO(const RotaryEncoderGPIO&) = delete;
      RotaryEncoderGPIO& operator=(const RotaryEncoderGPIO&) = delete;
      RotaryEncoderGPIO(RotaryEncoderGPIO&&) = default;
      RotaryEncoderGPIO& operator=(RotaryEncoderGPIO&&) = default;

    private:
      void emit(Direction d) {
        if (cb_)
          cb_(d);
      }

      std::unique_ptr<GPIOInput> chanA_; ///< owns line A
      std::unique_ptr<GPIOInput> chanB_; ///< owns line B
      Callback cb_{};                    ///< user’s event sink

      uint8_t lastGray_{ 0 }; ///< last 2-bit state for detent detection
    };

  } // namespace io
} // namespace milo
