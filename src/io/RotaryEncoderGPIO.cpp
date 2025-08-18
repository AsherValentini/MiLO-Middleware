#include "io/RotaryEncoderGPIO.hpp"
using namespace milo::io;

bool RotaryEncoderGPIO::open(const std::string& chip, unsigned int a, unsigned int b, bool low) {
  // parameters currently unused in stub
  (void)chip;
  (void)a;
  (void)b;
  (void)low;

  // real implementation will create concrete GPIOInput derivatives
  chanA_.reset();
  chanB_.reset();
  return true;
}

void RotaryEncoderGPIO::poll(std::chrono::milliseconds now) {
  if (chanA_)
    chanA_->poll(now);
  if (chanB_)
    chanB_->poll(now);
}

void RotaryEncoderGPIO::close() {
  chanA_.reset();
  chanB_.reset();
}

