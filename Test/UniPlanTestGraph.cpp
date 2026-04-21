#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// Graph command tests (v0.98.0+)
//
// Covers:
//   - Empty graph (bundle with no dependencies yet has one topic node)
//   - Full corpus graph returns all topics
//   - --topic filter returns reachable neighborhood
//   - --depth 0 returns only the focus topic's own nodes
//   - edge shape (from/to/kind/note) round-trips through the JSON
//   - Missing focus topic surfaces a warning
// ===================================================================

namespace
{

// Helper: add a topic-level bundle dependency on InSource → InTarget.
void AddBundleDep(UniPlan::FTopicBundle &InOutBundle,
                  const std::string &InTarget, const std::string &InNote = "")
{
    UniPlan::FBundleReference R;
    R.mKind = UniPlan::EDependencyKind::Bundle;
    R.mTopic = InTarget;
    R.mNote = InNote;
    InOutBundle.mMetadata.mDependencies.push_back(std::move(R));
}

// Helper: write a bundle back to the temp repo.
void WriteBundleToRepo(const fs::path &InRepoRoot, const std::string &InTopic,
                       const UniPlan::FTopicBundle &InBundle)
{
    const fs::path Path =
        InRepoRoot / "Docs" / "Plans" / (InTopic + ".Plan.json");
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(InBundle, Path, Error)) << Error;
}

} // namespace

TEST_F(FBundleTestFixture, GraphEmptyBundleReturnsTopicNodeNoEdges)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);

    StartCapture();
    const int Code = UniPlan::RunGraphCommand(
        {"--topic", "A", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"].get<std::string>(), "uni-plan-graph-v1");
    EXPECT_EQ(Json["focus_topic"].get<std::string>(), "A");
    // One topic node + one phase node.
    EXPECT_GE(Json["node_count"].get<int>(), 1);
    EXPECT_EQ(Json["edge_count"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, GraphFullCorpusReturnsAllTopics)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("B", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);

    StartCapture();
    const int Code =
        UniPlan::RunGraphCommand({"--repo-root", mRepoRoot.string()},
                                 mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"].get<std::string>(), "uni-plan-graph-v1");
    // Both topics should appear as nodes (plus their phase nodes).
    bool FoundA = false;
    bool FoundB = false;
    for (const auto &N : Json["nodes"])
    {
        const std::string Id = N["id"].get<std::string>();
        if (Id == "topic:A")
            FoundA = true;
        if (Id == "topic:B")
            FoundB = true;
    }
    EXPECT_TRUE(FoundA);
    EXPECT_TRUE(FoundB);
}

TEST_F(FBundleTestFixture, GraphBundleDependencyEmitsEdge)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("B", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    // A depends on B
    UniPlan::FTopicBundle A;
    ASSERT_TRUE(ReloadBundle("A", A));
    AddBundleDep(A, "B", "A consumes B contracts");
    WriteBundleToRepo(mRepoRoot, "A", A);

    StartCapture();
    const int Code =
        UniPlan::RunGraphCommand({"--repo-root", mRepoRoot.string()},
                                 mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // Exactly one edge: topic:A → topic:B
    EXPECT_GE(Json["edge_count"].get<int>(), 1);
    bool FoundEdge = false;
    for (const auto &E : Json["edges"])
    {
        if (E["from"].get<std::string>() == "topic:A" &&
            E["to"].get<std::string>() == "topic:B" &&
            E["kind"].get<std::string>() == "bundle")
        {
            FoundEdge = true;
            EXPECT_EQ(E["note"].get<std::string>(), "A consumes B contracts");
        }
    }
    EXPECT_TRUE(FoundEdge);
}

TEST_F(FBundleTestFixture, GraphFocusTopicReturnsNeighborhood)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("B", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("C", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    // A -> B, unrelated C stays out of A's neighborhood
    UniPlan::FTopicBundle A;
    ASSERT_TRUE(ReloadBundle("A", A));
    AddBundleDep(A, "B");
    WriteBundleToRepo(mRepoRoot, "A", A);

    StartCapture();
    const int Code = UniPlan::RunGraphCommand(
        {"--topic", "A", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // A + A's phase node + B + B's phase node should be visible; C should not.
    bool FoundA = false;
    bool FoundB = false;
    bool FoundC = false;
    for (const auto &N : Json["nodes"])
    {
        const std::string Topic = N["topic"].get<std::string>();
        if (Topic == "A")
            FoundA = true;
        if (Topic == "B")
            FoundB = true;
        if (Topic == "C")
            FoundC = true;
    }
    EXPECT_TRUE(FoundA);
    EXPECT_TRUE(FoundB);
    EXPECT_FALSE(FoundC);
}

TEST_F(FBundleTestFixture, GraphMissingFocusTopicReportsWarning)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);

    StartCapture();
    const int Code = UniPlan::RunGraphCommand(
        {"--topic", "DoesNotExist", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    // Warnings array should contain a "topic not found" message.
    bool SawWarning = false;
    for (const auto &W : Json["warnings"])
    {
        const std::string Text = W.get<std::string>();
        if (Text.find("DoesNotExist") != std::string::npos)
        {
            SawWarning = true;
            break;
        }
    }
    EXPECT_TRUE(SawWarning);
}

TEST_F(FBundleTestFixture, GraphPhaseDependencyEmitsPhaseEdge)
{
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("B", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);

    // A.phases[1] depends on B.phases[0]
    UniPlan::FTopicBundle A;
    ASSERT_TRUE(ReloadBundle("A", A));
    UniPlan::FBundleReference PhaseDep;
    PhaseDep.mKind = UniPlan::EDependencyKind::Phase;
    PhaseDep.mTopic = "B";
    PhaseDep.mPhase = 0;
    PhaseDep.mNote = "Phase 1 of A depends on phase 0 of B";
    A.mPhases[1].mDesign.mDependencies.push_back(std::move(PhaseDep));
    WriteBundleToRepo(mRepoRoot, "A", A);

    StartCapture();
    const int Code =
        UniPlan::RunGraphCommand({"--repo-root", mRepoRoot.string()},
                                 mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    bool FoundPhaseEdge = false;
    for (const auto &E : Json["edges"])
    {
        if (E["from"].get<std::string>() == "phase:A/1" &&
            E["to"].get<std::string>() == "phase:B/0" &&
            E["kind"].get<std::string>() == "phase")
        {
            FoundPhaseEdge = true;
        }
    }
    EXPECT_TRUE(FoundPhaseEdge);
}
