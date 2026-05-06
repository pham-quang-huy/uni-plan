#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace UniPlan
{

inline std::string ToGenericPath(const std::filesystem::path &InPath)
{
    return InPath.generic_string();
}

inline std::string ToCanonicalGenericPath(
    const std::filesystem::path &InPath)
{
    if (InPath.empty())
    {
        return {};
    }

    std::error_code Error;
    const std::filesystem::path AbsolutePath =
        InPath.is_absolute() ? InPath
                             : std::filesystem::absolute(InPath, Error);
    if (Error)
    {
        return InPath.lexically_normal().generic_string();
    }

    const std::filesystem::path CanonicalPath =
        std::filesystem::weakly_canonical(AbsolutePath, Error);
    if (Error)
    {
        return AbsolutePath.lexically_normal().generic_string();
    }
    return CanonicalPath.generic_string();
}

inline std::string ToCanonicalGenericPath(const std::string &InPath)
{
    return ToCanonicalGenericPath(std::filesystem::path(InPath));
}

inline std::string FormatJSONRepoRoot(const std::string &InRoot)
{
    return ToCanonicalGenericPath(InRoot);
}

} // namespace UniPlan
