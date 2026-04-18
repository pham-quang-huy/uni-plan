#pragma once

#include "UniPlanTypes.h"

#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#ifdef __APPLE__
#include <climits>
#include <mach-o/dyld.h>
#endif

namespace UniPlan
{

inline std::string ToGenericPath(const fs::path &InPath)
{
    return InPath.generic_string();
}

// Resolve a repo-relative (or absolute) path against InRepoRoot.
// Absolute InFilePath is returned unchanged; relative InFilePath is
// joined onto InRepoRoot. Empty InFilePath returns empty.
inline fs::path ResolveRepoRelativePath(const fs::path &InRepoRoot,
                                        const std::string &InFilePath)
{
    if (InFilePath.empty())
        return {};
    fs::path P(InFilePath);
    if (P.is_absolute())
        return P;
    return InRepoRoot / P;
}

// Check whether a file_manifest path exists on disk, resolved against
// InRepoRoot. Empty paths and filesystem errors map to false.
inline bool ManifestPathExists(const fs::path &InRepoRoot,
                               const std::string &InFilePath)
{
    if (InFilePath.empty())
        return false;
    const fs::path Resolved = ResolveRepoRelativePath(InRepoRoot, InFilePath);
    std::error_code EC;
    const bool bExists = fs::exists(Resolved, EC);
    return bExists && !EC;
}

inline bool TryReadFileLines(const fs::path &InPath,
                             std::vector<std::string> &OutLines,
                             std::string &OutError)
{
    std::ifstream Input(InPath);
    if (!Input.is_open())
    {
        OutError = "Unable to open file.";
        return false;
    }
    std::string Line;
    while (std::getline(Input, Line))
    {
        OutLines.push_back(Line);
    }
    if (Input.bad())
    {
        OutError = "File read failure.";
        return false;
    }
    return true;
}

inline void PrintRepoInfo(const fs::path &InRepoRoot)
{
    std::cerr << "[repo: " << InRepoRoot.string() << "]\n";
}

inline void PrintScanInfo(size_t InDocCount)
{
    std::cerr << "[scanned " << InDocCount << " docs]\n";
}

inline fs::path GetExecutableDirectory()
{
#ifdef __APPLE__
    char RawPath[4096];
    uint32_t Size = sizeof(RawPath);
    if (_NSGetExecutablePath(RawPath, &Size) == 0)
    {
        char Resolved[PATH_MAX];
        if (realpath(RawPath, Resolved) != nullptr)
        {
            return fs::path(Resolved).parent_path();
        }
    }
#endif
    // Fallback: current working directory
    return fs::current_path();
}

} // namespace UniPlan
