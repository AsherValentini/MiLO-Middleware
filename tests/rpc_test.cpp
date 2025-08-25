// MILO-Prod headers
#include "core/ErrorMonitor.hpp"
#include "core/RPCManager.hpp"
#include "io/SerialChannel.hpp"
#include "protocols/Command.hpp"

// MILO-Fake headers
#include "FakeSerialChannel.hpp"

// GTest headers
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace milo::test {

  using milo::core::Device;
  using milo::core::ErrorMonitor;
  using milo::core::RPCManager;
  using milo::protocols::Command;
  using milo::test::
      FakeSerialChannel; // is this required since we are in the namespace milo::test and FakeSerialChannel lives in milo::test np

  class MockErrorMonitor : public ErrorMonitor {
  public:
    MOCK_METHOD(void, notifyFailure, (const std::string&),
                (override)); // do we need to mock if we have defined a stub impl
  };

  class RPCManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
      errorMonitor = std::make_shared<testing::NiceMock<MockErrorMonitor>>();

      // Upcast to base class for RPCManager ctor
      manager = std::make_unique<RPCManager>(std::static_pointer_cast<ErrorMonitor>(errorMonitor));

      // Inject fakes
      for (auto dev : { Device::PG, Device::PSU, Device::Pump }) {
        auto fake = std::make_unique<FakeSerialChannel>();
        fakeChannels[dev] = fake.get();            // raw ptr for assertions
        manager->channels_[dev] = std::move(fake); // move into manager
      }

      manager->connected_ = true; // bypass connect logic
    }

    std::shared_ptr<testing::NiceMock<MockErrorMonitor>> errorMonitor;
    std::unique_ptr<RPCManager> manager;
    std::unordered_map<Device, FakeSerialChannel*> fakeChannels;
  };

  TEST_F(RPCManagerTest, sendCommand_WritesWireFormatToCorrectChannel) {
    Command dummyCmd;
    dummyCmd.payload = "TEST123";

    manager->sendCommand(Device::PG, dummyCmd);

    FakeSerialChannel* fakePG = fakeChannels[Device::PG];
    ASSERT_TRUE(fakePG->open("/dev/dummy", B115200));
    ASSERT_EQ(fakePG->getLastWritten(), "TEST123\r\n");
  }

} // namespace milo::test

