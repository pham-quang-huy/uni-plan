#include "UniPlanPathHelpers.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

TEST(PathHelpers, FormatJSONRepoRootUsesPlatformStableGenericPath)
{
    std::error_code Error;
    const fs::path CanonicalRoot =
        fs::weakly_canonical(fs::current_path(), Error);
    ASSERT_FALSE(Error) << Error.message();

    const std::string RepoRoot =
        UniPlan::FormatJSONRepoRoot(CanonicalRoot.string());
    EXPECT_EQ(RepoRoot, CanonicalRoot.generic_string());
    EXPECT_EQ(RepoRoot.find('\\'), std::string::npos);

#ifdef _WIN32
    const std::string NativeRoot = CanonicalRoot.string();
    ASSERT_NE(NativeRoot.find('\\'), std::string::npos)
        << "Windows native input must exercise backslash conversion";
    EXPECT_NE(RepoRoot.find('/'), std::string::npos);
#else
    EXPECT_TRUE(fs::path(RepoRoot).is_absolute());
#endif
}
