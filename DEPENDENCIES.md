# SkyrimCrashGuard Dependencies

This document provides a comprehensive reference for all runtime and build dependencies required by SkyrimCrashGuard.

---

## Runtime Dependencies

These dependencies must be installed by end users to run SkyrimCrashGuard.

| Dependency | Version | Purpose | License | Documentation |
|------------|---------|---------|---------|---------------|
| **SKSE64** | 2.0.20+ (SE)<br>2.1.5+ (AE) | Script extender providing plugin API and advanced scripting capabilities | Custom OSS | [skse.silverlock.org](https://skse.silverlock.org/) |
| **SKSEVR** | 2.0.12+ (VR) | VR version of SKSE for SkyrimVR | Custom OSS | [skse.silverlock.org](https://skse.silverlock.org/) |
| **Address Library for SKSE** | Latest | Version-independent address resolution for game functions across SE/AE/VR | MIT | [GitHub](https://github.com/meh321/AddressLibraryDatabase) |

### System Requirements

| Component | Requirement | Purpose |
|-----------|-------------|---------|
| **Operating System** | Windows 10/11 (64-bit) | VEH, DirectX 11, modern Windows APIs |
| **DirectX** | DirectX 11 | ImGui overlay rendering |
| **RAM** | 4GB+ recommended | Memory monitoring and tracking systems |

### Supported Game Versions

| Game Version | Executable | Version | Status |
|--------------|------------|---------|--------|
| **Skyrim Special Edition** | SkyrimSE.exe | 1.5.97 (1.5.x) | ✓ Supported |
| **Skyrim Anniversary Edition** | SkyrimSE.exe | 1.6.x (all versions) | ✓ Supported |
| **Skyrim VR** | SkyrimVR.exe | 1.4.15 | ✓ Supported |

---

## Build Dependencies

These dependencies are required to compile SkyrimCrashGuard from source. All libraries are statically linked into the final DLL and do NOT need to be installed by end users.

### Build Tools

| Tool | Version | Purpose | Download |
|------|---------|---------|----------|
| **Visual Studio** | 2022+ | C++23 compiler (MSVC) with Desktop C++ workload | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake** | 3.21+ | Build system configuration | [cmake.org](https://cmake.org/download/) |
| **vcpkg** | Latest | C++ package manager for dependency management | [GitHub](https://github.com/microsoft/vcpkg) |

### C++ Libraries (vcpkg)

All C++ libraries are managed via vcpkg manifest mode (`vcpkg.json`) and automatically installed during CMake configuration.

| Library | Version | Purpose | License | Documentation |
|---------|---------|---------|---------|---------------|
| **CommonLibSSE-NG** | 3.6.0 | Modern C++ framework for SKSE plugin development with multi-runtime support (SE/AE/VR) | MIT | [GitHub](https://github.com/CharmedBaryon/CommonLibSSE-NG) |
| **spdlog** | 1.17.0+ | Fast, thread-safe C++ logging library for diagnostic output | MIT | [GitHub](https://github.com/gabime/spdlog) |
| **fmt** | 12.1.0+ | Modern C++ string formatting library (faster and safer than printf) | MIT | [GitHub](https://github.com/fmtlib/fmt) |
| **Zydis** | 4.1.1+ | x86/x64 disassembler for instruction-level crash analysis and pattern matching | MIT | [GitHub](https://github.com/zyantific/zydis) |
| **nlohmann-json** | 3.12.0+ | Modern C++ JSON library for crash reports and configuration data | MIT | [GitHub](https://github.com/nlohmann/json) |
| **toml11** | 4.4.0+ | TOML parser for configuration file parsing (SkyrimCrashGuard.toml) | MIT | [GitHub](https://github.com/ToruNiina/toml11) |
| **DirectXTK** | 2025-10-27+ | DirectX Tool Kit providing graphics utilities for DirectX 11 integration | MIT | [GitHub](https://github.com/microsoft/DirectXTK) |
| **Dear ImGui** | 1.91.9+ | Immediate mode GUI library for in-game overlay and configuration menu | MIT | [GitHub](https://github.com/ocornut/imgui) |

**ImGui Features:** `dx11-binding`, `win32-binding` (required for DirectX 11 integration)

### Testing Libraries (Optional)

These libraries are only required when building with `BUILD_TESTS=ON`. They are NOT included in release builds.

| Library | Version | Purpose | License | Documentation |
|---------|---------|---------|---------|---------------|
| **Catch2** | 3.12.0+ | Modern C++ unit testing framework | Boost Software License 1.0 | [GitHub](https://github.com/catchorg/Catch2) |
| **RapidCheck** | Latest | Property-based testing framework for C++ | BSD 2-Clause | [GitHub](https://github.com/emil-e/rapidcheck) |

### Windows System Libraries

These libraries are part of the Windows SDK and automatically available with Visual Studio.

| Library | Purpose |
|---------|---------|
| **d3d11.lib** | Direct3D 11 graphics API for rendering |
| **dxgi.lib** | DirectX Graphics Infrastructure for display management |
| **dbghelp.lib** | Debug help library for minidump generation |
| **psapi.lib** | Process Status API for memory tracking |
| **shell32.lib** | Windows Shell API for system integration |

---

## vcpkg Configuration

### Manifest Mode

SkyrimCrashGuard uses vcpkg manifest mode, which automatically installs dependencies during CMake configuration. The manifest is defined in `Source/vcpkg.json`.

### Target Triplet

**Triplet:** `x64-windows-static-md`

This triplet configuration:
- **x64**: 64-bit architecture (required for Skyrim SE/AE/VR)
- **windows**: Windows platform
- **static**: Static linking of libraries (all dependencies embedded in DLL)
- **md**: Dynamic CRT linkage (uses shared MSVC runtime)

### vcpkg Baseline

**Baseline Commit:** `d1e11918f5c88c1dd364b93e1452fea69bacd479`

This baseline ensures reproducible builds by pinning vcpkg package versions.

### Version Overrides

- **CommonLibSSE-NG**: Pinned to version 3.6.0 for stability

---

## Installation Instructions

### For End Users (Runtime)

1. Install SKSE64 (SE/AE) or SKSEVR (VR) for your game version
2. Install Address Library for SKSE Plugins (SE/AE/VR version)
3. Install SkyrimCrashGuard via mod manager or manual installation
4. Ensure DirectX 11 is installed (usually included with Windows 10/11)

### For Developers (Build from Source)

#### 1. Install Build Tools

```powershell
# Install Visual Studio 2022 with Desktop C++ workload
# Download from: https://visualstudio.microsoft.com/

# Install CMake 3.21+
# Download from: https://cmake.org/download/

# Clone and bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Set VCPKG_ROOT environment variable
$env:VCPKG_ROOT = "C:\vcpkg"
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
```

#### 2. Build SkyrimCrashGuard

```powershell
# Navigate to Source directory
cd SkyrimCrashGuard-master/Source

# Configure CMake (automatically installs vcpkg dependencies via manifest mode)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build --config Release
```

The vcpkg manifest (`vcpkg.json`) automatically handles all C++ library dependencies. No manual `vcpkg install` commands are required.

#### 3. Build with Tests (Optional)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --config Release
```

---

## Dependency Verification

### Verify vcpkg Installation

```powershell
# Check vcpkg version
& "$env:VCPKG_ROOT\vcpkg.exe" version

# List installed packages
& "$env:VCPKG_ROOT\vcpkg.exe" list

# Check specific package
& "$env:VCPKG_ROOT\vcpkg.exe" list commonlibsse-ng
```

### Verify Build Tools

```powershell
# Check CMake version
cmake --version

# Check Visual Studio installation
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" /?
```

### Verify Runtime Dependencies (End Users)

1. Check SKSE installation: Look for `skse64_loader.exe` (SE/AE) or `sksevr_loader.exe` (VR) in game directory
2. Check Address Library: Look for `Data/SKSE/Plugins/version-*.bin` files
3. Check DirectX 11: Run `dxdiag` and verify DirectX 11 support

---

## License Summary

All third-party dependencies use permissive open-source licenses that allow commercial and non-commercial use, modification, and distribution:

- **MIT License**: CommonLibSSE-NG, spdlog, fmt, Zydis, nlohmann-json, toml11, DirectXTK, Dear ImGui, Address Library
- **Custom OSS License**: SKSE64/SKSEVR (permissive, allows plugin development)
- **Boost Software License 1.0**: Catch2 (testing only)
- **BSD 2-Clause License**: RapidCheck (testing only)
- **Microsoft Software License**: Windows SDK libraries (system libraries)

Full license texts for each dependency can be found in their respective repositories. See [CREDITS.md](CREDITS.md) for detailed attribution and acknowledgments.

---

## Troubleshooting

### vcpkg Issues

**Problem:** CMake can't find vcpkg toolchain

**Solution:**
```powershell
# Ensure VCPKG_ROOT is set
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# Or specify toolchain explicitly
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

**Problem:** vcpkg package installation fails

**Solution:**
```powershell
# Update vcpkg to latest version
cd $env:VCPKG_ROOT
git pull
.\bootstrap-vcpkg.bat

# Clear vcpkg cache and retry
.\vcpkg remove --outdated
cmake -S . -B build --fresh
```

### Build Issues

**Problem:** C++23 features not recognized

**Solution:** Ensure Visual Studio 2022 (17.0+) is installed with the latest updates. C++23 support requires VS 2022 or newer.

**Problem:** Missing Windows SDK libraries

**Solution:** Install the Windows 10/11 SDK via Visual Studio Installer (included with Desktop C++ workload).

### Runtime Issues

**Problem:** Plugin fails to load in SKSE

**Solution:**
1. Verify SKSE version matches game version (SE 2.0.20+, AE 2.1.5+, VR 2.0.12+)
2. Check `SKSE/skse64.log` for error messages
3. Ensure Address Library is installed for your game version

**Problem:** DirectX 11 errors or overlay not working

**Solution:**
1. Update graphics drivers
2. Verify DirectX 11 support: Run `dxdiag` and check DirectX version
3. Check `SKSE/CrashGuard.log` for ImGui initialization errors

---

## Additional Resources

- **Build Verification Report**: [TASK_6.2_BUILD_DEPENDENCIES_VERIFICATION.md](TASK_6.2_BUILD_DEPENDENCIES_VERIFICATION.md)
- **Runtime Verification Report**: [TASK_6.1_RUNTIME_DEPENDENCIES_VERIFICATION.md](TASK_6.1_RUNTIME_DEPENDENCIES_VERIFICATION.md)
- **Credits and Attribution**: [CREDITS.md](CREDITS.md)
- **Project README**: [../README.md](../README.md)

---

**Last Updated:** 2026-03-13  
**Verified Against:** Source code commit with vcpkg.json baseline `d1e11918f5c88c1dd364b93e1452fea69bacd479`
