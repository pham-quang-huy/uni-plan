#pragma once

#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

namespace fs = std::filesystem;

// Shared mutation infrastructure used by every raw/semantic mutation command
// in UniPlanCommand*.cpp. Extracted from the CommandBundle monolith so
// lifecycle, evidence, and mutation sites can share the same writeback +
// auto-changelog contract without each file redeclaring its own static copy.

// Emit the canonical "{schema, ok, topic, target, changes[], auto_changelog}"
// JSON response for a successful mutation.
void EmitMutationJson(
    const std::string &InTopic, const std::string &InTarget,
    const std::vector<
        std::pair<std::string, std::pair<std::string, std::string>>> &InChanges,
    bool InAutoChangelog);

// Append an FChangeLogEntry describing the mutation to the bundle's
// mChangeLogs. Extracts the phase index from "phases[N]..." targets; leaves
// mPhase = -1 for topic-scoped targets like "plan" or "verifications[N]".
void AppendAutoChangelog(FTopicBundle &InOutBundle, const std::string &InTarget,
                         const std::string &InDescription);

// Persist the bundle to its source path. Returns 0 on success and writes the
// error string to OutError on failure.
int WriteBundleBack(const FTopicBundle &InBundle, const fs::path &InRepoRoot,
                    std::string &OutError);

} // namespace UniPlan
