#pragma once

#include "UniPlanDocumentTypes.h"
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

// Load a document from disk. Detects format from extension:
// .json → TryReadDocumentJson, .md → TryMigrateMarkdownToDocument.
bool TryLoadDocument(const fs::path &InRepoRoot,
                     const std::string &InRelativePath, FDocument &OutDocument,
                     std::string &OutError);

// Save a document to disk as JSON (per architectural decision #2).
// Always writes .json regardless of the original format.
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

} // namespace UniPlan
