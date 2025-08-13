Cross-build Enabling ─ Executive Summary (Day 1)

| Topic                     | Outcome                                                                          |
| ------------------------- | -------------------------------------------------------------------------------- |
| **Host OS**               | Ubuntu 22.04 (x86-64)                                                            |
| **Target**                | STM32MP1 Cortex-A7 (hard-float, NEON-VFPv4)                                      |
| **SDK version**           | **OpenSTLinux v6.6 (Scarthgap)** — Developer Package (tool-chain)                |
| **Starter Package**       | **FLASH-stm32mp1-openstlinux-6.6-yocto-scarthgap-mpu-v25.06.11.tar.gz** (1.3 GB) |
| **Tool-chain triplet**    | `arm-ostl-linux-gnueabi`                                                         |
| **Final sysroot path**    | `/opt/st/SDK/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi`                |
| **Project build presets** | `host-debug` (works)  •  `arm-release` (now works)                               |
| **Verified artefact**     | `build/arm-release/milo-experimentd` (cross-compiled)                            |

What I did

1. Installed the SDK (.sh) from the Developer Package
    Result: GCC tool-chain landed in /opt/st/SDK, but its target sysroot was empty.

2. Installed the Starter Package matching the same version.
    Rationale: Only the Starter archive contains the complete root-filesystem image with libc, crt1.o, libstdc++.so, …

3. Populated the SDK sysroot manually
    Steps
    - Mounted st-image-weston-…rootfs.ext4 read-only.
    - Copied /usr/lib, /lib, and the multi-arch sub-dir usr/lib/arm-ostl-linux-gnueabi into the SDK’s sysroot.
    - Outcome: All runtime objects & libraries are now where the linker expects them.

4. Aligned the tool-chain file (cmake/toolchain-stm32mp1.cmake)
```
# Use the sysroot chosen by the environment script
set(CMAKE_SYSROOT $ENV{SDKTARGETSYSROOT})

# Ensure compiler ABI matches hard-float NEON build
set(COMMON_FLAGS "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} ${CMAKE_CXX_FLAGS}")
```
Key idea: remove hard-coded armv7at2hf… path; always inherit $SDKTARGETSYSROOT.

5. Verified compiler / linker success
- Fresh shell → source environment-setup-…
- cmake --preset arm-release now passes its test link.
- cmake --build --preset arm-release yields milo-experimentd for ARM.

6. Where we stand
- Host builds (host-debug, host-release) — ✔ working
- Cross build (arm-release) — ✔ now links & generates binaries
- Board image — ready to flash: st-image-weston-openstlinux-…rootfs.ext4 from Starter Package
- Next actions (Day 2)
    - Flash SD-card with the Starter image (trusted TSV layout).
    - Deploy milo-experimentd to /usr/local/bin on the board.
    - Create a systemd service file (already stubbed in deploy/systemd/).
    - Begin integration / functional testing on hardware.

Lessons learned
- Developer-Package SDK lacks sysroot by design — always pair it with the same-version Starter image.
- Matching hard-float ABI flags (-mfpu=neon-vfpv4 -mfloat-abi=hard) are critical; otherwise the linker rejects mixed objects.
- Let the SDK environment choose SDKTARGETSYSROOT; resist hard-coding paths in the CMake tool-chain.


Important source for this project: 
https://wiki.st.com/stm32mpu/wiki/STM32MPU_Developer_Package#Checking_the_prerequisites
