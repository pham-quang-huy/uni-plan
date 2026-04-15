#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Cache info (JSON)
// ---------------------------------------------------------------------------

int RunCacheInfoJson(const std::string &InRepoRoot,
                     const CacheInfoResult &InResult)
{
    PrintJsonHeader(kCacheInfoSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("cache_dir", InResult.mCacheDir);
    EmitJsonFieldNullable("config_cache_dir", InResult.mConfigCacheDir);
    EmitJsonField("ini_path", InResult.mIniPath);
    EmitJsonFieldBool("cache_enabled", InResult.mbCacheEnabled);
    EmitJsonFieldBool("cache_verbose", InResult.mbCacheVerbose);
    EmitJsonFieldBool("cache_exists", InResult.mbCacheExists);
    std::cout << "\"cache_size_bytes\":" << InResult.mCacheSizeBytes << ",";
    EmitJsonFieldInt("cache_entry_count", InResult.mCacheEntryCount);
    EmitJsonField("current_repo_cache_path", InResult.mCurrentRepoCachePath);
    EmitJsonFieldBool("current_repo_cache_exists",
                      InResult.mbCurrentRepoCacheExists);
    PrintJsonClose(InResult.mWarnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Cache clear (JSON)
// ---------------------------------------------------------------------------

int RunCacheClearJson(const std::string &InRepoRoot,
                      const CacheClearResult &InResult)
{
    PrintJsonHeader(kCacheClearSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("cache_dir", InResult.mCacheDir);
    EmitJsonFieldInt("entries_removed", InResult.mEntriesRemoved);
    std::cout << "\"bytes_freed\":" << InResult.mBytesFreed << ",";
    EmitJsonFieldBool("success", InResult.mbSuccess);
    EmitJsonFieldNullable("error", InResult.mError);
    PrintJsonClose(InResult.mWarnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Cache config (JSON)
// ---------------------------------------------------------------------------

int RunCacheConfigJson(const std::string &InRepoRoot,
                       const CacheConfigResult &InResult)
{
    PrintJsonHeader(kCacheConfigSchema, InResult.mGeneratedUtc, InRepoRoot);
    EmitJsonField("ini_path", InResult.mIniPath);
    EmitJsonFieldBool("success", InResult.mbSuccess);
    EmitJsonFieldNullable("error", InResult.mError);
    std::cout << "\"effective_config\":{";
    EmitJsonFieldNullable("dir", InResult.mDir);
    EmitJsonFieldBool("enabled", InResult.mbEnabled);
    EmitJsonFieldBool("verbose", InResult.mbVerbose, false);
    std::cout << "},";
    PrintJsonClose(InResult.mWarnings);
    return 0;
}

} // namespace UniPlan
