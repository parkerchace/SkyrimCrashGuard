#pragma once

// Force-include header to provide string_view literals for CommonLibSSE-NG generated code
// This ensures "sv" literal is available without manual edits to generated files

#include <string_view>

using namespace std::string_view_literals;
