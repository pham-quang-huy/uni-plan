#include "UniPlanRuntime.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <string>

namespace UniPlan
{

CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig)
{
    CacheConfigResult Result;
    Result.mGeneratedUtc = GetUtcNow();

    const fs::path ExeDir = ResolveExecutableDirectory();
    const fs::path IniPath = ExeDir / "uni-plan.ini";
    Result.mIniPath = IniPath.string();

    // Start with current effective values
    std::string EffectiveDir = InCurrentConfig.mCacheDir;
    std::string EffectiveEnabled =
        InCurrentConfig.mbCacheEnabled ? "true" : "false";
    std::string EffectiveVerbose =
        InCurrentConfig.mbCacheVerbose ? "true" : "false";

    // Merge only explicitly-set fields (mbDirSet distinguishes "not passed"
    // from "set to empty")
    if (InOptions.mbDirSet)
    {
        EffectiveDir = InOptions.mDir;
    }
    if (!InOptions.mEnabled.empty())
    {
        EffectiveEnabled = InOptions.mEnabled;
    }
    if (!InOptions.mVerbose.empty())
    {
        EffectiveVerbose = InOptions.mVerbose;
    }

    std::string WriteError;
    if (!TryWriteDocIni(IniPath, EffectiveDir, EffectiveEnabled,
                        EffectiveVerbose, WriteError))
    {
        Result.mbSuccess = false;
        Result.mError = WriteError;
    }

    // Re-read to get effective config
    const DocConfig NewConfig = LoadConfig(ExeDir);
    Result.mDir = NewConfig.mCacheDir;
    Result.mbEnabled = NewConfig.mbCacheEnabled;
    Result.mbVerbose = NewConfig.mbCacheVerbose;

    return Result;
}

} // namespace UniPlan
