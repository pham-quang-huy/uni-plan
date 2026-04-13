#pragma once

#include "UniPlanDocumentTypes.h"
#include "UniPlanTopicTypes.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// JSON document I/O — read/write FDocument to/from .json files.
// ---------------------------------------------------------------------------

bool TryWriteDocumentJson(const FDocument &InDocument, const fs::path &InPath,
                          std::string &OutError);

bool TryReadDocumentJson(const fs::path &InPath, FDocument &OutDocument,
                         std::string &OutError);

// ---------------------------------------------------------------------------
// Topic bundle I/O — read/write FTopicBundle (plan-bundle/v1).
// ---------------------------------------------------------------------------

bool TryWriteTopicBundle(const FTopicBundle &InBundle, const fs::path &InPath,
                         std::string &OutError);

bool TryReadTopicBundle(const fs::path &InPath, FTopicBundle &OutBundle,
                        std::string &OutError);

} // namespace UniPlan
