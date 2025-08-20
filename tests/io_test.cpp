#include "io/SerialChannel.hpp"
#include <gtest/gtest.h>
#include <pty.h> // openpty
#include <unistd.h>

TEST(serial_channel, opens_writes_closes) {
  // create a false ttyUSB0 "device"
  int masterFd, slaveFd;
  char slaveName[64];
  ASSERT_EQ(0, openpty(&masterFd, &slaveFd, slaveName, nullptr, nullptr));

  // check that we can open a serial channel to slave dev
  milo::io::SerialChannel chan;
  ASSERT_TRUE(chan.open(slaveName, B115200));

  // Writer on master side
  const char* msg = "PING\r\n";
  write(masterFd, msg, strlen(msg));

  auto line = chan.readLine(std::chrono::milliseconds{ 100 });
  ASSERT_TRUE(line);
  EXPECT_EQ(*line, "PING");

  chan.writeLine("PONG");
  char buf[16] = { 0 };
  read(masterFd, buf, sizeof(buf));
  EXPECT_STREQ(buf, "PONG\r\n");
}

