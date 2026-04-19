#include "UniPlanCommandHelp.h"
#include "UniPlanForwardDecls.h"
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

// Direct smoke test of PrintCommandUsage — covers every current
// kCommandHelp name to guard against an entry being accidentally
// dropped from the registry.
TEST(HelpRouting, PrintCommandUsageCoversEveryRegisteredGroup)
{
    for (const char *Name :
         {"topic", "phase", "job", "task", "changelog", "verification",
          "timeline", "blockers", "validate", "legacy-gap", "cache"})
    {
        std::ostringstream Out;
        UniPlan::PrintCommandUsage(Out, Name);
        const std::string S = Out.str();
        EXPECT_FALSE(S.empty()) << "group: " << Name;
        EXPECT_NE(S.find("Usage") == std::string::npos &&
                      S.find("cache") == std::string::npos,
                  true)
            << "group: " << Name; // cache uses inline format without
                                  // a "Usage:" bare line — accept it
    }
}
