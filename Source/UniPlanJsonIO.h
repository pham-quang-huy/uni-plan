#pragma once

#include "UniPlanDocumentTypes.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// JSON document I/O — read/write FDocument to/from .json files.
// Uses nlohmann/json internally but does not expose it in this header.
// ---------------------------------------------------------------------------

bool TryWriteDocumentJson(const FDocument &InDocument, const fs::path &InPath,
                          std::string &OutError);

bool TryReadDocumentJson(const fs::path &InPath, FDocument &OutDocument,
                         std::string &OutError);

} // namespace UniPlan
