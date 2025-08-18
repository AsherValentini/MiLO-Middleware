#include "io/ButtonGPIO.hpp"
using namespace milo::io;

void ButtonGPIO::poll(std::chrono::milliseconds) {
  // TODO real debounce; for now do nothing
}

