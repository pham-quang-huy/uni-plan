#include "UniPlanBundleWriteGuard.h"
#include "UniPlanCliConstants.h"
#include "UniPlanCommandMutationCommon.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONIO.h"
#include "UniPlanOptionTypes.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// ===================================================================
// uni-plan migrate v4-prose-to-arrays
//
// Eager-normalization utility (v0.89.0+). Reads every bundle (or one
// when --topic is set), lets dual-read auto-promote legacy string-
// form `risks` / `next_actions` / `acceptance_criteria`, and writes
// the bundle back so the on-disk shape is canonical array form.
//
// Default is dry-run: scans disk for the raw JSON shape of the three
// fields and reports which bundles would be rewritten. Pass `--apply`
// to actually rewrite.
//
// This is the eager counterpart to the lazy normalization that
// happens automatically on any CLI mutation (every mutation path
// ends in WriteBundleBack which emits array form). Use `migrate`
// when you want to normalize bundles that otherwise wouldn't see a
// mutation for a while.
// ===================================================================

namespace
{

// Scan the raw JSON text on disk for `"<field>": "` (string) vs.
// `"<field>": [` (array). Returns true when the field is still in
// legacy string form (migration candidate). Any parse issue returns
// false — the file is probably fine or unreadable; either way, we
// don't want the migrate path to rewrite it blindly.
bool FieldIsLegacyStringOnDisk(const std::string &InRaw,
                               const std::string &InField)
{
    const std::string Key = "\"" + InField + "\"";
    size_t Pos = InRaw.find(Key);
    if (Pos == std::string::npos)
        return false;
    // Skip past the key and the colon.
    Pos = InRaw.find(':', Pos + Key.size());
    if (Pos == std::string::npos)
        return false;
    ++Pos;
    // Skip whitespace after the colon.
    while (Pos < InRaw.size() &&
           std::isspace(static_cast<unsigned char>(InRaw[Pos])))
        ++Pos;
    if (Pos >= InRaw.size())
        return false;
    return InRaw[Pos] == '"';
}

bool TryReadFileRaw(const fs::path &InPath, std::string &OutRaw)
{
    std::ifstream Stream(InPath, std::ios::binary);
    if (!Stream.is_open())
        return false;
    std::ostringstream Buffer;
    Buffer << Stream.rdbuf();
    OutRaw = Buffer.str();
    return true;
}

struct FMigrateFinding
{
    std::string mTopic;
    std::string mPath;
    bool mbRisksLegacy = false;
    bool mbNextActionsLegacy = false;
    bool mbAcceptanceCriteriaLegacy = false;

    bool NeedsRewrite() const
    {
        return mbRisksLegacy || mbNextActionsLegacy ||
               mbAcceptanceCriteriaLegacy;
    }
};

void EmitFindingsJson(const std::vector<FMigrateFinding> &InFindings,
                      bool InApplied, size_t InRewritten,
                      const std::string &InRepoRoot)
{
    const std::string UTC = GetUtcNow();
    PrintJsonHeader("uni-plan-migrate-v1", UTC, InRepoRoot);
    EmitJsonFieldBool("apply", InApplied);
    EmitJsonFieldSizeT("scanned_count", InFindings.size());
    EmitJsonFieldSizeT("rewritten_count", InRewritten);
    std::cout << "\"findings\":[";
    size_t EmitIndex = 0;
    for (const FMigrateFinding &F : InFindings)
    {
        if (!F.NeedsRewrite())
            continue;
        if (EmitIndex > 0)
            std::cout << ",";
        ++EmitIndex;
        std::cout << "{";
        EmitJsonField("topic", F.mTopic);
        EmitJsonField("path", F.mPath);
        EmitJsonFieldBool("risks_legacy", F.mbRisksLegacy);
        EmitJsonFieldBool("next_actions_legacy", F.mbNextActionsLegacy);
        EmitJsonFieldBool("acceptance_criteria_legacy",
                          F.mbAcceptanceCriteriaLegacy, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
}

} // namespace

int RunMigrateCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    // Parse flags: accept --topic, --apply, --repo-root plus common options.
    BaseOptions Common;
    const auto Remaining = ConsumeCommonOptions(InArgs, Common, false);
    std::string Topic;
    bool bApply = false;
    std::string SubMode;
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (SubMode.empty() && Token.size() > 0 && Token[0] != '-')
        {
            SubMode = Token;
            continue;
        }
        if (Token == "--topic")
        {
            Topic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--apply")
        {
            bApply = true;
            continue;
        }
        throw UsageError("Unknown option for migrate: " + Token);
    }
    if (SubMode != "v4-prose-to-arrays")
    {
        throw UsageError(
            "migrate requires a sub-mode; supported: v4-prose-to-arrays");
    }

    const fs::path RepoRoot = NormalizeRepoRootPath(
        Common.mRepoRoot.empty() ? InRepoRoot : Common.mRepoRoot);

    std::vector<FTopicBundle> Bundles;
    std::vector<std::string> LoadWarnings;
    if (!Topic.empty())
    {
        FTopicBundle B;
        std::string Error;
        if (!TryLoadBundleByTopic(RepoRoot, Topic, B, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(B));
    }
    else
    {
        Bundles = LoadAllBundles(RepoRoot, LoadWarnings);
    }

    std::vector<FMigrateFinding> Findings;
    size_t Rewritten = 0;
    for (const FTopicBundle &B : Bundles)
    {
        FMigrateFinding F;
        F.mTopic = B.mTopicKey;
        F.mPath = B.mBundlePath;
        std::string Raw;
        if (!TryReadFileRaw(B.mBundlePath, Raw))
            continue;
        F.mbRisksLegacy = FieldIsLegacyStringOnDisk(Raw, "risks");
        F.mbNextActionsLegacy = FieldIsLegacyStringOnDisk(Raw, "next_actions");
        F.mbAcceptanceCriteriaLegacy =
            FieldIsLegacyStringOnDisk(Raw, "acceptance_criteria");
        Findings.push_back(F);
        if (!bApply || !F.NeedsRewrite())
            continue;
        // Rewrite: the in-memory bundle is already in array form (dual-
        // read promoted it on load). Route through GuardedWriteBundle so
        // the rewrite gets lock + atomic-rename parity with every other
        // mutation path; LoadAllBundles stamped mReadSession so this also
        // picks up stale-check protection against a concurrent peer.
        std::string WriteError;
        if (GuardedWriteBundle(B, WriteError) != 0)
        {
            std::cerr << "migrate: " << B.mTopicKey << ": " << WriteError
                      << "\n";
            return 1;
        }
        ++Rewritten;
    }

    EmitFindingsJson(Findings, bApply, Rewritten, RepoRoot.string());
    return 0;
}

} // namespace UniPlan
