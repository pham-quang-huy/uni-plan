#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// INI data typedef
// ---------------------------------------------------------------------------

using IniData = std::map<std::string, std::map<std::string, std::string>>;

// ---------------------------------------------------------------------------
// Enum classes
// ---------------------------------------------------------------------------

enum class EDocumentKind
{
    Plan,
    Playbook,
    Implementation
};

// ---------------------------------------------------------------------------
// Core domain structs
// ---------------------------------------------------------------------------

struct DocConfig
{
    std::string mCacheDir; // [cache] dir — empty means use built-in default
    bool mbCacheEnabled = true;  // [cache] enabled — global toggle
    bool mbCacheVerbose = false; // [cache] verbose — print hit/miss to stderr
};

struct DocumentRecord
{
    EDocumentKind mKind = EDocumentKind::Plan;
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mStatusRaw;
    std::string mStatus;
    std::string mPath;
};

struct SidecarRecord
{
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mOwnerKind;
    std::string mDocKind;
    std::string mPath;
};

struct TopicPairRecord
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPlanStatus;
    std::string mImplementationPath;
    std::string mImplementationStatus;
    std::string mOverallStatus;
    std::vector<DocumentRecord> mPlaybooks;
    std::string mPairState;
};

struct Inventory
{
    std::string mGeneratedUtc;
    std::string mRepoRoot;
    std::vector<DocumentRecord> mPlans;
    std::vector<DocumentRecord> mPlaybooks;
    std::vector<DocumentRecord> mImplementations;
    std::vector<SidecarRecord> mSidecars;
    std::vector<TopicPairRecord> mPairs;
    std::vector<std::string> mWarnings;
};

struct MarkdownDocument
{
    fs::path mAbsolutePath;
    std::string mRelativePath;
};

} // namespace UniPlan
