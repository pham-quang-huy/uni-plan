#include "UniPlanCommandHelp.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanJSON.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ===================================================================
// Tests for --help routing (v0.85.0).
//
// Contract under test: every hand-rolled dispatcher and every single-
// command handler must handle `--help` as a success path — exit 0,
// print to stdout, write nothing to stderr. Unknown subcommands must
// still throw UsageError so scripts can distinguish `--help` from
// malformed input.
// ===================================================================

namespace
{

// Drive one command dispatcher with the given args and capture stdout
// + stderr. Returns the int exit code. Throws nothing — if the
// underlying dispatcher throws, we re-raise on the test thread so
// EXPECT_THROW / ASSERT_NO_THROW work unchanged.
struct FCaptureResult
{
    int mExitCode = -1;
    std::string mStdout;
    std::string mStderr;
};

FCaptureResult CaptureDispatcher(
    int (*InDispatcher)(const std::vector<std::string> &, const std::string &),
    const std::vector<std::string> &InArgs)
{
    std::ostringstream Out;
    std::ostringstream Err;
    std::streambuf *rpOldOut = std::cout.rdbuf(Out.rdbuf());
    std::streambuf *rpOldErr = std::cerr.rdbuf(Err.rdbuf());
    FCaptureResult Result;
    try
    {
        Result.mExitCode = InDispatcher(InArgs, ".");
    }
    catch (...)
    {
        std::cout.rdbuf(rpOldOut);
        std::cerr.rdbuf(rpOldErr);
        Result.mStdout = Out.str();
        Result.mStderr = Err.str();
        throw;
    }
    std::cout.rdbuf(rpOldOut);
    std::cerr.rdbuf(rpOldErr);
    Result.mStdout = Out.str();
    Result.mStderr = Err.str();
    return Result;
}

} // namespace

// Every hand-rolled group dispatcher exits 0 on --help, writes to
// stdout, writes nothing to stderr. Pre-v0.85.0 these threw UsageError
// (exit 2, stderr).
TEST(HelpRouting, GroupHelpExitsZeroStdoutOnly)
{
    struct FCase
    {
        const char *mLabel;
        int (*mpDispatcher)(const std::vector<std::string> &,
                            const std::string &);
    };
    const FCase Cases[] = {
        {"topic", &UniPlan::RunTopicCommand},
        {"phase", &UniPlan::RunBundlePhaseCommand},
        {"changelog", &UniPlan::RunBundleChangelogCommand},
        {"verification", &UniPlan::RunBundleVerificationCommand},
    };
    for (const FCase &C : Cases)
    {
        const auto Result = CaptureDispatcher(C.mpDispatcher, {"--help"});
        EXPECT_EQ(Result.mExitCode, 0) << "group: " << C.mLabel;
        EXPECT_FALSE(Result.mStdout.empty()) << "group: " << C.mLabel;
        EXPECT_TRUE(Result.mStderr.empty())
            << "group: " << C.mLabel << " stderr: " << Result.mStderr;
        // Every help block must mention "Usage:" so agents can confirm
        // they got help output, not something else.
        EXPECT_NE(Result.mStdout.find("Usage:"), std::string::npos)
            << "group: " << C.mLabel;
    }
}

// Empty-args at a group dispatcher prints group help and exits 0.
// Pre-v0.85.0 this threw UsageError.
TEST(HelpRouting, GroupEmptyArgsPrintsHelpExitZero)
{
    struct FCase
    {
        const char *mLabel;
        int (*mpDispatcher)(const std::vector<std::string> &,
                            const std::string &);
    };
    const FCase Cases[] = {
        {"topic", &UniPlan::RunTopicCommand},
        {"phase", &UniPlan::RunBundlePhaseCommand},
    };
    for (const FCase &C : Cases)
    {
        const auto Result = CaptureDispatcher(C.mpDispatcher, {});
        EXPECT_EQ(Result.mExitCode, 0) << "group: " << C.mLabel;
        EXPECT_FALSE(Result.mStdout.empty()) << "group: " << C.mLabel;
        EXPECT_NE(Result.mStdout.find("Usage:"), std::string::npos)
            << "group: " << C.mLabel;
    }
}

// Subcommand-level --help (e.g. "phase get --help") exits 0 on stdout.
// In Commit 1 the content falls through to the group block since
// subcommand entries aren't registered yet; Commit 2 populates them.
// What we assert here is the *routing contract*: exit 0, stdout, no
// stderr.
TEST(HelpRouting, SubcommandHelpExitsZeroStdoutOnly)
{
    struct FCase
    {
        const char *mLabel;
        int (*mpDispatcher)(const std::vector<std::string> &,
                            const std::string &);
        std::vector<std::string> mArgs;
    };
    const std::vector<FCase> Cases = {
        {"phase get --help",
         &UniPlan::RunBundlePhaseCommand,
         {"get", "--help"}},
        {"phase metric --help",
         &UniPlan::RunBundlePhaseCommand,
         {"metric", "--help"}},
        {"phase set --help",
         &UniPlan::RunBundlePhaseCommand,
         {"set", "--help"}},
        {"phase drift --help",
         &UniPlan::RunBundlePhaseCommand,
         {"drift", "--help"}},
        {"topic get --help", &UniPlan::RunTopicCommand, {"get", "--help"}},
        {"topic set --help", &UniPlan::RunTopicCommand, {"set", "--help"}},
        {"changelog add --help",
         &UniPlan::RunBundleChangelogCommand,
         {"add", "--help"}},
        {"verification add --help",
         &UniPlan::RunBundleVerificationCommand,
         {"add", "--help"}},
    };
    for (const FCase &C : Cases)
    {
        const auto Result = CaptureDispatcher(C.mpDispatcher, C.mArgs);
        EXPECT_EQ(Result.mExitCode, 0) << "case: " << C.mLabel;
        EXPECT_FALSE(Result.mStdout.empty()) << "case: " << C.mLabel;
        EXPECT_TRUE(Result.mStderr.empty())
            << "case: " << C.mLabel << " stderr: " << Result.mStderr;
    }
}

// Single-command handlers (validate / timeline / blockers / legacy-gap)
// also exit 0 on --help and write to stdout.
TEST(HelpRouting, SingleCommandHelpExitsZeroStdoutOnly)
{
    struct FCase
    {
        const char *mLabel;
        int (*mpDispatcher)(const std::vector<std::string> &,
                            const std::string &);
    };
    const FCase Cases[] = {
        {"validate", &UniPlan::RunBundleValidateCommand},
        {"timeline", &UniPlan::RunBundleTimelineCommand},
        {"blockers", &UniPlan::RunBundleBlockersCommand},
        {"legacy-gap", &UniPlan::RunLegacyGapCommand},
        {"graph", &UniPlan::RunGraphCommand},
        {"migrate", &UniPlan::RunMigrateCommand},
        {"_catalog", &UniPlan::RunCatalogCommand},
    };
    for (const FCase &C : Cases)
    {
        const auto Result = CaptureDispatcher(C.mpDispatcher, {"--help"});
        EXPECT_EQ(Result.mExitCode, 0) << "cmd: " << C.mLabel;
        EXPECT_FALSE(Result.mStdout.empty()) << "cmd: " << C.mLabel;
        EXPECT_TRUE(Result.mStderr.empty())
            << "cmd: " << C.mLabel << " stderr: " << Result.mStderr;
        EXPECT_NE(Result.mStdout.find("Usage:"), std::string::npos)
            << "cmd: " << C.mLabel;
    }
}

// Regression guard: unknown subcommands still throw UsageError.
// --help routing must not swallow malformed input into a success path.
TEST(HelpRouting, UnknownSubcommandStillErrors)
{
    EXPECT_THROW(UniPlan::RunTopicCommand({"bogus"}, "."), UniPlan::UsageError);
    EXPECT_THROW(UniPlan::RunBundlePhaseCommand({"nonsense"}, "."),
                 UniPlan::UsageError);
}

// Content scoping — subcommand leaves carry ONLY their flags, not the
// pooled group flags from siblings. This regression test guards
// against the v0.85.0 Commit 2 bleed cleanup being undone.
TEST(HelpContent, PhaseGetCarriesModesNotSetOnlyFlags)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "phase", "get");
    const std::string S = Out.str();
    // Present: get-only flags / modes
    EXPECT_NE(S.find("--brief"), std::string::npos);
    EXPECT_NE(S.find("--design"), std::string::npos);
    EXPECT_NE(S.find("--execution"), std::string::npos);
    EXPECT_NE(S.find("--phases"), std::string::npos);
    // Absent: set-only flags must NOT bleed in
    EXPECT_EQ(S.find("--done-clear"), std::string::npos);
    EXPECT_EQ(S.find("--investigation"), std::string::npos);
    EXPECT_EQ(S.find("--origin"), std::string::npos);
    EXPECT_EQ(S.find("--started-at"), std::string::npos);
}

TEST(HelpContent, PhaseSetCarriesMutationFlagsNotGetModes)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "phase", "set");
    const std::string S = Out.str();
    // Present: set-only flags
    EXPECT_NE(S.find("--done-clear"), std::string::npos);
    EXPECT_NE(S.find("--investigation"), std::string::npos);
    EXPECT_NE(S.find("--origin"), std::string::npos);
    EXPECT_NE(S.find("--started-at"), std::string::npos);
    // Absent: get-only modes must NOT bleed in
    EXPECT_EQ(S.find("--brief"), std::string::npos);
    EXPECT_EQ(S.find("--execution"), std::string::npos);
    EXPECT_EQ(S.find("--phases"), std::string::npos);
}

TEST(HelpContent, PhaseDriftDocumentsFourKindsAndSchema)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "phase", "drift");
    const std::string S = Out.str();
    EXPECT_NE(S.find("status_lag_lane"), std::string::npos);
    EXPECT_NE(S.find("status_lag_done"), std::string::npos);
    EXPECT_NE(S.find("status_lag_timestamp"), std::string::npos);
    EXPECT_NE(S.find("completion_lag_lane"), std::string::npos);
    EXPECT_NE(S.find("uni-plan-phase-drift-v1"), std::string::npos);
}

TEST(HelpContent, TopicGetDocumentsSectionsFlagAndValidNames)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "topic", "get");
    const std::string S = Out.str();
    EXPECT_NE(S.find("--sections"), std::string::npos);
    // Spot-check a few of the 14 valid section names
    EXPECT_NE(S.find("summary"), std::string::npos);
    EXPECT_NE(S.find("acceptance_criteria"), std::string::npos);
    EXPECT_NE(S.find("source_references"), std::string::npos);
    EXPECT_NE(S.find("phases"), std::string::npos);
}

TEST(HelpContent, TopicGetIsScopedAwayFromTopicSetFlags)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "topic", "get");
    const std::string S = Out.str();
    // topic set-only flags — must NOT appear on topic get
    EXPECT_EQ(S.find("--acceptance-criteria"), std::string::npos);
    EXPECT_EQ(S.find("--dependency-add"), std::string::npos);
    EXPECT_EQ(S.find("--baseline-audit"), std::string::npos);
}

TEST(HelpContent, ChangelogAddDocumentsAffectedFlag)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "changelog", "add");
    const std::string S = Out.str();
    EXPECT_NE(S.find("--affected"), std::string::npos);
    EXPECT_NE(S.find("feat"), std::string::npos);
    EXPECT_NE(S.find("refactor"), std::string::npos);
}

TEST(HelpContent, VerificationAddDocumentsDetailFlag)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "verification", "add");
    const std::string S = Out.str();
    EXPECT_NE(S.find("--detail"), std::string::npos);
    EXPECT_NE(S.find("--check"), std::string::npos);
    EXPECT_NE(S.find("--result"), std::string::npos);
}

// Schema-name coupling: every query subcommand's help must mention the
// actual k*Schema constant string so an accidental rename in
// UniPlanCliConstants.h trips a test failure.
TEST(HelpContent, QuerySubcommandsCiteSchemaConstants)
{
    struct FCase
    {
        const char *mGroup;
        const char *mSub;
        const char *mExpectedSchema;
    };
    const FCase Cases[] = {
        {"topic", "list", "uni-plan-topic-list-v1"},
        {"topic", "get", "uni-plan-topic-get-v1"},
        {"topic", "status", "uni-plan-topic-status-v1"},
        {"phase", "list", "uni-plan-phase-list-v2"},
        {"phase", "get", "uni-plan-phase-get-v1"},
        {"phase", "get", "uni-plan-phase-get-v2"},
        {"phase", "metric", "uni-plan-phase-metric-v1"},
        {"phase", "next", "uni-plan-phase-next-v1"},
        {"phase", "readiness", "uni-plan-phase-readiness-v1"},
        {"phase", "wave-status", "uni-plan-phase-wave-status-v1"},
        {"phase", "drift", "uni-plan-phase-drift-v1"},
        {"changelog", "query", "uni-plan-changelog-v2"},
        {"verification", "query", "uni-plan-verification-v2"},
    };
    for (const FCase &C : Cases)
    {
        std::ostringstream Out;
        UniPlan::PrintCommandUsage(Out, C.mGroup, C.mSub);
        const std::string S = Out.str();
        EXPECT_NE(S.find(C.mExpectedSchema), std::string::npos)
            << "case: " << C.mGroup << " " << C.mSub
            << " missing schema: " << C.mExpectedSchema;
    }
}

// Every leaf emits the Exit codes block — agents rely on this for
// error handling.
TEST(HelpContent, EveryLeafEmitsExitCodes)
{
    const char *const kLeaves[][2] = {
        {"topic", "list"},       {"topic", "get"},   {"topic", "set"},
        {"phase", "list"},       {"phase", "get"},   {"phase", "metric"},
        {"phase", "set"},        {"phase", "drift"}, {"changelog", "add"},
        {"verification", "add"}, {"job", "set"},     {"task", "set"},
    };
    for (const auto &C : kLeaves)
    {
        std::ostringstream Out;
        UniPlan::PrintCommandUsage(Out, C[0], C[1]);
        const std::string S = Out.str();
        EXPECT_NE(S.find("Exit codes:"), std::string::npos)
            << "leaf: " << C[0] << " " << C[1];
    }
}

// Direct smoke test of PrintCommandUsage — covers every catalog group to guard
// against an entry being accidentally dropped from the help registry.
TEST(HelpRouting, PrintCommandUsageCoversEveryRegisteredGroup)
{
    const auto Result = CaptureDispatcher(&UniPlan::RunCatalogCommand,
                                          {"--json"});
    ASSERT_EQ(Result.mExitCode, 0);
    const nlohmann::json Catalog = nlohmann::json::parse(Result.mStdout);
    ASSERT_TRUE(Catalog.contains("verbs"));

    for (const auto &Verb : Catalog["verbs"])
    {
        const std::string Name = Verb.value("name", "");
        std::ostringstream Out;
        UniPlan::PrintCommandUsage(Out, Name);
        const std::string S = Out.str();
        EXPECT_FALSE(S.empty()) << "group: " << Name;
        EXPECT_NE(S.find("Usage"), std::string::npos)
            << "group missing Usage: " << Name;
    }
}

// Coverage guard. The machine-readable catalog is the public command
// inventory, so every catalog leaf must have targeted help with exit-code
// prose and the advertised output schema.
TEST(HelpCoverage, EveryDispatchTargetHasHelpEntry)
{
    const auto Result = CaptureDispatcher(&UniPlan::RunCatalogCommand,
                                          {"--json"});
    ASSERT_EQ(Result.mExitCode, 0);
    ASSERT_TRUE(Result.mStderr.empty());

    const nlohmann::json Catalog = nlohmann::json::parse(Result.mStdout);
    ASSERT_TRUE(Catalog.contains("verbs"));
    ASSERT_TRUE(Catalog["verbs"].is_array());

    for (const auto &Verb : Catalog["verbs"])
    {
        const std::string VerbName = Verb.value("name", "");
        ASSERT_TRUE(Verb.contains("subcommands")) << VerbName;
        ASSERT_TRUE(Verb["subcommands"].is_array()) << VerbName;

        for (const auto &Subcommand : Verb["subcommands"])
        {
            const std::string SubName = Subcommand.value("name", "");
            const std::string Schema =
                Subcommand.value("output_schema_name", "");

            std::ostringstream Out;
            UniPlan::PrintCommandUsage(Out, VerbName, SubName);

            const std::string Help = Out.str();
            EXPECT_NE(Help.find("Usage:"), std::string::npos)
                << "missing usage help: " << VerbName << " " << SubName;
            EXPECT_NE(Help.find("Exit codes:"), std::string::npos)
                << "missing exit-code help: " << VerbName << " " << SubName;
            if (!Schema.empty())
            {
                EXPECT_NE(Help.find(Schema), std::string::npos)
                    << "missing schema help: " << VerbName << " " << SubName
                    << " schema: " << Schema;
            }
        }
    }
}

// Manifest list carries the v0.84.0 --stale-plan flag on its leaf.
TEST(HelpContent, ManifestListDocumentsStalePlanFlag)
{
    std::ostringstream Out;
    UniPlan::PrintCommandUsage(Out, "manifest", "list");
    const std::string S = Out.str();
    EXPECT_NE(S.find("--stale-plan"), std::string::npos);
    EXPECT_NE(S.find("stale_create"), std::string::npos);
    EXPECT_NE(S.find("stale_delete"), std::string::npos);
    EXPECT_NE(S.find("dangling_modify"), std::string::npos);
    EXPECT_NE(S.find("--missing-only"), std::string::npos);
}

// Cache subcommand --help produces a dedicated leaf block, not the
// group block.
TEST(HelpContent, CacheSubcommandsHaveLeafBlocks)
{
    for (const char *Sub : {"info", "clear", "config"})
    {
        std::ostringstream Out;
        UniPlan::PrintCommandUsage(Out, "cache", Sub);
        const std::string S = Out.str();
        EXPECT_NE(S.find("Exit codes:"), std::string::npos)
            << "cache " << Sub << " should emit leaf block";
    }
}
