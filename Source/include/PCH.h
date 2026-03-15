// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// ═══════════════════════════════════════════════════════════════════════
// Precompiled Header — Common includes for all source files
// ═══════════════════════════════════════════════════════════════════════

// SKSE and CommonLibSSE MUST come first
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

// AddressLib wrapper
#include "AddressLib.h"

// Windows headers AFTER CommonLibSSE
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

// Undefine Windows macros that conflict with CommonLibSSE
#undef GetObject  // Windows.h defines this as GetObjectW/GetObjectA

// Third-party libraries
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <Zydis/Zydis.h>
#include <nlohmann/json.hpp>
#include <toml.hpp>

// Standard library
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
