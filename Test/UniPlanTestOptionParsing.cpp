#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

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

TEST(OptionParsing, PhaseMetricParsesSelectorsAndStatus)
{
    const auto Opts = UniPlan::ParsePhaseMetricOptions(
        {"--topic", "X", "--phases", "2,0,2", "--status", "in_progress",
         "--human"});
    EXPECT_EQ(Opts.mTopic, "X");
    ASSERT_EQ(Opts.mPhaseIndices.size(), 2);
    EXPECT_EQ(Opts.mPhaseIndices[0], 0);
    EXPECT_EQ(Opts.mPhaseIndices[1], 2);
    EXPECT_EQ(Opts.mStatus, "in_progress");
    EXPECT_TRUE(Opts.mbHuman);
}

TEST(OptionParsing, PhaseMetricRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParsePhaseMetricOptions({"--phase", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseMetricRejectsSelectorConflict)
{
    EXPECT_THROW(UniPlan::ParsePhaseMetricOptions(
                     {"--topic", "X", "--phase", "0", "--phases", "1"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, PhaseMetricRejectsUnsupportedStatus)
{
    EXPECT_THROW(
        UniPlan::ParsePhaseMetricOptions({"--topic", "X", "--status", "done"}),
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

// ===================================================================
// --<field>-file flag family (v0.76.0+) — hazard-closure regression
// for shell-metachar-bearing prose fields. Verifies that content with
// `$VAR`, backticks, and double quotes round-trips byte-identically
// through the file-read path (no shell expansion, no interpretation).
// ===================================================================

namespace
{
std::string WriteTempProseFile(const std::string &InContents)
{
    const std::filesystem::path Dir =
        std::filesystem::temp_directory_path() / "uni-plan-test-prose";
    std::filesystem::create_directories(Dir);
    const std::filesystem::path Path =
        Dir / ("prose-" + std::to_string(std::rand()) + ".txt");
    std::ofstream Out(Path, std::ios::binary);
    Out << InContents;
    Out.close();
    return Path.string();
}
} // namespace

TEST(OptionParsing, PhaseSetInvestigationFilePreservesShellMetachars)
{
    // The exact content the hazard would corrupt on the
    // `--investigation "$(cat file)"` path. `-file` must round-trip it
    // byte-identically because the CLI reads the file itself — bash
    // never sees the content inside double quotes.
    const std::string Hazardous =
        "Reference `$PATH` variable and use $(pwd) with \"quoted\" text.";
    const std::string Path = WriteTempProseFile(Hazardous);
    const auto O = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--investigation-file", Path});
    EXPECT_EQ(O.mInvestigation, Hazardous);
}

TEST(OptionParsing, PhaseSetInvestigationFileMissingPathThrows)
{
    EXPECT_THROW(UniPlan::ParsePhaseSetOptions({"--topic", "X", "--phase", "0",
                                                "--investigation-file",
                                                "/no/such/file-xyz.txt"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TopicSetSummaryFilePreservesNewlines)
{
    // Multi-line prose with a backtick + embedded newline. Round-trip
    // through the file path must preserve every byte including the
    // trailing LF.
    const std::string Multiline = "line one `code` token\nline two ends\n";
    const std::string Path = WriteTempProseFile(Multiline);
    const auto O =
        UniPlan::ParseTopicSetOptions({"--topic", "X", "--summary-file", Path});
    EXPECT_EQ(O.mSummary, Multiline);
}

TEST(OptionParsing, PhaseSetInvestigationFlagAndFileAreInterchangeable)
{
    // Byte-identical content via --investigation vs --investigation-file
    // must produce the same stored value.
    const std::string Content = "plain prose with no metachars.";
    const std::string Path = WriteTempProseFile(Content);
    const auto Inline = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--investigation", Content});
    const auto ViaFile = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--investigation-file", Path});
    EXPECT_EQ(Inline.mInvestigation, ViaFile.mInvestigation);
}

// ===================================================================
// v0.100.0 — JSON-file setters for typed-array inputs
//
// The pipe grammar (`<platform>|<command>|<description>`) used by
// --validation-commands / --validation-add / --dependency-add is
// fundamentally shell-hostile: a command containing literal `|` (common
// in bash pipelines) is silently split. The JSON-file flags sidestep
// the pipe grammar entirely — all content is inert JSON bytes.
// ===================================================================

TEST(OptionParsing, PhaseSetValidationCommandsJsonFilePreservesPipeInCommand)
{
    // A command containing literal `|` would be silently mangled by
    // --validation-add / --validation-commands. Via the JSON-file path,
    // every byte including the pipe must survive round-trip.
    const std::string JsonContent = R"([
        {"platform":"macos","command":"grep foo bar.log | wc -l","description":"count foos"},
        {"platform":"any","command":"echo `pwd` && ls $HOME","description":"shell-hostile"}
    ])";
    const std::string Path = WriteTempProseFile(JsonContent);
    const auto O = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--validation-commands-json-file",
         Path});
    EXPECT_TRUE(O.mbValidationClear);
    ASSERT_EQ(O.mValidationAdd.size(), 2u);
    EXPECT_EQ(O.mValidationAdd[0].mPlatform, UniPlan::EPlatformScope::MacOS);
    EXPECT_EQ(O.mValidationAdd[0].mCommand, "grep foo bar.log | wc -l");
    EXPECT_EQ(O.mValidationAdd[0].mDescription, "count foos");
    EXPECT_EQ(O.mValidationAdd[1].mPlatform, UniPlan::EPlatformScope::Any);
    EXPECT_EQ(O.mValidationAdd[1].mCommand, "echo `pwd` && ls $HOME");
    EXPECT_EQ(O.mValidationAdd[1].mDescription, "shell-hostile");
}

TEST(OptionParsing, TopicSetValidationCommandsJsonFileHasReplaceSemantics)
{
    // --validation-commands-json-file mirrors --validation-commands:
    // the first use sets mbValidationClear=true (REPLACE).
    const std::string JsonContent = R"([
        {"platform":"linux","command":"make test","description":"CI"}
    ])";
    const std::string Path = WriteTempProseFile(JsonContent);
    const auto O = UniPlan::ParseTopicSetOptions(
        {"--topic", "X", "--validation-commands-json-file", Path});
    EXPECT_TRUE(O.mbValidationClear);
    ASSERT_EQ(O.mValidationAdd.size(), 1u);
    EXPECT_EQ(O.mValidationAdd[0].mCommand, "make test");
}

TEST(OptionParsing, PhaseSetValidationAddJsonFileHasAppendSemantics)
{
    // --validation-add-json-file appends, does NOT set mbValidationClear.
    const std::string JsonContent = R"([
        {"platform":"windows","command":"Tests\\smoke.bat","description":""}
    ])";
    const std::string Path = WriteTempProseFile(JsonContent);
    const auto O = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--validation-add-json-file", Path});
    EXPECT_FALSE(O.mbValidationClear);
    ASSERT_EQ(O.mValidationAdd.size(), 1u);
    EXPECT_EQ(O.mValidationAdd[0].mPlatform, UniPlan::EPlatformScope::Windows);
    EXPECT_EQ(O.mValidationAdd[0].mCommand, "Tests\\smoke.bat");
}

TEST(OptionParsing, PhaseSetDependencyAddJsonFileHandlesAllKinds)
{
    // Every dependency kind: bundle/phase/governance/external. The `note`
    // field can contain a literal `|` — impossible via --dependency-add.
    const std::string JsonContent = R"([
        {"kind":"bundle","topic":"Foo"},
        {"kind":"phase","topic":"Bar","phase":3},
        {"kind":"governance","path":"CODING.md","note":"a | b | c"},
        {"kind":"external","path":"https://example.com/?a=1|2","note":""}
    ])";
    const std::string Path = WriteTempProseFile(JsonContent);
    const auto O = UniPlan::ParsePhaseSetOptions(
        {"--topic", "X", "--phase", "0", "--dependency-add-json-file", Path});
    ASSERT_EQ(O.mDependencyAdd.size(), 4u);
    EXPECT_EQ(O.mDependencyAdd[0].mKind, UniPlan::EDependencyKind::Bundle);
    EXPECT_EQ(O.mDependencyAdd[0].mTopic, "Foo");
    EXPECT_EQ(O.mDependencyAdd[1].mKind, UniPlan::EDependencyKind::Phase);
    EXPECT_EQ(O.mDependencyAdd[1].mPhase, 3);
    EXPECT_EQ(O.mDependencyAdd[2].mKind, UniPlan::EDependencyKind::Governance);
    EXPECT_EQ(O.mDependencyAdd[2].mPath, "CODING.md");
    EXPECT_EQ(O.mDependencyAdd[2].mNote, "a | b | c");
    EXPECT_EQ(O.mDependencyAdd[3].mKind, UniPlan::EDependencyKind::External);
    EXPECT_EQ(O.mDependencyAdd[3].mPath, "https://example.com/?a=1|2");
}

TEST(OptionParsing, ValidationCommandsJsonFileMalformedThrows)
{
    // JSON parse error surfaces as UsageError (exit 2).
    const std::string Path = WriteTempProseFile("not json at all");
    EXPECT_THROW(UniPlan::ParseTopicSetOptions(
                     {"--topic", "X", "--validation-commands-json-file", Path}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, ValidationCommandsJsonFileEmptyCommandThrows)
{
    // Empty command is a UsageError — mirrors --validation-add's behavior.
    const std::string Path = WriteTempProseFile(
        R"([{"platform":"any","command":"","description":"nope"}])");
    EXPECT_THROW(UniPlan::ParseTopicSetOptions(
                     {"--topic", "X", "--validation-commands-json-file", Path}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, DependencyAddJsonFileBundleWithoutTopicThrows)
{
    // kind=bundle without a topic is a UsageError.
    const std::string Path = WriteTempProseFile(R"([{"kind":"bundle"}])");
    EXPECT_THROW(
        UniPlan::ParsePhaseSetOptions({"--topic", "X", "--phase", "0",
                                       "--dependency-add-json-file", Path}),
        UniPlan::UsageError);
}

TEST(OptionParsing, DependencyAddJsonFileTopLevelMustBeArray)
{
    // Top-level object (not array) must fail fast.
    const std::string Path =
        WriteTempProseFile(R"({"kind":"bundle","topic":"X"})");
    EXPECT_THROW(
        UniPlan::ParsePhaseSetOptions({"--topic", "X", "--phase", "0",
                                       "--dependency-add-json-file", Path}),
        UniPlan::UsageError);
}
