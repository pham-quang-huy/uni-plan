#pragma once

#include "UniPlanDocumentTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Migration engine: convert .md governance documents to .json FDocument.
// ---------------------------------------------------------------------------

// Convert a single .md file to an FDocument.
// Uses existing ParseHeadingRecords + ParseMarkdownTables internally.
bool TryMigrateMarkdownToDocument(const fs::path &InRepoRoot,
                                  const std::string &InRelativePath,
                                  FDocument &OutDocument,
                                  std::string &OutError);

// Convert a single .md file to .json on disk.
// Writes <basename>.json next to the source .md file.
bool TryMigrateFileToJson(const fs::path &InRepoRoot,
                          const std::string &InRelativePath,
                          std::string &OutJsonPath, std::string &OutError);

// Migrate all documents for a given topic (plan + impl + playbooks
// + sidecars).
struct FMigrateTopicResult
{
    std::string mTopicKey;
    int mConverted = 0;
    int mFailed = 0;
    std::vector<std::string> mConvertedPaths;
    std::vector<std::string> mFailedPaths;
    std::vector<std::string> mErrors;
};

FMigrateTopicResult MigrateTopicToJson(const fs::path &InRepoRoot,
                                       const Inventory &InInventory,
                                       const std::string &InTopicKey);

// Migration status for an entire repository.
struct FMigrateStatusResult
{
    int mTotalDocuments = 0;
    int mJsonDocuments = 0;
    int mMarkdownDocuments = 0;
};

FMigrateStatusResult ComputeMigrateStatus(const fs::path &InRepoRoot);

} // namespace UniPlan
