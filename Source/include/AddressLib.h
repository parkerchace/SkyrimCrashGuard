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

#include <string>

namespace AddressLib {

/**
 * @brief Force the address library to load and report whether it is usable.
 *
 * Note on semantics: CommonLibSSE NG does not return an error when the address
 * library for this runtime is missing or mismatched — REL::IDDB::get() calls
 * report_and_fail(), which shows a message box and terminates the process. So a
 * false return here only covers unexpected exceptions; the "no address library"
 * case never reaches this code. The check is kept because it forces the database
 * to load early, while there is still a log to write to.
 *
 * @return true if the address library is initialized and usable
 */
inline bool IsValid() {
    try {
        (void)REL::IDDB::get();
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Get the reason why address library initialization failed
 * @return String describing the reason, or "valid"
 */
inline std::string Reason() {
    try {
        (void)REL::IDDB::get();
        return "valid";
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "unknown error";
    }
}

// ResolveID() used to wrap a hand-copied {SE id, AE id} pair. It was removed with
// the move to CommonLibSSE NG: the ids it held are already in RE::VTABLE_* /
// RELOCATION_ID form inside CommonLibSSE, where they are maintained per runtime,
// and a wrapper that appears to fail softly is misleading — a missing id
// terminates the process inside REL::IDDB rather than returning an error.

} // namespace AddressLib
