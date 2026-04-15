#pragma once

#include "UniPlanTypes.h"

#include <fstream>
#include <iostream>
#include <string>
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
