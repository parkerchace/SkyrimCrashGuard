// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

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
