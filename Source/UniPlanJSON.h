#pragma once

// ---------------------------------------------------------------------------
// Wrapper header isolating the nlohmann/json third-party dependency.
// Domain code should include this header, never json.hpp directly.
// ---------------------------------------------------------------------------

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include <nlohmann/json.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace UniPlan
{

using JSONValue = nlohmann::json;

} // namespace UniPlan
