#pragma once
/** @file  GPIOInput.hpp
 *  @brief Abstract, debounced edge-input wrapper for a single GPIO line.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <chrono>
#include <functional>
#include <string>

namespace milo {
  namespace io {

    /**
 * @class GPIOInput
 * @brief Base-class that owns one Linux GPIO line, debounces its state,
 *        and invokes a user-supplied callback on rising/falling edges.
 *
 *  * Non-blocking: `poll()` is called periodically by the owner thread.
 *  * No copy, move-enabled (sole owner of the line handle).
 */
    class GPIOInput {
    public:
      enum class Edge { Rising, Falling };

      using Callback = std::function<void(Edge)>;

      GPIOInput() = default;
      virtual ~GPIOInput(); ///< auto-release line

      /** @returns false if the GPIO chip/line cannot be opened. */
      bool open(const std::string& chip, ///< e.g. "/dev/gpiochip0"
                unsigned int line,       ///< pin number
                bool activeLow = false);

      /** Polls the line and emits debounced edge events.  To be called from the
      owner’s loop (UI thread or epoll mux). */
      virtual void poll(std::chrono::milliseconds now) = 0;

      void registerCallback(Callback cb) { cb_ = std::move(cb); }

      void close();

      // ─── non-copyable, move-enabled ───────────────────────────────────────────
      GPIOInput(const GPIOInput&) = delete;
      GPIOInput& operator=(const GPIOInput&) = delete;
      GPIOInput(GPIOInput&&) = default;
      GPIOInput& operator=(GPIOInput&&) = default;

    protected:
      /** Derived classes call this when they detect a debounced edge. */
      void emit(Edge e) {
        if (cb_)
          cb_(e);
      }

      int fd_{ -1 }; ///< gpiod line FD (-1 = closed)
      Callback cb_{};

      // simple debounce helpers
      bool lastState_{ false };
      uint32_t lastTick_{ 0 }; ///< msec timestamp of last change
    };

  } // namespace io
} // namespace milo
