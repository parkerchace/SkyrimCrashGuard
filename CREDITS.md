```markdown
# Skyrim Crash Guard — Credits

This project would not be possible without the incredible work of the Skyrim modding community and the developers of the open-source libraries I depend on.

## Third-Party Dependencies

### Core Framework Dependencies

#### CommonLibSSE-NG
- **Purpose**: Modern C++ library for SKSE plugin development with multi-runtime support (SE/AE/VR)
- **Version**: 3.6.0
- **License**: MIT License
- **Repository**: https://github.com/CharmedBaryon/CommonLibSSE-NG
- **Lineage**: Forked from [powerof3's CommonLibSSE](https://github.com/powerof3/CommonLibSSE), originally based on Ryan-rsm-McKenzie's CommonLibSSE
- **Description**: Provides game introspection, memory manipulation, and SKSE integration for Skyrim SE/AE/VR

#### SKSE64 (Skyrim Script Extender)
- **Purpose**: Script extender that enables advanced plugin functionality
- **License**: Custom open-source license
- **Website**: https://skse.silverlock.org/
- **Description**: Required runtime dependency that extends Skyrim's scripting capabilities and provides plugin API

#### Address Library for SKSE
- **Purpose**: Version-independent address resolution for game functions
- **License**: MIT License
- **Repository**: https://github.com/meh321/AddressLibraryDatabase
- **Description**: Enables plugins to work across multiple game versions without recompilation

### Logging and Formatting

#### spdlog
- **Purpose**: Fast C++ logging library
- **License**: MIT License
- **Repository**: https://github.com/gabime/spdlog
- **Description**: Provides high-performance, thread-safe logging with multiple output targets

#### fmt
- **Purpose**: Modern C++ formatting library
- **License**: MIT License
- **Repository**: https://github.com/fmtlib/fmt
- **Description**: Fast and safe alternative to printf and IOStreams for string formatting

### Disassembly and Analysis

#### Zydis
- **Purpose**: Fast x86/x64 disassembler and code generation library
- **License**: MIT License
- **Repository**: https://github.com/zyantific/zydis
- **Description**: Used for instruction-level crash analysis and pattern matching in L1b, L3, and L4 recovery layers

### Data Serialization

#### nlohmann/json
- **Purpose**: JSON for Modern C++
- **License**: MIT License
- **Repository**: https://github.com/nlohmann/json
- **Description**: Used for crash report generation, configuration data, and pattern learning system

#### toml11
- **Purpose**: TOML parser and serializer for C++11
- **License**: MIT License
- **Repository**: https://github.com/ToruNiina/toml11
- **Description**: Parses the SkyrimCrashGuard.toml configuration file

### Graphics and UI

#### DirectXTK (DirectX Tool Kit)
- **Purpose**: Collection of helper classes for DirectX 11 development
- **License**: MIT License
- **Repository**: https://github.com/microsoft/DirectXTK
- **Description**: Provides graphics utilities for DirectX 11 integration

#### Dear ImGui
- **Purpose**: Immediate mode graphical user interface library
- **License**: MIT License
- **Repository**: https://github.com/ocornut/imgui
- **Features Used**: dx11-binding, win32-binding
- **Description**: Powers the F11 in-game configuration menu and overlay system

### Testing Dependencies (Development Only)

#### Catch2
- **Purpose**: Modern C++ unit testing framework
- **License**: Boost Software License 1.0
- **Repository**: https://github.com/catchorg/Catch2
- **Description**: Used for unit testing during development (not included in release builds)

#### RapidCheck
- **Purpose**: Property-based testing framework for C++
- **License**: BSD 2-Clause License
- **Repository**: https://github.com/emil-e/rapidcheck
- **Description**: Used for property-based testing during development (not included in release builds)

### System Libraries

#### Windows SDK
- **Components Used**: dbghelp.lib, psapi.lib, d3d11.lib, dxgi.lib, shell32.lib
- **License**: Microsoft Software License
- **Description**: Windows system libraries for debugging, process management, and DirectX functionality

---

## AI Development Assistance

I developed this project with assistance from AI tools. In the interest of transparency:

### Tools Used

- **Kiro**: AI-powered IDE used for development
- **GitHub Copilot**: Code completion assistant
- **Claude 3.5, GPT-4.5, Gemini Pro, DeepSeek**: Used for code review, debugging, and documentation

All AI-generated code and documentation was reviewed and validated by me. The final responsibility for code quality and correctness rests with me as the sole developer.

---

## Special Thanks and Acknowledgments

### Skyrim Modding Community

This project would not exist without the incredible Skyrim modding community. Special thanks to:

- **CharmedBaryon** - For creating and maintaining CommonLibSSE-NG, the foundation that makes modern SKSE plugin development possible across SE/AE/VR
- **powerof3** - For the CommonLibSSE fork that CommonLibSSE-NG is based on, and for numerous contributions to the Skyrim modding ecosystem
- **Ryan-rsm-McKenzie** - For the original CommonLibSSE library, Trainwreck, and contributions to the crash logging ecosystem
- **meh321** - For the Address Library, enabling version-independent plugins and making cross-version compatibility feasible
- **The SKSE Team** - For decades of work on the Skyrim Script Extender, the backbone of advanced Skyrim modding
- **aers** - For SSE Engine Fixes, which pioneered many engine-level improvements for Skyrim

### Inspiration Sources

The architecture and design of SkyrimCrashGuard drew inspiration from:

- **Windows Exception Handling** - Structured Exception Handling (SEH) and Vectored Exception Handler (VEH) documentation and best practices
- **Existing Skyrim Stability Tools**:
  - **Crash Logger** (meh321) - Crash analysis and reporting patterns
  - **Trainwreck** (Ryan-rsm-McKenzie) - Modern crash logging approaches
  - **SSE Engine Fixes** (aers) - Engine-level stability improvements

### Community Testing and Feedback

While this project is in active development, I'm grateful to:

- **Early Testers** - Community members who test experimental builds and provide feedback on crash recovery effectiveness
- **Bug Reporters** - Users who take the time to submit detailed crash reports and reproduction steps
- **Documentation Reviewers** - Community members who help improve documentation clarity and accuracy

**Note**: As this is an experimental project, I encourage community involvement. If you've contributed testing, bug reports, or feedback, thank you for helping improve SkyrimCrashGuard!

### Development Tools

Special recognition for the open-source tools that made development possible:

- **Visual Studio** - Primary development environment
- **CMake** - Build system configuration
- **vcpkg** - C++ package management
- **Git** - Version control
- **GitHub** - Project hosting and collaboration

---

## License Summary

All third-party dependencies use permissive open-source licenses (primarily MIT) that allow commercial and non-commercial use, modification, and distribution. Full license texts for each dependency can be found in their respective repositories.

SkyrimCrashGuard itself is released under the MIT License (see LICENSE file in root directory).

```
