#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Cache info (text)
// ---------------------------------------------------------------------------

int RunCacheInfoText(const CacheInfoResult &InResult)
{
    std::cout << "Cache directory: " << InResult.mCacheDir << "\n";
    std::cout << "Exists: " << (InResult.mbCacheExists ? "yes" : "no") << "\n";
    if (InResult.mbCacheExists)
    {
        std::cout << "Size: " << FormatBytesHuman(InResult.mCacheSizeBytes)
                  << " (" << InResult.mCacheSizeBytes << " bytes)\n";
        std::cout << "Entries: " << InResult.mCacheEntryCount << "\n";
    }
    std::cout << "Current repo cache: " << InResult.mCurrentRepoCachePath
              << "\n";
    std::cout << "Current repo cached: "
              << (InResult.mbCurrentRepoCacheExists ? "yes" : "no") << "\n";
    std::cout << "Config dir override: "
              << (InResult.mConfigCacheDir.empty() ? "(default)"
                                                   : InResult.mConfigCacheDir)
              << "\n";
    std::cout << "Cache enabled: "
              << (InResult.mbCacheEnabled ? "true" : "false") << "\n";
    std::cout << "Cache verbose: "
              << (InResult.mbCacheVerbose ? "true" : "false") << "\n";
    std::cout << "INI path: " << InResult.mIniPath << "\n";
    PrintTextWarnings(InResult.mWarnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Cache clear (text)
// ---------------------------------------------------------------------------

int RunCacheClearText(const CacheClearResult &InResult)
{
    if (!InResult.mbSuccess)
    {
        std::cerr << "Error: " << InResult.mError << "\n";
        return 1;
    }
    if (InResult.mEntriesRemoved == 0 && InResult.mBytesFreed == 0)
    {
        std::cout << "Cache is already empty.\n";
    }
    else
    {
        std::cout << "Cleared " << InResult.mEntriesRemoved
                  << " entries, freed "
                  << FormatBytesHuman(InResult.mBytesFreed) << ".\n";
    }
    PrintTextWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Cache config (text)
// ---------------------------------------------------------------------------

int RunCacheConfigText(const CacheConfigResult &InResult)
{
    if (!InResult.mbSuccess)
    {
        std::cerr << "Error: " << InResult.mError << "\n";
        return 1;
    }
    std::cout << "Configuration written to: " << InResult.mIniPath << "\n";
    std::cout << "Effective config:\n";
    std::cout << "  dir     = "
              << (InResult.mDir.empty() ? "(default)" : InResult.mDir) << "\n";
    std::cout << "  enabled = " << (InResult.mbEnabled ? "true" : "false")
              << "\n";
    std::cout << "  verbose = " << (InResult.mbVerbose ? "true" : "false")
              << "\n";
    PrintTextWarnings(InResult.mWarnings);
    return InResult.mbSuccess ? 0 : 1;
}

} // namespace UniPlan
