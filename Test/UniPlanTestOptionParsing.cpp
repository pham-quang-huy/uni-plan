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
    ASSERT_TRUE(Opts.opStatus.has_value());
    EXPECT_EQ(*Opts.opStatus, UniPlan::ETopicStatus::Blocked);
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
    ASSERT_TRUE(Opts.opStatus.has_value());
    EXPECT_EQ(*Opts.opStatus, UniPlan::EExecutionStatus::Completed);
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

TEST(OptionParsing, LaneSetRequiresLane)
{
    EXPECT_THROW(UniPlan::ParseLaneSetOptions({"--topic", "X", "--phase", "0"}),
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
    ASSERT_TRUE(Opts.opAction.has_value());
    EXPECT_EQ(*Opts.opAction, UniPlan::EFileAction::Create);
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

TEST(OptionParsing, TestingSetRequiresIndex)
{
    EXPECT_THROW(
        UniPlan::ParseTestingSetOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, TestingSetHappyPath)
{
    const auto O = UniPlan::ParseTestingSetOptions(
        {"--topic", "X", "--phase", "1", "--index", "2", "--step", "s"});
    EXPECT_EQ(O.mTopic, "X");
    EXPECT_EQ(O.mPhaseIndex, 1);
    EXPECT_EQ(O.mIndex, 2);
    EXPECT_EQ(O.mStep, "s");
}

TEST(OptionParsing, VerificationSetRequiresIndex)
{
    EXPECT_THROW(UniPlan::ParseVerificationSetOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, ManifestSetRequiresIndex)
{
    EXPECT_THROW(
        UniPlan::ParseManifestSetOptions({"--topic", "X", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, ChangelogSetRequiresIndex)
{
    EXPECT_THROW(UniPlan::ParseChangelogSetOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, ChangelogSetHappyPath)
{
    const auto O = UniPlan::ParseChangelogSetOptions(
        {"--topic", "X", "--index", "2", "--phase", "topic", "--change",
         "Updated", "--type", "fix", "--affected", "phases[2]"});
    EXPECT_EQ(O.mTopic, "X");
    EXPECT_EQ(O.mIndex, 2);
    EXPECT_EQ(O.mPhase, -1);
    EXPECT_EQ(O.mChange, "Updated");
    ASSERT_TRUE(O.opType.has_value());
    EXPECT_EQ(*O.opType, UniPlan::EChangeType::Fix);
    EXPECT_EQ(O.mAffected, "phases[2]");
}

TEST(OptionParsing, LaneAddRequiresPhase)
{
    EXPECT_THROW(UniPlan::ParseLaneAddOptions({"--topic", "X"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, LaneAddHappyPath)
{
    const auto O = UniPlan::ParseLaneAddOptions(
        {"--topic", "X", "--phase", "0", "--scope", "S"});
    EXPECT_EQ(O.mTopic, "X");
    EXPECT_EQ(O.mPhaseIndex, 0);
    EXPECT_EQ(O.mScope, "S");
}

TEST(OptionParsing, UnknownOptionThrows)
{
    EXPECT_THROW(
        UniPlan::ParseTopicSetOptions({"--topic", "X", "--bogus", "val"}),
        UniPlan::UsageError);
}

// ===================================================================
// Legacy-gap parser (stateless V3 <-> V4 parity audit, 0.75.0+)
// ===================================================================

TEST(OptionParsing, LegacyGapEmptyIsAllTopicsAllCategories)
{
    const auto O = UniPlan::ParseLegacyGapOptions({});
    EXPECT_TRUE(O.mTopic.empty());
    EXPECT_FALSE(O.opCategory.has_value());
}

TEST(OptionParsing, LegacyGapTopicFilter)
{
    const auto O = UniPlan::ParseLegacyGapOptions({"--topic", "CycleRefactor"});
    EXPECT_EQ(O.mTopic, "CycleRefactor");
}

TEST(OptionParsing, LegacyGapCategoryFilter)
{
    const auto O =
        UniPlan::ParseLegacyGapOptions({"--category", "legacy_rich"});
    ASSERT_TRUE(O.opCategory.has_value());
    EXPECT_EQ(*O.opCategory, UniPlan::EPhaseGapCategory::LegacyRich);
}

TEST(OptionParsing, LegacyGapInvalidCategoryThrows)
{
    EXPECT_THROW(UniPlan::ParseLegacyGapOptions({"--category", "bogus"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, LegacyGapUnknownOptionThrows)
{
    EXPECT_THROW(UniPlan::ParseLegacyGapOptions({"--bogus", "val"}),
                 UniPlan::UsageError);
}

// ===================================================================
// Enum round-trips
// ===================================================================

TEST(PhaseEnums, PhaseOriginRoundTrip)
{
    const UniPlan::EPhaseOrigin Origins[] = {
        UniPlan::EPhaseOrigin::NativeV4,
        UniPlan::EPhaseOrigin::V3Migration,
    };
    for (const UniPlan::EPhaseOrigin O : Origins)
    {
        const std::string Str = UniPlan::ToString(O);
        UniPlan::EPhaseOrigin Parsed = UniPlan::EPhaseOrigin::NativeV4;
        ASSERT_TRUE(UniPlan::PhaseOriginFromString(Str, Parsed))
            << "round-trip failed for " << Str;
        EXPECT_EQ(Parsed, O) << "mismatch on " << Str;
    }
}

TEST(PhaseEnums, PhaseOriginEmptyStringIsNativeV4)
{
    // Empty-string path is the migration-friendly default used when a bundle
    // has no `origin` key at all.
    UniPlan::EPhaseOrigin Out = UniPlan::EPhaseOrigin::V3Migration;
    ASSERT_TRUE(UniPlan::PhaseOriginFromString("", Out));
    EXPECT_EQ(Out, UniPlan::EPhaseOrigin::NativeV4);
}

TEST(PhaseEnums, PhaseOriginRejectsInvalid)
{
    UniPlan::EPhaseOrigin Out = UniPlan::EPhaseOrigin::NativeV4;
    EXPECT_FALSE(UniPlan::PhaseOriginFromString("not_a_value", Out));
    EXPECT_FALSE(UniPlan::PhaseOriginFromString("V3Migration", Out));
}

TEST(PhaseEnums, PhaseGapCategoryRoundTrip)
{
    const UniPlan::EPhaseGapCategory Cats[] = {
        UniPlan::EPhaseGapCategory::LegacyRich,
        UniPlan::EPhaseGapCategory::LegacyRichMatched,
        UniPlan::EPhaseGapCategory::LegacyThin,
        UniPlan::EPhaseGapCategory::LegacyStub,
        UniPlan::EPhaseGapCategory::LegacyAbsent,
        UniPlan::EPhaseGapCategory::V4Only,
        UniPlan::EPhaseGapCategory::HollowBoth,
        UniPlan::EPhaseGapCategory::Drift,
    };
    for (const UniPlan::EPhaseGapCategory C : Cats)
    {
        const std::string Str = UniPlan::ToString(C);
        UniPlan::EPhaseGapCategory Parsed =
            UniPlan::EPhaseGapCategory::LegacyAbsent;
        ASSERT_TRUE(UniPlan::PhaseGapCategoryFromString(Str, Parsed))
            << "round-trip failed for " << Str;
        EXPECT_EQ(Parsed, C) << "mismatch on " << Str;
    }
}

TEST(PhaseEnums, PhaseGapCategoryFromStringRejectsInvalid)
{
    UniPlan::EPhaseGapCategory Out = UniPlan::EPhaseGapCategory::LegacyAbsent;
    EXPECT_FALSE(UniPlan::PhaseGapCategoryFromString("not_a_cat", Out));
    EXPECT_FALSE(UniPlan::PhaseGapCategoryFromString("", Out));
}
