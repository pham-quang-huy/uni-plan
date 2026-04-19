#pragma once

#include "UniPlanTopicTypes.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Topic bundle I/O — read/write FTopicBundle (plan-v4).
// ---------------------------------------------------------------------------

bool TryWriteTopicBundle(const FTopicBundle &InBundle, const fs::path &InPath,
                         std::string &OutError);

bool TryReadTopicBundle(const fs::path &InPath, FTopicBundle &OutBundle,
                        std::string &OutError);

} // namespace UniPlan
