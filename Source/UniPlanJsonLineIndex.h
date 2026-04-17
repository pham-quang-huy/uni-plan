#pragma once

#include <string>
#include <unordered_map>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// FJsonLineIndex — maps JSON paths (e.g., "phases[0].scope") to the 1-based
// line number of the key in the pretty-printed JSON source.
//
// Uses a single forward pass over the raw text, tracking an explicit path
// stack. Handles string escapes and nested objects/arrays. Works with any
// well-formed JSON; does not require nlohmann::json at build time.
// ---------------------------------------------------------------------------

class FJsonLineIndex
{
  public:
    // Parse raw JSON text; populate path→line map.
    void Build(const std::string &InText);

    // Return 1-based line for the given path, or -1 if not found.
    // Path format: dot-separated keys with [N] for array indices.
    // Examples: "phases[0].scope", "changelogs[2].affected", "summary".
    int LineFor(const std::string &InPath) const;

  private:
    std::unordered_map<std::string, int> mPathToLine;
};

} // namespace UniPlan
