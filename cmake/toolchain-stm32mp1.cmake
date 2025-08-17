
if(NOT DEFINED ENV{SDKTARGETSYSROOT})
  message(FATAL_ERROR
    "SDKTARGETSYSROOT not set. Source the STM32MP1 SDK environment script first.") # source /opt/st/SDK/environment-setup-cortexa7hf-neon-vfpv4-ostl-linux-gnueabi
endif()


# cmake/toolchain-stm32mp1.cmake
# Minimal cross toolchain skeleton (will adjust TOOLCHAIN_PREFIX to our SDK later)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# --- Ensure compiler matches cortexa7 hard-float sysroot ---
set(COMMON_FLAGS "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")

set(CMAKE_C_FLAGS   "${COMMON_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} ${CMAKE_CXX_FLAGS}")


# Common triplet for many ARMhf toolchains; will need to change if our SDK differs
set(TOOLCHAIN_PREFIX arm-ostl-linux-gnueabi)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)


# Let the environment script decide the right sysroot
set(CMAKE_SYSROOT $ENV{SDKTARGETSYSROOT})

# Add this to help CMake find includes/libs
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# Speed up/find behavior — i’ll refine once SDK path is known.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
