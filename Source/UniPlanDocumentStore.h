#pragma once

#include "UniPlanDocumentTypes.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Document Store — format-agnostic document access layer.
// All consumers (CLI commands, watch mode, mutation commands) use
// this layer instead of reading files directly.
// ---------------------------------------------------------------------------

// Load a document from disk (.json format only).
bool TryLoadDocument(const fs::path &InRepoRoot,
                     const std::string &InRelativePath, FDocument &OutDocument,
                     std::string &OutError);

// Save a document to disk as JSON.
bool TrySaveDocument(const fs::path &InRepoRoot, const FDocument &InDocument,
                     std::string &OutError);

// Extract a section from a loaded document by section ID.
// Returns empty FSectionContent with empty mSectionID if not found.
FSectionContent ResolveSectionFromDocument(const FDocument &InDocument,
                                           const std::string &InSectionID);

// Extract a table from a loaded document by table ID.
// Searches both top-level tables and tables within sections.
// Returns empty FStructuredTable with mTableID = -1 if not found.
FStructuredTable ResolveTableFromDocument(const FDocument &InDocument,
                                          int InTableID);

// Load a phase record from a playbook path (e.g.
// "Docs/Plans/X.Plan.json#playbook:P1"). Uses the bundle
// cache. Returns the FPhaseRecord directly — no table scanning.
bool TryLoadPhaseRecord(const fs::path &InRepoRoot,
                        const std::string &InPlaybookPath,
                        FPhaseRecord &OutPhase, std::string &OutError);

// Load a full FTopicBundle via the bundle cache. Path must be
// a .Plan.json file (no fragment). Used by watch mode to extract
// plan metadata (summary, goals, non-goals) from cached bundles.
bool TryLoadTopicBundleCached(const fs::path &InRepoRoot,
                              const std::string &InBundlePath,
                              const FTopicBundle *&OutBundle,
                              std::string &OutError);

// Clear the in-memory bundle cache. Call between snapshot
// rebuilds to pick up file changes.
void ClearBundleCache();

} // namespace UniPlan
