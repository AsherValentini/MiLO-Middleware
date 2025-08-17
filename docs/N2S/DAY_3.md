# Day 3 — “Stub-Fest”  
*2025-08-17*

| Timebox | What we shipped | Notes |
|---------|-----------------|-------|
| 09:00 – 12:00 | **Public-header scaffolding** for 9 core modules (`SystemCoordinator`, `Logger`, `RPCManager`, `ParameterStore`, `UIController`, `ErrorMonitor`, `ExperimentProtocol`, `ProtocolFactory`, `ConfigLoader`). | All headers compile in isolation; zero project cross-includes. |
| 12:00 – 13:00 | **clang-format détente** | New `.clang-format` (LLVM-based, 2-space, brace-attach). Verified on Clang 18; no IDE crashes. |
| 13:00 – 15:00 | **CMake targets updated** | Header-only interface libs (`milo_core`, `milo_ui`, `milo_protocols`). Presets build clean on host; cross build works after `stm32sdk` alias. |
| 15:00 – 16:00 | **GoogleTest shell** | Added via `FetchContent`. New `milo_tests` binary with 5 placeholder suites and a CTest hook (`add_test(NAME all …)`). |
| 16:00 – 17:00 | **CI still green** | Host Debug/Release + ARM Release compile in ≈3 min. CTest passes (0 failures). |

## Artifacts
* `include/` tree with 9 compile-clean headers  
* Updated `.clang-format`, `CMakeLists.txt`, `CMakePresets.json`  
* `tests/*.cpp` empty test stubs  
* GitHub workflow unchanged and passing

## Note: 
> I did not create stub for any concrete protocols. I will implement those headers when I have a clearer idea of what those 
protocols will look like. 

> Something that I found helpful: echoed the arm environment path to stm32sdk in bashrc file. Its just way easier to source the env. 

**Exits:** Build/CI baseline re-established; ready for Day 4 implementation stubs.  

