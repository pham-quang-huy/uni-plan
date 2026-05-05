#pragma once

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace UniPlanTest
{

namespace fs = std::filesystem;

inline bool CreateUniqueDirectory(const fs::path &InRoot,
                                  const std::string &InPrefix,
                                  fs::path &OutPath, std::string &OutError)
{
    std::error_code Error;
    fs::create_directories(InRoot, Error);
    if (Error)
    {
        OutError = Error.message();
        return false;
    }

    const auto Seed =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for (int Attempt = 0; Attempt < 1000; ++Attempt)
    {
        const fs::path Candidate =
            InRoot / (InPrefix + "-" + std::to_string(Seed) + "-" +
                      std::to_string(Attempt));
        Error.clear();
        if (fs::create_directory(Candidate, Error))
        {
            OutPath = Candidate;
            return true;
        }
        if (Error && Error != std::make_error_code(std::errc::file_exists))
        {
            OutError = Error.message();
            return false;
        }
    }

    OutError = "failed to allocate unique temporary directory";
    return false;
}

inline int SetEnvironmentVariableForTest(const char *InName,
                                         const char *InValue,
                                         const bool InOverwrite)
{
#ifdef _WIN32
    if (!InOverwrite && std::getenv(InName) != nullptr)
    {
        return 0;
    }
    return ::_putenv_s(InName, InValue);
#else
    return ::setenv(InName, InValue, InOverwrite ? 1 : 0);
#endif
}

inline int UnsetEnvironmentVariableForTest(const char *InName)
{
#ifdef _WIN32
    return ::_putenv_s(InName, "");
#else
    return ::unsetenv(InName);
#endif
}

} // namespace UniPlanTest
