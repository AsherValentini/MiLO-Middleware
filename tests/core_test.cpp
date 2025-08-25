#include "core/ErrorMonitor.hpp"
#include "core/RPCManager.hpp"
#include "io/SerialChannel.hpp"

#include "FakeSerialChannel.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace milo::core;
using namespace milo::test;

class MockErrorMonitor : public ErrorMonitor {
public:
  MOCK_METHOD(void, notifyFailure, (const std::string&), (override));
};

TEST(core_tests, passes) {}

