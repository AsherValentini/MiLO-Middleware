#pragma once
/** @file  OLEDDisplay.hpp
 *  @brief Tiny-I²C 128×64 monochrome OLED wrapper.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace milo {
  namespace io {

    /**
 * @class OLEDDisplay
 * @brief Convenience API for text + bitmap blitting; hides low-level I²C.
 *
 *  * Owns the file-descriptor to /dev/i2c-*.
 *  * Keeps an off-screen frame-buffer; `flush()` pushes deltas.
 */
    class OLEDDisplay {
    public:
      OLEDDisplay() = default;
      ~OLEDDisplay(); ///< auto-closes fd

      //---public API---------------------------------------------------------
      bool init(const std::string& devPath = "/dev/spidev0.0",
                uint8_t dcPin = 24); // data/command GPIO for 4-wire SPI

      /* Text helpers --------------------------------------------------------- */
      void clear();
      void drawText(int x, int y, const std::string& utf8);

      /* Bitmap helper -------------------------------------------------------- */
      void drawBitmap(int x, int y, int w, int h, const std::vector<uint8_t>& monoBits);

      void flush(); ///< push buffer to panel
      void close();

      /* non-copyable, move-enabled ------------------------------------------ */
      OLEDDisplay(const OLEDDisplay&) = delete;
      OLEDDisplay& operator=(const OLEDDisplay&) = delete;
      OLEDDisplay(OLEDDisplay&&) = default;
      OLEDDisplay& operator=(OLEDDisplay&&) = default;

    private:
      int fd_{ -1 }; ///< SPI device fd
      bool dirty_{ false };
      uint8_t buffer_[1024]{}; ///< 128×64 / 8 bits per byte
    };

  } // namespace io
} // namespace milo
