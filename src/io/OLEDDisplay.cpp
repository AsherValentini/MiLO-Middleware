#include "io/OLEDDisplay.hpp"
using namespace milo::io;

OLEDDisplay::~OLEDDisplay() { close(); }

bool OLEDDisplay::init(const std::string&, uint8_t) { return true; }
void OLEDDisplay::clear() {}
void OLEDDisplay::drawText(int, int, const std::string&) {}
void OLEDDisplay::drawBitmap(int, int, int, int, const std::vector<uint8_t>&) {}
void OLEDDisplay::flush() {}
void OLEDDisplay::close() { fd_ = -1; }

