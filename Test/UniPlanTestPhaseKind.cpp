#include "UniPlanPhaseKind.h"
#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

#include <set>
#include <string>

// ===================================================================
// Pure predicate unit tests for UniPlanPhaseKind. These do not need
// the FBundleTestFixture harness because they operate on in-memory
// FPhaseRecord structs directly — no JSON I/O, no temp dirs.
// ===================================================================

namespace
{

UniPlan::FPhaseRecord MakeBarePhase()
{
    UniPlan::FPhaseRecord Phase;
    Phase.mScope = "Test phase";
    return Phase;
}

UniPlan::FPhaseRecord MakeCodeBearingPhase()
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    Phase.mDesign.mCodeEntityContract = "struct FWidget { int mValue; };";
    Phase.mDesign.mCodeSnippets = "FWidget W; W.mValue = 1;";
    Phase.mDesign.mInvestigation = "Investigated widget ownership";
    Phase.mDesign.mBestPractices = "Keep mValue trivially copyable";
    Phase.mDesign.mMultiPlatforming = "Platform-neutral POD; parity trivial";
    UniPlan::FTestingRecord T;
    T.mActor = UniPlan::ETestingActor::Human;
    T.mStep = "Read widget header";
    T.mAction = "cat FWidget.h";
    T.mExpected = "Compiles standalone";
    Phase.mTesting.push_back(std::move(T));
    return Phase;
}

UniPlan::FPhaseRecord MakeGovernancePhase()
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    // Governance phases explicitly opt out of file-manifest gates via
    // the dedicated bool + required reason string. They typically
    // carry investigation + best_practices prose but produce no code.
    Phase.mbNoFileManifest = true;
    Phase.mFileManifestSkipReason =
        "Governance phase: topic bundle authoring only; no source files";
    Phase.mDesign.mInvestigation = "Governance rationale";
    Phase.mDesign.mBestPractices = "Governance principles";
    UniPlan::FTestingRecord T;
    T.mActor = UniPlan::ETestingActor::Human;
    T.mStep = "Human review of bundle JSON";
    T.mAction = "uni-plan topic get --topic T";
    T.mExpected = "Fields populated per CEC";
    Phase.mTesting.push_back(std::move(T));
    return Phase;
}

} // namespace

// ===================================================================
// Phase-kind predicates
// ===================================================================

TEST(PhaseKind, IsCodeBearingPhaseTrueWhenCECPopulated)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    Phase.mDesign.mCodeEntityContract = "struct FX;";
    EXPECT_TRUE(UniPlan::IsCodeBearingPhase(Phase));
}

TEST(PhaseKind, IsCodeBearingPhaseTrueWhenSnippetsPopulated)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    Phase.mDesign.mCodeSnippets = "int x = 1;";
    EXPECT_TRUE(UniPlan::IsCodeBearingPhase(Phase));
}

TEST(PhaseKind, IsCodeBearingPhaseFalseWhenBothEmpty)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    EXPECT_FALSE(UniPlan::IsCodeBearingPhase(Phase));
}

TEST(PhaseKind, IsGovernancePhaseTrueWhenBoolSet)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    Phase.mbNoFileManifest = true;
    Phase.mFileManifestSkipReason = "doc phase";
    EXPECT_TRUE(UniPlan::IsGovernancePhase(Phase));
}

TEST(PhaseKind, IsGovernancePhaseFalseByDefault)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    EXPECT_FALSE(UniPlan::IsGovernancePhase(Phase));
}

// ===================================================================
// Tri-state status helper
// ===================================================================

TEST(PhaseKind, ReadinessGateStatusToStringCoversEveryValue)
{
    EXPECT_STREQ("pass",
                 UniPlan::ToString(UniPlan::EReadinessGateStatus::Pass));
    EXPECT_STREQ("fail",
                 UniPlan::ToString(UniPlan::EReadinessGateStatus::Fail));
    EXPECT_STREQ(
        "not_applicable",
        UniPlan::ToString(UniPlan::EReadinessGateStatus::NotApplicable));
}

// ===================================================================
// Gate evaluation semantics
// ===================================================================

TEST(PhaseKind, GateReportsPassWhenApplicableAndContentPopulated)
{
    const auto &Gates = UniPlan::GetPhaseReadinessGates();
    UniPlan::FPhaseRecord Phase = MakeCodeBearingPhase();
    for (const auto &Gate : Gates)
    {
        EXPECT_EQ(UniPlan::EReadinessGateStatus::Pass, Gate.Evaluate(Phase))
            << "gate: " << Gate.mName;
    }
}

TEST(PhaseKind, GateReportsFailWhenApplicableAndContentEmpty)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    // Every gate that applies to a bare phase should Fail.
    for (const auto &Gate : UniPlan::GetPhaseReadinessGates())
    {
        const UniPlan::EReadinessGateStatus Status = Gate.Evaluate(Phase);
        EXPECT_EQ(UniPlan::EReadinessGateStatus::Fail, Status)
            << "gate: " << Gate.mName;
    }
}

TEST(PhaseKind, GateReportsNotApplicableForGovernancePhaseCodeGates)
{
    UniPlan::FPhaseRecord Phase = MakeGovernancePhase();
    std::set<std::string> NAExpected = {
        "code_entity_contract", "code_snippets", "multi_platforming"};
    std::set<std::string> PassExpected = {"investigation", "best_practices",
                                          "testing"};
    for (const auto &Gate : UniPlan::GetPhaseReadinessGates())
    {
        const UniPlan::EReadinessGateStatus Status = Gate.Evaluate(Phase);
        if (NAExpected.count(Gate.mName))
        {
            EXPECT_EQ(UniPlan::EReadinessGateStatus::NotApplicable, Status)
                << "gate: " << Gate.mName;
        }
        else if (PassExpected.count(Gate.mName))
        {
            EXPECT_EQ(UniPlan::EReadinessGateStatus::Pass, Status)
                << "gate: " << Gate.mName;
        }
    }
}

// ===================================================================
// Aggregate readiness
// ===================================================================

TEST(PhaseKind, AllReadinessGatesSatisfiedTrueForFullyPopulatedCodePhase)
{
    UniPlan::FPhaseRecord Phase = MakeCodeBearingPhase();
    EXPECT_TRUE(UniPlan::AllReadinessGatesSatisfied(Phase));
}

TEST(PhaseKind, AllReadinessGatesSatisfiedTrueForGovernancePhaseWithBaseFields)
{
    // Governance phases pass aggregate readiness when they satisfy the
    // gates that APPLY to them (investigation + best_practices +
    // testing). The code-bearing gates report NotApplicable, which
    // does not block the aggregate.
    UniPlan::FPhaseRecord Phase = MakeGovernancePhase();
    EXPECT_TRUE(UniPlan::AllReadinessGatesSatisfied(Phase));
}

TEST(PhaseKind, AllReadinessGatesSatisfiedFalseForBarePhase)
{
    UniPlan::FPhaseRecord Phase = MakeBarePhase();
    EXPECT_FALSE(UniPlan::AllReadinessGatesSatisfied(Phase));
}

TEST(PhaseKind, AllReadinessGatesSatisfiedFalseWhenGovernancePhaseMissingInvestigation)
{
    // Sanity: the applicability opt-out covers code-bearing gates
    // only. Governance phases must still pass investigation +
    // best_practices + testing.
    UniPlan::FPhaseRecord Phase = MakeGovernancePhase();
    Phase.mDesign.mInvestigation.clear();
    EXPECT_FALSE(UniPlan::AllReadinessGatesSatisfied(Phase));
}

// ===================================================================
// Drift guard: the registry is the single source of truth for
// readiness gates. If a new gate is added, this test prevents a
// duplicate-name or stale-predicate regression. If a gate is removed,
// the test still passes (the check is forward-compatible), but
// downstream tests that reference removed names will fail loudly.
// ===================================================================

TEST(PhaseKind, ReadinessGateRegistryNamesAreUniqueAndNonEmpty)
{
    const auto &Gates = UniPlan::GetPhaseReadinessGates();
    ASSERT_FALSE(Gates.empty()) << "registry is empty";
    std::set<std::string> SeenNames;
    for (const auto &Gate : Gates)
    {
        ASSERT_NE(nullptr, Gate.mName) << "gate name is null";
        ASSERT_NE(nullptr, Gate.mPassCheck)
            << "gate " << Gate.mName << " missing pass predicate";
        ASSERT_NE(nullptr, Gate.mAppliesCheck)
            << "gate " << Gate.mName << " missing applicability predicate";
        const std::string Name = Gate.mName;
        ASSERT_FALSE(Name.empty()) << "gate name is empty string";
        EXPECT_TRUE(SeenNames.insert(Name).second)
            << "duplicate gate name: " << Name;
    }
}

TEST(PhaseKind, ReadinessGateRegistryCoversExpectedDesignFields)
{
    // Drift guard: the registry's gate names are the public contract
    // for the `phase readiness` response shape + the readiness_gate
    // documentation in .claude/skills/fie-plan-audit. If a field is
    // renamed or removed from the registry, this test flags the
    // divergence so downstream docs stay in sync. Expected set pinned
    // at v0.96.0.
    const std::set<std::string> kExpectedGateNames = {
        "investigation",      "code_entity_contract", "code_snippets",
        "best_practices",     "multi_platforming",    "testing"};
    std::set<std::string> SeenGateNames;
    for (const auto &Gate : UniPlan::GetPhaseReadinessGates())
    {
        SeenGateNames.insert(Gate.mName);
    }
    EXPECT_EQ(kExpectedGateNames, SeenGateNames);
}

// ===================================================================
// End-to-end: drive phase readiness CLI through the registry and
// verify every status value appears correctly on a governance phase.
// This uses the FBundleTestFixture harness so we exercise JSON I/O
// + opt-out deserialization + emitter end-to-end.
// ===================================================================

TEST_F(FBundleTestFixture, PhaseReadinessGovernancePhaseReportsNotApplicable)
{
    CreateMinimalFixture("GovT", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted,
                         /*InPopulateDesign=*/false);
    // Load + decorate the phase with governance opt-out + base-field
    // content so applicable gates pass.
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("GovT", Bundle));
    UniPlan::FPhaseRecord &Phase = Bundle.mPhases[0];
    Phase.mbNoFileManifest = true;
    Phase.mFileManifestSkipReason =
        "Governance phase: topic bundle authoring only";
    Phase.mDesign.mInvestigation = "Governance investigation";
    Phase.mDesign.mBestPractices = "Governance best practices";
    UniPlan::FTestingRecord T;
    T.mActor = UniPlan::ETestingActor::Human;
    T.mStep = "Read bundle JSON";
    T.mAction = "uni-plan topic get --topic GovT";
    T.mExpected = "Fields populated per spec";
    Phase.mTesting.push_back(std::move(T));
    std::string WriteError;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(
        Bundle, mRepoRoot / "Docs" / "Plans" / "GovT.Plan.json", WriteError))
        << WriteError;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"readiness", "--topic", "GovT", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ready"].get<bool>());
    ASSERT_TRUE(Json.contains("gates"));
    ASSERT_EQ(6u, Json["gates"].size());
    const std::set<std::string> kExpectedNA = {
        "code_entity_contract", "code_snippets", "multi_platforming"};
    const std::set<std::string> kExpectedPass = {"investigation",
                                                 "best_practices", "testing"};
    for (const auto &GateJson : Json["gates"])
    {
        const std::string Name = GateJson["name"].get<std::string>();
        const std::string Status = GateJson["status"].get<std::string>();
        if (kExpectedNA.count(Name))
        {
            EXPECT_EQ("not_applicable", Status) << "gate " << Name;
        }
        else if (kExpectedPass.count(Name))
        {
            EXPECT_EQ("pass", Status) << "gate " << Name;
        }
    }
}

// ===================================================================
// phase get exposes the opt-out signal in every mode.
// ===================================================================

TEST_F(FBundleTestFixture, PhaseGetEmitsNoFileManifestFieldsWhenOptedOut)
{
    CreateMinimalFixture("OptOutT", UniPlan::ETopicStatus::InProgress, 1);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("OptOutT", Bundle));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "doc-only phase";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(
        Bundle, mRepoRoot / "Docs" / "Plans" / "OptOutT.Plan.json", Error))
        << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "OptOutT", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("no_file_manifest"));
    EXPECT_TRUE(Json["no_file_manifest"].get<bool>());
    EXPECT_TRUE(Json.contains("file_manifest_skip_reason"));
    EXPECT_EQ("doc-only phase",
              Json["file_manifest_skip_reason"].get<std::string>());
}

TEST_F(FBundleTestFixture, PhaseGetEmitsNoFileManifestFieldsWhenNotOptedOut)
{
    // The field is always emitted (not conditionally) so auditors can
    // distinguish "phase has not opted out" from "phase get tooling
    // missing the field" — the previous failure mode under v0.95.
    CreateMinimalFixture("NoOptOutT", UniPlan::ETopicStatus::InProgress, 1);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "NoOptOutT", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("no_file_manifest"));
    EXPECT_FALSE(Json["no_file_manifest"].get<bool>());
    EXPECT_TRUE(Json.contains("file_manifest_skip_reason"));
    EXPECT_TRUE(Json["file_manifest_skip_reason"].is_null());
}

// ===================================================================
// phase next uses the shared registry — governance phases should
// report ready=True without stuffing code-bearing gates into
// missing_fields.
// ===================================================================

TEST_F(FBundleTestFixture, PhaseNextTreatsGovernancePhaseAsReady)
{
    CreateMinimalFixture("GovNextT", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("GovNextT", Bundle));
    UniPlan::FPhaseRecord &Phase = Bundle.mPhases[0];
    Phase.mbNoFileManifest = true;
    Phase.mFileManifestSkipReason = "governance";
    Phase.mDesign.mInvestigation = "I";
    Phase.mDesign.mBestPractices = "B";
    UniPlan::FTestingRecord T;
    T.mActor = UniPlan::ETestingActor::Human;
    T.mStep = "S";
    T.mAction = "A";
    T.mExpected = "E";
    Phase.mTesting.push_back(std::move(T));
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(
        Bundle, mRepoRoot / "Docs" / "Plans" / "GovNextT.Plan.json", Error))
        << Error;

    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"next", "--topic", "GovNextT", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(0, Json["phase_index"].get<int>());
    EXPECT_TRUE(Json["ready"].get<bool>());
    EXPECT_EQ(0u, Json["missing_fields"].size());
}
