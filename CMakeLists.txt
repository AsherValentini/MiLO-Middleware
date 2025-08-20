cmake_minimum_required(VERSION 3.24)
project(milo-runtime VERSION 0.1.0 LANGUAGES CXX)

# Global compile settings 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MILO_ENABLE_ASAN "Enable AdressSanitizer in Debug" OFF)
option(MILO_ENABLE_UBSAN "Enable UBSanitizer in Debug" OFF)
option(MILO_ENABLE_TSAN "Enable ThreadSanitizer in Debug" OFF)
option(MILO_ENABLE_LTO  "Enable Link-Time Optimization" OFF)

# -----------------------------------------------------------------------------
# GoogleTest (host builds only)
# -----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Option to skip tests (used by cross-compile preset)
option(MILO_SKIP_TESTS "Disable GoogleTest + test target" OFF)

if(NOT MILO_SKIP_TESTS)
	FetchContent_MakeAvailable(googletest)
	enable_testing()
endif()


if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Werror -fno-omit-frame-pointer)
  if(CMAKE_BUILD_TYPE MATCHES "Release" AND MILO_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
    if(ipo_supported)
      set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
  endif()
endif()


add_library(milo_core  INTERFACE)
target_include_directories(milo_core INTERFACE include)

add_library(milo_protocols INTERFACE)
target_include_directories(milo_protocols INTERFACE include)

add_library(milo_ui    INTERFACE)
target_include_directories(milo_ui INTERFACE include)

# -----------------------------------------------------------------------------
# I/O layer (stub impls for now)
# -----------------------------------------------------------------------------
add_library(milo_io STATIC
    src/io/SerialChannel.cpp
    src/io/GPIOInput.cpp
    src/io/ButtonGPIO.cpp
    src/io/RotaryEncoderGPIO.cpp
    src/io/FileLogger.cpp
    src/io/OLEDDisplay.cpp
)
target_include_directories(milo_io PUBLIC include)

# -----------------------------------------------------------------------------
# Main Deamon Target
# -----------------------------------------------------------------------------
add_executable(milo-experimentd src/main.cpp)
target_link_libraries(milo-experimentd PRIVATE milo_core milo_io milo_protocols)

# -----------------------------------------------------------------------------
# Unit-test target
# -----------------------------------------------------------------------------
if(NOT MILO_SKIP_TESTS)
	add_executable(milo_tests
		tests/core_test.cpp
		tests/ui_test.cpp
		tests/rpc_test.cpp
		tests/param_test.cpp
		tests/logger_test.cpp
		tests/io_test.cpp
	)
	target_link_libraries(milo_tests
		PRIVATE
			gtest_main
			milo_core
			milo_ui
			milo_protocols
			milo_io
	)

	add_test(NAME all COMMAND milo_tests)
endif()


