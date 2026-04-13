#pragma once

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Validation result types extracted from UniPlanTypes.h.
// These structs hold the output of the 28 validation checks in
// UniPlanValidation.cpp.
// ---------------------------------------------------------------------------

struct ValidateCheck
{
    std::string mId;
    bool mbOk = true;
    bool mbCritical = false;
    std::string mDetail;
    std::string mRuleId;
    std::vector<std::string> mDiagnostics;
};

struct ActivePhaseRecord
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPhaseKey;
    std::string mStatusRaw;
    std::string mStatus;
};

struct PhaseEntryGateResult
{
    int mActivePhaseCount = 0;
    int mMissingPlaybookCount = 0;
    int mUnpreparedPlaybookCount = 0;
};

struct ArtifactRoleBoundaryResult
{
    int mPlaybookViolationCount = 0;
    int mImplementationViolationCount = 0;
};

struct PlanSchemaValidationResult
{
    int mPlanCount = 0;
    int mReadFailureCount = 0;
    int mMissingRequiredPlanCount = 0;
    int mOrderDriftPlanCount = 0;
    int mLiteralMismatchPlanCount = 0;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingNamingDriftPlanCount = 0;
    int mHeadingIndexedPrefixPlanCount = 0;
    std::vector<std::string> mMissingRequiredDiagnostics;
    std::vector<std::string> mOrderDriftDiagnostics;
    std::vector<std::string> mLiteralMismatchDiagnostics;
    std::vector<std::string> mHeadingNamingDiagnostics;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct BlankSectionsResult
{
    int mPlanCount = 0;
    int mReadFailureCount = 0;
    int mBlankSectionPlanCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct CrossStatusResult
{
    int mTopicCount = 0;
    int mMismatchCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct PlaybookSchemaResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mMissingSectionPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct LinkIntegrityResult
{
    int mDocCount = 0;
    int mBrokenLinkCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TaxonomyJobCompletenessResult
{
    int mPlaybookCount = 0;
    int mIncompleteJobCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TaxonomyTaskTraceabilityResult
{
    int mPlaybookCount = 0;
    int mUntraceableTaskCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct ValidationHeadingOwnershipResult
{
    int mPlanViolationCount = 0;
    int mImplViolationCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TestingActorCoverageResult
{
    int mPlaybookCount = 0;
    int mMissingActorPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct HeadingAliasResult
{
    int mDocCount = 0;
    int mAliasDocCount = 0;
    int mAliasHeadingCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct ImplSchemaValidationResult
{
    int mImplCount = 0;
    int mReadFailureCount = 0;
    int mMissingRequiredImplCount = 0;
    std::vector<std::string> mMissingRequiredDiagnostics;
    int mOrderDriftImplCount = 0;
    std::vector<std::string> mOrderDriftDiagnostics;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingNamingDriftImplCount = 0;
    std::vector<std::string> mHeadingNamingDiagnostics;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingIndexedPrefixImplCount = 0;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct PlaybookOrderResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mOrderDriftPlaybookCount = 0;
    std::vector<std::string> mOrderDriftDiagnostics;
};

struct PlaybookHeadingNamingResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingNamingDriftPlaybookCount = 0;
    std::vector<std::string> mHeadingNamingDiagnostics;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingIndexedPrefixPlaybookCount = 0;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct PlaybookBlankSectionsResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mBlankSectionPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

} // namespace UniPlan
