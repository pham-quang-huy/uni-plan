#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

struct FFileFingerprint
{
    std::string mPath;
    std::string mRelativePath;
    uint64_t mWriteTime = 0;
    uint64_t mFileSize = 0;
};

struct FBundleFileIndexEntry
{
    FFileFingerprint mFingerprint;
    std::string mTopicKey;
};

struct FBundleFileIndexResult
{
    std::vector<FBundleFileIndexEntry> mBundles;
    std::vector<std::string> mWarnings;
    uint64_t mSignature = 0;
};

struct FMarkdownFileIndexResult
{
    std::vector<FFileFingerprint> mFiles;
    std::vector<std::string> mWarnings;
    uint64_t mSignature = 0;
};

bool operator==(const FFileFingerprint &InLeft,
                const FFileFingerprint &InRight);
bool operator!=(const FFileFingerprint &InLeft,
                const FFileFingerprint &InRight);

bool TryBuildBundleFileIndex(const fs::path &InRepoRoot,
                             FBundleFileIndexResult &OutIndex,
                             std::string &OutError);
bool TryBuildMarkdownFileIndex(const fs::path &InRepoRoot,
                               FMarkdownFileIndexResult &OutIndex,
                               std::string &OutError);

} // namespace UniPlan
