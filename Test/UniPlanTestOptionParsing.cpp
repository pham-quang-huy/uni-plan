#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// Topic option parsers
// ===================================================================

TEST(OptionParsing, TopicSetRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParseTopicSetOptions({"--status", "done"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TopicSetHappyPath)
{
    const auto Opts = UniPlan::ParseTopicSetOptions(
        {"--topic", "Foo", "--status", "blocked"});
    EXPECT_EQ(Opts.mTopic, "Foo");
    EXPECT_EQ(Opts.mStatus, "blocked");
}

TEST(OptionParsing, TopicStartRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParseTopicStartOptions({}), UniPlan::UsageError);
}

TEST(OptionParsing, TopicCompleteRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParseTopicCompleteOptions({}), UniPlan::UsageError);
}

TEST(OptionParsing, TopicBlockRequiresReason)
{
    EXPECT_THROW(UniPlan::ParseTopicBlockOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

// ===================================================================
// Phase option parsers
// ===================================================================

TEST(OptionParsing, PhaseSetRequiresTopicAndPhase)
{
    EXPECT_THROW(UniPlan::ParsePhaseSetOptions({"--topic", "X"}),
                 UniPlan::UsageError);
    EXPECT_THROW(UniPlan::ParsePhaseSetOptions({"--phase", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseSetHappyPath)
{
    const auto Opts = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "3", "--status", "completed", "--done",
         "All done"});
    EXPECT_EQ(Opts.mTopic, "X");
    EXPECT_EQ(Opts.mPhaseIndex, 3);
    EXPECT_EQ(Opts.mStatus, "completed");
    EXPECT_EQ(Opts.mDone, "All done");
}

TEST(OptionParsing, PhaseStartRequiresTopicAndPhase)
{
    EXPECT_THROW(UniPlan::ParsePhaseStartOptions({"--topic", "X"}),
                 UniPlan::UsageError);
    EXPECT_THROW(UniPlan::ParsePhaseStartOptions({"--phase", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseStartHappyPath)
{
    const auto Opts = UniPlan::ParsePhaseStartOptions(
        {"--topic", "Foo", "--phase", "2", "--context", "ctx"});
    EXPECT_EQ(Opts.mTopic, "Foo");
    EXPECT_EQ(Opts.mPhaseIndex, 2);
    EXPECT_EQ(Opts.mContext, "ctx");
}

TEST(OptionParsing, PhaseCompleteRequiresDone)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseCompleteOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, PhaseBlockRequiresReason)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseBlockOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, PhaseUnblockHappyPath)
{
    const auto Opts =
        UniPlan::ParsePhaseUnblockOptions({"--topic", "X", "--phase", "5"});
    EXPECT_EQ(Opts.mTopic, "X");
    EXPECT_EQ(Opts.mPhaseIndex, 5);
}

TEST(OptionParsing, PhaseProgressRequiresBothFields)
{
    EXPECT_THROW(UniPlan::ParsePhaseProgressOptions(
                     {"--topic", "X", "--phase", "0", "--done", "WIP"}),
                 UniPlan::UsageError);
    EXPECT_THROW(UniPlan::ParsePhaseProgressOptions(
                     {"--topic", "X", "--phase", "0", "--remaining", "More"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseCompleteJobsHappyPath)
{
    const auto Opts = UniPlan::ParsePhaseCompleteJobsOptions(
        {"--topic", "X", "--phase", "1"});
    EXPECT_EQ(Opts.mTopic, "X");
    EXPECT_EQ(Opts.mPhaseIndex, 1);
}

// ===================================================================
// Evidence option parsers
// ===================================================================

TEST(OptionParsing, PhaseLogRequiresPhaseAndChange)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseLogOptions({"--topic", "X", "--change", "test"}),
        UniPlan::UsageError);
    EXPECT_THROW(
        UniPlan::ParsePhaseLogOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, PhaseVerifyRequiresPhaseAndCheck)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseVerifyOptions({"--topic", "X", "--check", "test"}),
        UniPlan::UsageError);
    EXPECT_THROW(
        UniPlan::ParsePhaseVerifyOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, ChangelogAddRequiresChange)
{
    EXPECT_THROW(UniPlan::ParseChangelogAddOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, VerificationAddRequiresCheck)
{
    EXPECT_THROW(UniPlan::ParseVerificationAddOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

// ===================================================================
// Entity option parsers
// ===================================================================

TEST(OptionParsing, LaneSetRequiresAll)
{
    EXPECT_THROW(UniPlan::ParseLaneSetOptions(
                     {"--topic", "X", "--phase", "0", "--lane", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TestingAddRequiresStepActionExpected)
{
    EXPECT_THROW(UniPlan::ParseTestingAddOptions(
                     {"--topic", "X", "--phase", "0", "--step", "s"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, ManifestAddRequiresFileActionDescription)
{
    EXPECT_THROW(UniPlan::ParseManifestAddOptions(
                     {"--topic", "X", "--phase", "0", "--file", "f"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, ManifestAddHappyPath)
{
    const auto Opts = UniPlan::ParseManifestAddOptions(
        {"--topic", "X", "--phase", "1", "--file", "Source/Foo.cpp", "--action",
         "create", "--description", "New file"});
    EXPECT_EQ(Opts.mTopic, "X");
    EXPECT_EQ(Opts.mPhaseIndex, 1);
    EXPECT_EQ(Opts.mFile, "Source/Foo.cpp");
    EXPECT_EQ(Opts.mAction, "create");
    EXPECT_EQ(Opts.mDescription, "New file");
}

// ===================================================================
// Query + misc option parsers
// ===================================================================

TEST(OptionParsing, PhaseQueryRequiresTopicAndPhase)
{
    EXPECT_THROW(UniPlan::ParsePhaseQueryOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, JobSetRequiresAll)
{
    EXPECT_THROW(UniPlan::ParseJobSetOptions({"--topic", "X", "--phase", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TaskSetRequiresAll)
{
    EXPECT_THROW(UniPlan::ParseTaskSetOptions(
                     {"--topic", "X", "--phase", "0", "--job", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, UnknownOptionThrows)
{
    EXPECT_THROW(
        UniPlan::ParseTopicSetOptions({"--topic", "X", "--bogus", "val"}),
        UniPlan::UsageError);
}
