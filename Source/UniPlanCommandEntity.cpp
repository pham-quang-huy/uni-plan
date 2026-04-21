#include "UniPlanCommandMutationCommon.h"
#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanOutputHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

// ===================================================================
// Tier 3: Evidence shortcuts
// ===================================================================

// ---------------------------------------------------------------------------
// phase log — changelog add scoped to a phase (with bounds check)
// ---------------------------------------------------------------------------

int RunPhaseLogCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FChangelogAddOptions Options = ParsePhaseLogOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Bounds check on phase index
    const int PhaseIndex = std::atoi(Options.mScope.c_str());
    if (PhaseIndex < 0 ||
        static_cast<size_t>(PhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range: " << PhaseIndex << "\n";
        return 1;
    }

    FChangeLogEntry Entry;
    Entry.mPhase = PhaseIndex;
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mChange = Options.mChange;
    Entry.mAffected = Options.mAffected;
    Entry.mActor = ETestingActor::AI;
    Entry.mType = Options.mType;

    Bundle.mChangeLogs.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetChangelogs);
    EmitJsonFieldSizeT("entry_index", Bundle.mChangeLogs.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase verify — verification add scoped to a phase (with bounds check)
// ---------------------------------------------------------------------------

int RunPhaseVerifyCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FVerificationAddOptions Options = ParsePhaseVerifyOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Bounds check on phase index
    const int PhaseIndex = std::atoi(Options.mScope.c_str());
    if (PhaseIndex < 0 ||
        static_cast<size_t>(PhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range: " << PhaseIndex << "\n";
        return 1;
    }

    FVerificationEntry Entry;
    Entry.mPhase = PhaseIndex;
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mCheck = Options.mCheck;
    Entry.mResult = Options.mResult;
    Entry.mDetail = Options.mDetail;

    Bundle.mVerifications.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetVerifications);
    EmitJsonFieldSizeT("entry_index", Bundle.mVerifications.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ===================================================================
// Tier 5: Missing entity coverage
// ===================================================================

// ---------------------------------------------------------------------------
// lane set — set lane status
// ---------------------------------------------------------------------------

int RunLaneSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FLaneSetOptions Options = ParseLaneSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    if (static_cast<size_t>(Options.mLaneIndex) >= Phase.mLanes.size())
    {
        std::cerr << "Lane index out of range\n";
        return 1;
    }

    FLaneRecord &Lane = Phase.mLanes[static_cast<size_t>(Options.mLaneIndex)];
    const std::string Target =
        MakeLaneTarget(Options.mPhaseIndex, Options.mLaneIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    std::string Desc;

    if (Options.opStatus.has_value())
    {
        const EExecutionStatus NewStatus = *Options.opStatus;
        const std::string NewStatusStr = ToString(NewStatus);
        Changes.push_back({"status", {ToString(Lane.mStatus), NewStatusStr}});
        Lane.mStatus = NewStatus;
        Desc = Target + " → " + NewStatusStr;
    }
    if (!Options.mScope.empty())
    {
        Changes.push_back({"scope", {Lane.mScope, Options.mScope}});
        Lane.mScope = Options.mScope;
        if (Desc.empty())
            Desc = Target + " updated scope";
    }
    if (!Options.mExitCriteria.empty())
    {
        Changes.push_back(
            {"exit_criteria", {Lane.mExitCriteria, Options.mExitCriteria}});
        Lane.mExitCriteria = Options.mExitCriteria;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    // Build the changelog message from the full field list so repeated
    // set calls with different field subsets produce distinct entries.
    // Previously only the last-changed field ended up in Desc, which
    // caused no_duplicate_changelog warnings whenever the same single
    // field was set twice.
    std::string FieldList;
    for (const auto &C : Changes)
    {
        if (!FieldList.empty())
            FieldList += ", ";
        FieldList += C.first;
    }
    AppendAutoChangelog(Bundle, Target, Target + " updated: " + FieldList);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// testing add — append a testing record to a phase
// ---------------------------------------------------------------------------

int RunTestingAddCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTestingAddOptions Options = ParseTestingAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    FTestingRecord Record;
    Record.mSession = Options.mSession;
    if (Options.opActor.has_value())
        Record.mActor = *Options.opActor;
    Record.mStep = Options.mStep;
    Record.mAction = Options.mAction;
    Record.mExpected = Options.mExpected;
    Record.mEvidence = Options.mEvidence;

    Phase.mTesting.push_back(std::move(Record));

    const std::string Target =
        MakePhaseTarget(Options.mPhaseIndex) + ".testing";

    const size_t NewTestingIndex = Phase.mTesting.size() - 1;
    AppendAutoChangelog(Bundle, Target,
                        "testing[" + std::to_string(NewTestingIndex) +
                            "] added to phases[" +
                            std::to_string(Options.mPhaseIndex) + "]");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("entry_index", Phase.mTesting.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// manifest add — append a file manifest item to a phase
// ---------------------------------------------------------------------------

int RunManifestAddCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FManifestAddOptions Options = ParseManifestAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    FFileManifestItem Item;
    Item.mFilePath = Options.mFile;
    Item.mAction = *Options.opAction;
    Item.mDescription = Options.mDescription;

    Phase.mFileManifest.push_back(std::move(Item));

    const std::string Target =
        MakePhaseTarget(Options.mPhaseIndex) + ".file_manifest";

    const size_t NewManifestIndex = Phase.mFileManifest.size() - 1;
    AppendAutoChangelog(Bundle, Target,
                        "file_manifest[" + std::to_string(NewManifestIndex) +
                            "] added to phases[" +
                            std::to_string(Options.mPhaseIndex) +
                            "]: " + Options.mFile);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("entry_index", Phase.mFileManifest.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// testing set — modify an existing testing record by index
// ---------------------------------------------------------------------------

int RunTestingSetCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTestingSetOptions Options = ParseTestingSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mTesting.size())
    {
        std::cerr << "Testing index out of range: " << Options.mIndex
                  << " (size " << Phase.mTesting.size() << ")\n";
        return 1;
    }

    FTestingRecord &Record =
        Phase.mTesting[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        MakeTestingTarget(Options.mPhaseIndex, Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mSession.empty())
    {
        Changes.push_back({"session", {Record.mSession, Options.mSession}});
        Record.mSession = Options.mSession;
    }
    if (Options.opActor.has_value())
    {
        const ETestingActor NewActor = *Options.opActor;
        const std::string NewActorStr = ToString(NewActor);
        Changes.push_back({"actor", {ToString(Record.mActor), NewActorStr}});
        Record.mActor = NewActor;
    }
    if (!Options.mStep.empty())
    {
        Changes.push_back({"step", {Record.mStep, Options.mStep}});
        Record.mStep = Options.mStep;
    }
    if (!Options.mAction.empty())
    {
        Changes.push_back({"action", {Record.mAction, Options.mAction}});
        Record.mAction = Options.mAction;
    }
    if (!Options.mExpected.empty())
    {
        Changes.push_back({"expected", {Record.mExpected, Options.mExpected}});
        Record.mExpected = Options.mExpected;
    }
    if (!Options.mEvidence.empty())
    {
        Changes.push_back({"evidence", {Record.mEvidence, Options.mEvidence}});
        Record.mEvidence = Options.mEvidence;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// verification set — modify an existing verification by index
// ---------------------------------------------------------------------------

int RunVerificationSetCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FVerificationSetOptions Options = ParseVerificationSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mIndex) >= Bundle.mVerifications.size())
    {
        std::cerr << "Verification index out of range: " << Options.mIndex
                  << " (size " << Bundle.mVerifications.size() << ")\n";
        return 1;
    }

    FVerificationEntry &Entry =
        Bundle.mVerifications[static_cast<size_t>(Options.mIndex)];
    const std::string Target = MakeVerificationTarget(Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mCheck.empty())
    {
        Changes.push_back({"check", {Entry.mCheck, Options.mCheck}});
        Entry.mCheck = Options.mCheck;
    }
    if (!Options.mResult.empty())
    {
        Changes.push_back({"result", {Entry.mResult, Options.mResult}});
        Entry.mResult = Options.mResult;
    }
    if (!Options.mDetail.empty())
    {
        Changes.push_back({"detail", {Entry.mDetail, Options.mDetail}});
        Entry.mDetail = Options.mDetail;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// manifest set — modify an existing file manifest entry by index
// ---------------------------------------------------------------------------

int RunManifestSetCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FManifestSetOptions Options = ParseManifestSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mFileManifest.size())
    {
        std::cerr << "Manifest index out of range: " << Options.mIndex
                  << " (size " << Phase.mFileManifest.size() << ")\n";
        return 1;
    }

    FFileManifestItem &Item =
        Phase.mFileManifest[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        MakeManifestTarget(Options.mPhaseIndex, Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mFile.empty())
    {
        Changes.push_back({"file_path", {Item.mFilePath, Options.mFile}});
        Item.mFilePath = Options.mFile;
    }
    if (Options.opAction.has_value())
    {
        const EFileAction NewAction = *Options.opAction;
        const std::string NewActionStr = ToString(NewAction);
        Changes.push_back({"action", {ToString(Item.mAction), NewActionStr}});
        Item.mAction = NewAction;
    }
    if (!Options.mDescription.empty())
    {
        Changes.push_back(
            {"description", {Item.mDescription, Options.mDescription}});
        Item.mDescription = Options.mDescription;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// manifest remove - drop a file manifest entry by index
//
// Needed to undo bad manifest adds (e.g., invented file paths that
// don't exist on disk). No trailing-only restriction - unlike phase
// remove, file_manifest entries are not referenced by other entities
// so removing any index is safe. Auto-changelog is filed phase-scoped
// (not targeted at the removed index) so the removed-index path does
// not dangle.
// ---------------------------------------------------------------------------

int RunManifestRemoveCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FManifestRemoveOptions Options = ParseManifestRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mFileManifest.size())
    {
        std::cerr << "Manifest index out of range: " << Options.mIndex
                  << " (size " << Phase.mFileManifest.size() << ")\n";
        return 1;
    }

    const std::string RemovedFile =
        Phase.mFileManifest[static_cast<size_t>(Options.mIndex)].mFilePath;
    Phase.mFileManifest.erase(Phase.mFileManifest.begin() + Options.mIndex);

    const std::string PhaseTarget = MakePhaseTarget(Options.mPhaseIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back({"file_manifest[" + std::to_string(Options.mIndex) + "]",
                       {RemovedFile, "(removed)"}});

    AppendAutoChangelog(Bundle, PhaseTarget,
                        "file_manifest[" + std::to_string(Options.mIndex) +
                            "] removed from phases[" +
                            std::to_string(Options.mPhaseIndex) +
                            "]: " + RemovedFile);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, PhaseTarget, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// manifest list — enumerate file_manifest entries across bundles
//
// Read-only aggregate query. Optional --topic filters to a single bundle,
// --phase further narrows to a single phase index, --missing-only filters
// to entries whose file_path does not resolve on disk. Designed so that
// "which manifest paths are broken across all 42 bundles" is a single
// CLI call instead of a Python loop over raw JSON reads — the gap that
// prompted this feature in v0.71.0.
// ---------------------------------------------------------------------------

// Classify one FFileManifestItem against disk reality. Empty string means
// the manifest intent and on-disk reality agree. Shared between JSON and
// human renderers so both emit the same verdict per row. Added v0.84.0.
static std::string ComputeManifestStaleReason(const fs::path &InRepoRoot,
                                              const FFileManifestItem &InItem,
                                              bool &OutExists)
{
    OutExists = ManifestPathExists(InRepoRoot, InItem.mFilePath);
    if (InItem.mAction == EFileAction::Create && OutExists)
        return "stale_create";
    if (InItem.mAction == EFileAction::Delete && OutExists)
        return "stale_delete";
    if (InItem.mAction == EFileAction::Modify && !OutExists)
        return "dangling_modify";
    return std::string();
}

// Decide whether one manifest row should appear in the output given the
// active filter flags. Returns true when the row is kept. --missing-only
// and --stale-plan are orthogonal AND predicates: when both are set, a
// row must satisfy both.
static bool KeepManifestRow(const FManifestListOptions &InOptions,
                            bool InbExists, const std::string &InStaleReason)
{
    if (InOptions.mbMissingOnly && InbExists)
        return false;
    if (InOptions.mbStalePlan && InStaleReason.empty())
        return false;
    return true;
}

// One ANSI-color hint per stale_reason (red for drift, dim "-" when
// aligned). Separated so the single-word label can change without
// reshuffling the table column layout. Added v0.84.0 alongside the
// manifest list --human renderer.
static std::string ColorizeStaleReason(const std::string &InReason)
{
    if (InReason.empty())
        return Colorize(kColorDim, "-");
    return Colorize(kColorRed, InReason);
}

static int RunManifestListJson(const fs::path &InRepoRoot,
                               const FManifestListOptions &InOptions,
                               const std::vector<FTopicBundle> &InBundles,
                               const std::vector<std::string> &InWarnings)
{
    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot.string());
    std::cout << "\"entries\":[";
    bool bFirst = true;
    size_t TotalEntries = 0;
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            if (InOptions.mPhaseIndex >= 0 &&
                PI != static_cast<size_t>(InOptions.mPhaseIndex))
                continue;
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t MI = 0; MI < Phase.mFileManifest.size(); ++MI)
            {
                const FFileManifestItem &FM = Phase.mFileManifest[MI];
                bool bExists = false;
                const std::string StaleReason =
                    ComputeManifestStaleReason(InRepoRoot, FM, bExists);
                if (!KeepManifestRow(InOptions, bExists, StaleReason))
                    continue;
                if (!bFirst)
                    std::cout << ",";
                bFirst = false;
                ++TotalEntries;
                std::cout << "{";
                EmitJsonField("topic", B.mTopicKey);
                EmitJsonFieldSizeT("phase_index", PI);
                EmitJsonFieldSizeT("manifest_index", MI);
                EmitJsonField("file_path", FM.mFilePath);
                EmitJsonField("action", ToString(FM.mAction));
                EmitJsonField("description", FM.mDescription);
                EmitJsonFieldBool("exists_on_disk", bExists);
                EmitJsonFieldNullable("stale_reason", StaleReason, false);
                std::cout << "}";
            }
        }
    }
    std::cout << "],";
    EmitJsonFieldSizeT("entry_count", TotalEntries);
    PrintJsonClose(InWarnings);
    return 0;
}

// Human-mode renderer (v0.84.0+). Replaces the silent-fallback-to-JSON
// behavior that `manifest list` had before — `--human` is now a real
// ANSI-table surface. Columns: Topic, Ph, M, Action, Exists, Stale,
// File, Description (full content; v0.97.0+ no-truncation contract).
// Header line summarizes total count +
// active filters so a human reader sees context at a glance.
static int RunManifestListHuman(const fs::path &InRepoRoot,
                                const FManifestListOptions &InOptions,
                                const std::vector<FTopicBundle> &InBundles,
                                const std::vector<std::string> &InWarnings)
{
    HumanTable Table;
    Table.mHeaders = {"Topic",  "Ph",    "M",    "Action",
                      "Exists", "Stale", "File", "Description"};
    size_t TotalEntries = 0;
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            if (InOptions.mPhaseIndex >= 0 &&
                PI != static_cast<size_t>(InOptions.mPhaseIndex))
                continue;
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t MI = 0; MI < Phase.mFileManifest.size(); ++MI)
            {
                const FFileManifestItem &FM = Phase.mFileManifest[MI];
                bool bExists = false;
                const std::string StaleReason =
                    ComputeManifestStaleReason(InRepoRoot, FM, bExists);
                if (!KeepManifestRow(InOptions, bExists, StaleReason))
                    continue;
                ++TotalEntries;
                const std::string ExistsCell =
                    bExists ? Colorize(kColorGreen, "yes")
                            : Colorize(kColorRed, "no");
                Table.AddRow({B.mTopicKey, std::to_string(PI),
                              std::to_string(MI), ToString(FM.mAction),
                              ExistsCell, ColorizeStaleReason(StaleReason),
                              FM.mFilePath, FM.mDescription});
            }
        }
    }

    // Header summary line — mirrors the agent-facing JSON `entry_count`
    // but adds filter context so a human reader isn't surprised by a
    // short result.
    std::cout << kColorBold << "File manifest" << kColorReset
              << "  count=" << kColorOrange << TotalEntries << kColorReset;
    if (!InOptions.mTopic.empty())
        std::cout << "  topic=" << kColorOrange << InOptions.mTopic
                  << kColorReset;
    if (InOptions.mPhaseIndex >= 0)
        std::cout << "  phase=" << kColorOrange << InOptions.mPhaseIndex
                  << kColorReset;
    if (InOptions.mbMissingOnly)
        std::cout << "  " << Colorize(kColorDim, "[--missing-only]");
    if (InOptions.mbStalePlan)
        std::cout << "  " << Colorize(kColorDim, "[--stale-plan]");
    std::cout << "\n\n";

    if (TotalEntries == 0)
    {
        std::cout << Colorize(kColorDim,
                              "No manifest entries match the filter.")
                  << "\n";
    }
    else
    {
        Table.Print();
    }
    PrintHumanWarnings(InWarnings);
    return 0;
}

int RunManifestListCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot)
{
    const FManifestListOptions Options = ParseManifestListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    std::vector<std::string> BundleWarnings;
    std::vector<FTopicBundle> Bundles;
    if (!Options.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(RepoRoot, BundleWarnings);
    }

    if (Options.mbHuman)
        return RunManifestListHuman(RepoRoot, Options, Bundles, BundleWarnings);
    return RunManifestListJson(RepoRoot, Options, Bundles, BundleWarnings);
}

// ---------------------------------------------------------------------------
// changelog set — modify an existing changelog entry by index
// ---------------------------------------------------------------------------

int RunChangelogSetCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot)
{
    const FChangelogSetOptions Options = ParseChangelogSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mIndex) >= Bundle.mChangeLogs.size())
    {
        std::cerr << "Changelog index out of range: " << Options.mIndex
                  << " (size " << Bundle.mChangeLogs.size() << ")\n";
        return 1;
    }

    FChangeLogEntry &Entry =
        Bundle.mChangeLogs[static_cast<size_t>(Options.mIndex)];
    const std::string Target = MakeChangelogTarget(Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (Options.mPhase != -2)
    {
        if (Options.mPhase >= 0 &&
            static_cast<size_t>(Options.mPhase) >= Bundle.mPhases.size())
        {
            std::cerr << "Phase out of range: " << Options.mPhase << "\n";
            return 1;
        }
        Changes.push_back(
            {"phase",
             {std::to_string(Entry.mPhase), std::to_string(Options.mPhase)}});
        Entry.mPhase = Options.mPhase;
    }
    if (!Options.mDate.empty())
    {
        Changes.push_back({"date", {Entry.mDate, Options.mDate}});
        Entry.mDate = Options.mDate;
    }
    if (!Options.mChange.empty())
    {
        Changes.push_back({"change", {Entry.mChange, Options.mChange}});
        Entry.mChange = Options.mChange;
    }
    if (Options.opType.has_value())
    {
        const EChangeType NewType = *Options.opType;
        const std::string NewTypeStr = ToString(NewType);
        Changes.push_back({"type", {ToString(Entry.mType), NewTypeStr}});
        Entry.mType = NewType;
    }
    if (!Options.mAffected.empty())
    {
        Changes.push_back({"affected", {Entry.mAffected, Options.mAffected}});
        Entry.mAffected = Options.mAffected;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// changelog remove — erase an existing changelog entry by index
// ---------------------------------------------------------------------------
//
// Use case: dedup residuals, accidental duplicate entries emitted by bulk
// mutations, or explicit repair of a bundle's audit trail. Shifts all
// subsequent indices down by one.
int RunChangelogRemoveCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FChangelogRemoveOptions Options = ParseChangelogRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mIndex) >= Bundle.mChangeLogs.size())
    {
        std::cerr << "Changelog index out of range: " << Options.mIndex
                  << " (size " << Bundle.mChangeLogs.size() << ")\n";
        return 1;
    }

    const FChangeLogEntry Removed =
        Bundle.mChangeLogs[static_cast<size_t>(Options.mIndex)];
    Bundle.mChangeLogs.erase(Bundle.mChangeLogs.begin() +
                             static_cast<ptrdiff_t>(Options.mIndex));

    const std::string Target = MakeChangelogTarget(Options.mIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes = {{"change", {Removed.mChange, "(removed)"}}};

    // Removing an audit entry is itself an audit event — surface the removed
    // change text so the trail records *what* was dropped and at which index.
    AppendAutoChangelog(Bundle, Target,
                        Target + " removed: '" + Removed.mChange + "'");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// lane add — append a new lane to a phase
// ---------------------------------------------------------------------------

int RunLaneAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FLaneAddOptions Options = ParseLaneAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    FLaneRecord Lane;
    if (Options.opStatus.has_value())
        Lane.mStatus = *Options.opStatus;
    Lane.mScope = Options.mScope;
    Lane.mExitCriteria = Options.mExitCriteria;

    Phase.mLanes.push_back(std::move(Lane));

    const std::string Target = MakePhaseTarget(Options.mPhaseIndex) + ".lanes";

    AppendAutoChangelog(Bundle, Target,
                        "Lane added at index " +
                            std::to_string(Phase.mLanes.size() - 1));
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("lane_index", Phase.mLanes.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// manifest suggest (v0.86.0) — backfill helper that scans git history
// for the phase's started_at..completed_at window and proposes
// file_manifest entries.
//
// Defaults to dry-run JSON output. With --apply, calls
// RunManifestAddCommand for each suggestion (full mutation path: writes
// the bundle, auto-changelog, validation). Files already in the
// manifest are filtered out — repeat invocations are idempotent.
//
// Why this lives in uni-plan (not a shell script):
//   * The phase window (started_at, completed_at) only exists inside
//     the bundle — only the CLI can read it under the CLI-only rule.
//   * The dedupe predicate ("file already in manifest") needs the
//     typed FFileManifestItem array.
//   * --apply needs the same auto-changelog + validation gates that
//     manifest add already runs.
// ---------------------------------------------------------------------------

// Map a single git --name-status status letter to EFileAction. R/C
// (rename/copy) are treated as `modify` of the destination — close
// enough for backfill; authors can edit the action later via
// `manifest set`. Unknown letters skip the row.
static bool TryGitStatusToFileAction(char InStatus, EFileAction &OutAction)
{
    switch (InStatus)
    {
    case 'A':
        OutAction = EFileAction::Create;
        return true;
    case 'M':
    case 'R': // rename — destination side, treat as modify of new path
    case 'C': // copy — destination side, treat as modify
    case 'T': // type change (file → symlink etc.)
        OutAction = EFileAction::Modify;
        return true;
    case 'D':
        OutAction = EFileAction::Delete;
        return true;
    default:
        return false;
    }
}

// Spawn `git -C <repo_root> log --name-status --no-renames` over the
// phase's [started_at..completed_at] window. Returns the raw stdout for
// the caller to parse line by line. ISO timestamps are validated by the
// caller before reaching this function (defense-in-depth: the parser
// gate already rejects malformed inputs at parse time).
static bool RunGitLogNameStatusInWindow(const fs::path &InRepoRoot,
                                        const std::string &InSinceISO,
                                        const std::string &InUntilISO,
                                        std::string &OutStdout,
                                        std::string &OutError)
{
    // Build the command. Quote the timestamps to pass through the shell
    // unchanged. ISO timestamps contain only [0-9TZ:-], which are shell-
    // safe — but the quoting also future-proofs against unforeseen
    // characters and matches the existing CLI convention of always
    // quoting bundle paths.
    std::ostringstream Command;
    Command << "git -C \"" << InRepoRoot.string() << "\" log "
            << "--name-status --no-renames "
            << "--since=\"" << InSinceISO << "\" "
            << "--until=\"" << InUntilISO << "\" "
            << "--pretty=format:";
    FILE *rpPipe = popen(Command.str().c_str(), "r");
    if (rpPipe == nullptr)
    {
        OutError = "manifest suggest: failed to spawn `git log` (popen)";
        return false;
    }
    char Buffer[4096];
    while (std::fgets(Buffer, sizeof(Buffer), rpPipe) != nullptr)
    {
        OutStdout.append(Buffer);
    }
    const int ExitCode = pclose(rpPipe);
    if (ExitCode != 0)
    {
        OutError = "manifest suggest: `git log` exited non-zero (status=" +
                   std::to_string(ExitCode) + "); is the repo a git checkout?";
        return false;
    }
    return true;
}

// Collapse multiple history rows for the same path into a single
// suggestion. Rules:
//   * If a file appears as both A and D in the window → cancel (file
//     was created and deleted within the window; don't suggest).
//   * Otherwise the LAST status wins (e.g., A then M → keep A; M then D
//     → keep D). This matches the "what happened by end of window"
//     intent of the suggestion.
struct FManifestSuggestion
{
    std::string mFilePath;
    EFileAction mAction = EFileAction::Modify;
};

static std::vector<FManifestSuggestion>
CollapseGitLogToSuggestions(const std::string &InGitLog)
{
    // Two passes: first collect per-path action history, then collapse.
    std::map<std::string, std::vector<char>> PerPath;
    std::vector<std::string> InsertOrder; // preserve first-seen order
    std::istringstream Stream(InGitLog);
    std::string Line;
    while (std::getline(Stream, Line))
    {
        if (Line.empty())
            continue;
        // Format: "<STATUS>\t<PATH>" (single-letter status; tab-separated).
        if (Line.size() < 3 || Line[1] != '\t')
            continue;
        const char Status = Line[0];
        const std::string Path = Line.substr(2);
        if (Path.empty())
            continue;
        if (PerPath.find(Path) == PerPath.end())
            InsertOrder.push_back(Path);
        PerPath[Path].push_back(Status);
    }
    std::vector<FManifestSuggestion> Out;
    for (const std::string &Path : InsertOrder)
    {
        const auto &History = PerPath[Path];
        if (History.empty())
            continue;
        // Cancel A-then-D within window.
        bool bSawAdd = false;
        bool bSawDelete = false;
        for (const char S : History)
        {
            if (S == 'A')
                bSawAdd = true;
            if (S == 'D')
                bSawDelete = true;
        }
        if (bSawAdd && bSawDelete)
            continue; // ephemeral file, don't suggest
        // Use the last actionable status.
        EFileAction Action = EFileAction::Modify;
        bool bResolved = false;
        for (auto It = History.rbegin(); It != History.rend(); ++It)
        {
            if (TryGitStatusToFileAction(*It, Action))
            {
                bResolved = true;
                break;
            }
        }
        if (!bResolved)
            continue;
        FManifestSuggestion S;
        S.mFilePath = Path;
        S.mAction = Action;
        Out.push_back(std::move(S));
    }
    return Out;
}

int RunManifestSuggestCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FManifestSuggestOptions Options = ParseManifestSuggestOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }
    if (Options.mPhaseIndex < 0 ||
        static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index " << Options.mPhaseIndex
                  << " out of range (0.." << Bundle.mPhases.size() - 1 << ")\n";
        return 1;
    }
    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string &Started = Phase.mLifecycle.mStartedAt;
    const std::string &Completed = Phase.mLifecycle.mCompletedAt;
    if (Started.empty())
    {
        std::cerr
            << "manifest suggest: phase has no started_at; backfill the "
               "lifecycle stamp first via `phase set --started-at <iso>` "
               "or run the phase through `phase start` / `phase complete`\n";
        return 1;
    }
    // For completed phases, use completed_at as the upper bound; for
    // in_progress phases, use the current time so authors can suggest
    // mid-phase. "now" is git's understood synonym.
    const std::string Until = Completed.empty() ? "now" : Completed;

    std::string GitOut;
    if (!RunGitLogNameStatusInWindow(RepoRoot, Started, Until, GitOut, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }
    std::vector<FManifestSuggestion> Raw = CollapseGitLogToSuggestions(GitOut);

    // Filter out files already in the manifest. Repeat invocations are
    // idempotent: once an author has run --apply, re-running the
    // command surfaces only newly-touched files.
    std::set<std::string> AlreadyInManifest;
    for (const FFileManifestItem &Item : Phase.mFileManifest)
        AlreadyInManifest.insert(Item.mFilePath);

    std::vector<FManifestSuggestion> Suggestions;
    for (FManifestSuggestion &S : Raw)
    {
        if (AlreadyInManifest.find(S.mFilePath) != AlreadyInManifest.end())
            continue;
        Suggestions.push_back(std::move(S));
    }

    // --apply: mutate the bundle by invoking RunManifestAddCommand for
    // each suggestion. Reuses the existing auto-changelog + validation
    // pipeline, so the side effects are identical to a manual sequence
    // of `manifest add` calls. Failures are reported with the index of
    // the failing suggestion so callers can re-run from a known point.
    int AppliedCount = 0;
    int ApplyExitCode = 0;
    if (Options.mbApply)
    {
        for (size_t I = 0; I < Suggestions.size(); ++I)
        {
            const FManifestSuggestion &S = Suggestions[I];
            const std::vector<std::string> AddArgs = {
                "--topic",
                Options.mTopic,
                "--phase",
                std::to_string(Options.mPhaseIndex),
                "--file",
                S.mFilePath,
                "--action",
                ToString(S.mAction),
                "--description",
                "Backfilled by `manifest suggest --apply` from git history "
                "in phase window [" +
                    Started + ", " + Until + "]",
                "--repo-root",
                RepoRoot.string(),
            };
            // Discard the per-add JSON output — the suggest command
            // emits its own summary at the end.
            std::ostringstream NullSink;
            std::streambuf *rpOldStream = std::cout.rdbuf(NullSink.rdbuf());
            const int Code = RunManifestAddCommand(AddArgs, RepoRoot.string());
            std::cout.rdbuf(rpOldStream);
            if (Code != 0)
            {
                ApplyExitCode = Code;
                std::cerr << "manifest suggest --apply: row " << I
                          << " (file=" << S.mFilePath << ") failed; "
                          << "remaining " << (Suggestions.size() - I - 1)
                          << " row(s) not applied\n";
                break;
            }
            ++AppliedCount;
        }
    }

    // Emit summary JSON. Fields: schema, phase window, suggestion count,
    // applied count (0 in dry-run), per-row file_path/action.
    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kManifestSuggestSchema, UTC, RepoRoot.string());
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", Options.mPhaseIndex);
    EmitJsonField("started_at", Started);
    EmitJsonField("until", Until);
    EmitJsonFieldBool("applied", Options.mbApply);
    EmitJsonFieldSizeT("suggestion_count", Suggestions.size());
    EmitJsonFieldInt("applied_count", AppliedCount);
    std::cout << "\"suggestions\":[";
    for (size_t I = 0; I < Suggestions.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        std::cout << "{";
        EmitJsonField("file_path", Suggestions[I].mFilePath);
        EmitJsonField("action", ToString(Suggestions[I].mAction), false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return ApplyExitCode;
}

} // namespace UniPlan
