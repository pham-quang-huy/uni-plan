#pragma once

#include "UniPlanDocumentTypes.h"

#include <map>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Topic bundle types — one file per topic containing all governance
// documents (plan + implementation + playbooks + changelogs +
// verifications).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FChangeLogEntry — one entry in a changelog.
// ---------------------------------------------------------------------------

struct FChangeLogEntry
{
    std::string mDate;
    std::string mChange;
    std::string mFiles;
    std::string mEvidence;
};

// ---------------------------------------------------------------------------
// FVerificationEntry — one entry in a verification record.
// ---------------------------------------------------------------------------

struct FVerificationEntry
{
    std::string mDate;
    std::string mCheck;
    std::string mResult;
    std::string mDetail;
};

// ---------------------------------------------------------------------------
// FTopicBundle — complete governance bundle for one topic.
// Stored as a single <TopicKey>.Plan.json file.
// Schema: "uni-plan://plan-bundle/v1"
// ---------------------------------------------------------------------------

struct FTopicBundle
{
    std::string mTopicKey;
    std::string mStatus;
    int mSchemaVersion = 1;

    // Core documents
    FDocument mPlan;
    FDocument mImplementation;
    std::map<std::string, FDocument> mPlaybooks; // key = phase

    // Evidence (key = "plan", "implementation", or phase key)
    std::map<std::string, std::vector<FChangeLogEntry>> mChangeLogs;
    std::map<std::string, std::vector<FVerificationEntry>> mVerifications;
};

} // namespace UniPlan
