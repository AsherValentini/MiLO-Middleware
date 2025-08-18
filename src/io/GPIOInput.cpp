/* @file SerialChannel.cpp
 * @brief stub impl
 *
 * © 2025 Milo Medical — MIT-licensed.
 */

#include "io/GPIOInput.hpp"

using namespace milo::io;

GPIOInput::~GPIOInput() = default;

bool GPIOInput::open(const std::string&, unsigned int, bool) { return true; }

void GPIOInput::close() {}

