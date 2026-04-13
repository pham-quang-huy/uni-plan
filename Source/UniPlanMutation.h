#pragma once

#include "UniPlanDocumentTypes.h"
#include "UniPlanEnums.h"
#include "UniPlanMutationTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Mutation primitives — atomic document modification via document store.
// All mutations load any format, modify FDocument in memory, and
// always save as .json (architectural decision #2).
// ---------------------------------------------------------------------------

// Update the text content of a section.
bool TryUpdateSectionContent(const fs::path &InRepoRoot,
                             const std::string &InRelativePath,
                             const std::string &InSectionID,
                             const std::string &InNewContent,
                             FMutationResult &OutResult, std::string &OutError);

// Set a single field value in a section's fields map.
bool TryUpdateSectionField(const fs::path &InRepoRoot,
                           const std::string &InRelativePath,
                           const std::string &InSectionID,
                           const std::string &InFieldName,
                           const std::string &InFieldValue,
                           FMutationResult &OutResult, std::string &OutError);

// Append a row to a table within a section.
bool TryAppendTableRow(const fs::path &InRepoRoot,
                       const std::string &InRelativePath,
                       const std::string &InSectionID, int InTableIndex,
                       const std::vector<std::string> &InCellValues,
                       FMutationResult &OutResult, std::string &OutError);

// Update a specific cell in a table.
bool TryUpdateTableCell(const fs::path &InRepoRoot,
                        const std::string &InRelativePath,
                        const std::string &InSectionID, int InTableIndex,
                        int InRowIndex, const std::string &InColumnName,
                        const std::string &InNewValue,
                        FMutationResult &OutResult, std::string &OutError);

// ---------------------------------------------------------------------------
// Phase status state machine
// ---------------------------------------------------------------------------

// Validate whether a phase transition is allowed.
// Returns true if the transition is valid.
// On false, OutAllowedTargets contains the valid targets.
bool ValidatePhaseTransition(EPhaseStatus InCurrentStatus,
                             EPhaseStatus InTargetStatus,
                             std::vector<EPhaseStatus> &OutAllowedTargets);

// Compute a content hash (FNV-1a) for a serialized document.
std::string ComputeContentHash(const std::string &InContent);

} // namespace UniPlan
