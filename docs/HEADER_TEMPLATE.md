# Header Templates 

## Overview 

| Category                                                                                              | Typical data members                                                                | Copy / move semantics we want               | Why                                                                                                                                                                                                 |
| ----------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------- | ------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **I/O wrappers**<br>(`SerialChannel`, `GPIOInput`/`ButtonGPIO`, `OLEDDisplay`, `FileLogger`)          | *Raw OS handles*—file descriptors, `libgpiod_line*`, `i2c_fd`, mmap’d frame buffers | **Non-copyable, move-enabled**              | Two objects pointing at the **same** fd or GPIO line would double-close it and cause undefined behaviour.  Moving is fine (transfers ownership).                                                    |
| **Core/UI/Protocol “logic” classes**<br>(`SystemCoordinator`, `ParameterStore`, `UIController`, etc.) | Only STL containers or small POD members **today** (no raw OS handles yet)          | **Leave default** (copyable *and* moveable) | They don’t currently manage unique resources, and making them non-copyable would complicate unit tests and container usage.  If a core class later gains unique resources, we’ll lock it down then. |

## "Logic" Classes Header Template 
```
#pragma once
/** @file  <ClassName>.hpp
 *  @brief <one-liner summary>.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <memory>        // <only the STL headers this class really needs>

namespace milo {
namespace <lib> { // e.g., core

/**
 * @class <ClassName>
 * @brief <longer description – 1-2 lines>.
 */
class <ClassName>
{
public:
  <ClassName>()  = default;
  ~<ClassName>() = default;

  // ---- public API ----------------------------------------------------------

  // (method stubs go here)

private:
  // data members (if any) – forward-decls only, no project headers
};

} // namespace lib // e.g., core
} // namespace milo
```
## I/O Header Template 

```
#pragma once
/** @file  <ClassName>.hpp
 *  @brief <one-liner summary>.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <memory>        // <only the STL headers this class really needs>

namespace milo {
namespace io {

/**
 * @class <ClassName>
 * @brief <longer description – 1-2 lines>.
 */
class <ClassName>
{
public:
  <ClassName>()  = default;
  ~<ClassName>() = default;

  // ---- public API ----------------------------------------------------------

  // (method stubs go here)

  // ---- non-copyable --------------------------------------------------------
  <ClassName>(const <ClassName>&)            = delete;
  <ClassName>& operator=(const <ClassName>&) = delete;

  // ---- move-enabled --------------------------------------------------------
  <ClassName>(<ClassName>&&)            = default;
  <ClassName>& operator=(<ClassName>&&) = default;

private:
  // data members (if any) – forward-decls only, no project headers
};

} // namespace io
} // namespace milo

```
