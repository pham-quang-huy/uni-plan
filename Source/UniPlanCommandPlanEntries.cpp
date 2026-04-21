#include "UniPlanCliConstants.h"
#include "UniPlanCommandMutationCommon.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanOptionTypes.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

// ===================================================================
// v0.89.0 plan-entry mutation commands
//
// `risk`, `next-action`, `acceptance-criterion` groups each expose
// `add / set / remove / list` subcommands that mutate the typed arrays
// on `FPlanMetadata::mRisks` / `FPlanMetadata::mAcceptanceCriteria` /
// `FTopicBundle::mNextActions`. Shape mirrors the existing `changelog`
// add/set/remove precedent at UniPlanCommandMutation.cpp:992 +
// UniPlanCommandEntity.cpp:859,947. Each add/set/remove writes the
// bundle back via the shared WriteBundleBack + AppendAutoChangelog
// contract; list commands are read-only.
// ===================================================================

namespace
{

using Change = std::pair<std::string, std::pair<std::string, std::string>>;

void EmitArrayMutationAdded(const std::string &InTopic,
                            const std::string &InTarget, size_t InEntryIndex)
{
    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", InTopic);
    EmitJsonField("target", InTarget);
    EmitJsonFieldSizeT("entry_index", InEntryIndex, false);
    std::cout << "}\n";
}

bool CheckIndexInRange(int InIndex, size_t InSize, const char *InLabel)
{
    if (InIndex < 0 || static_cast<size_t>(InIndex) >= InSize)
    {
        std::cerr << InLabel << " index out of range: " << InIndex << " (size "
                  << InSize << ")\n";
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// risk add
// ---------------------------------------------------------------------------

int RunRiskAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FRiskAddOptions Options = ParseRiskAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FRiskEntry Entry;
    Entry.mId = Options.mId;
    Entry.mStatement = Options.mStatement;
    Entry.mMitigation = Options.mMitigation;
    if (Options.opSeverity.has_value())
        Entry.mSeverity = *Options.opSeverity;
    if (Options.opStatus.has_value())
        Entry.mStatus = *Options.opStatus;
    Entry.mNotes = Options.mNotes;

    Bundle.mMetadata.mRisks.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mMetadata.mRisks.size() - 1;
    const std::string Target = "risks[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan, "risk added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// ---------------------------------------------------------------------------
// risk set
// ---------------------------------------------------------------------------

int RunRiskSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FRiskSetOptions Options = ParseRiskSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mMetadata.mRisks.size(),
                           "Risk"))
        return 1;

    FRiskEntry &Entry =
        Bundle.mMetadata.mRisks[static_cast<size_t>(Options.mIndex)];
    const std::string Target = "risks[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (!Options.mId.empty())
    {
        Changes.push_back({"id", {Entry.mId, Options.mId}});
        Entry.mId = Options.mId;
    }
    if (!Options.mStatement.empty())
    {
        Changes.push_back(
            {"statement", {Entry.mStatement, Options.mStatement}});
        Entry.mStatement = Options.mStatement;
    }
    if (!Options.mMitigation.empty())
    {
        Changes.push_back(
            {"mitigation", {Entry.mMitigation, Options.mMitigation}});
        Entry.mMitigation = Options.mMitigation;
    }
    if (Options.opSeverity.has_value())
    {
        const ERiskSeverity NewSeverity = *Options.opSeverity;
        Changes.push_back(
            {"severity", {ToString(Entry.mSeverity), ToString(NewSeverity)}});
        Entry.mSeverity = NewSeverity;
    }
    if (Options.opStatus.has_value())
    {
        const ERiskStatus NewStatus = *Options.opStatus;
        Changes.push_back(
            {"status", {ToString(Entry.mStatus), ToString(NewStatus)}});
        Entry.mStatus = NewStatus;
    }
    if (!Options.mNotes.empty())
    {
        Changes.push_back({"notes", {Entry.mNotes, Options.mNotes}});
        Entry.mNotes = Options.mNotes;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// risk remove
// ---------------------------------------------------------------------------

int RunRiskRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FRiskRemoveOptions Options = ParseRiskRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mMetadata.mRisks.size(),
                           "Risk"))
        return 1;

    const std::string Target = "risks[" + std::to_string(Options.mIndex) + "]";
    const FRiskEntry Removed =
        Bundle.mMetadata.mRisks[static_cast<size_t>(Options.mIndex)];
    Bundle.mMetadata.mRisks.erase(Bundle.mMetadata.mRisks.begin() +
                                  Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "risk removed: " + Target + " (" + Removed.mStatement +
                            ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_statement", Removed.mStatement, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// risk list
// ---------------------------------------------------------------------------

int RunRiskListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FRiskListOptions Options = ParseRiskListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Preserve storage-order indices through the filter so the emitted
    // `index` field remains bound to the pre-filter position (v0.95.0+).
    // `risk set --index N` / `risk remove --index N` target the storage
    // index in Bundle.mMetadata.mRisks; any filtered list must expose
    // that stable target, not the filtered-array position.
    std::vector<FRiskEntry> Filtered;
    std::vector<size_t> OriginalIndices;
    for (size_t I = 0; I < Bundle.mMetadata.mRisks.size(); ++I)
    {
        const FRiskEntry &R = Bundle.mMetadata.mRisks[I];
        if (Options.opSeverityFilter.has_value() &&
            R.mSeverity != *Options.opSeverityFilter)
            continue;
        if (Options.opStatusFilter.has_value() &&
            R.mStatus != *Options.opStatusFilter)
            continue;
        Filtered.push_back(R);
        OriginalIndices.push_back(I);
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Filtered.size());
    EmitRisksJson("risks", Filtered, true, &OriginalIndices);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// next-action add
// ---------------------------------------------------------------------------

int RunNextActionAddCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FNextActionAddOptions Options = ParseNextActionAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FNextActionEntry Entry;
    // Auto-assign order as size+1 when caller left --order unset (0).
    // Preserves the "ordered list" semantics that the legacy string form
    // encoded via pipe-delimited numeric first segment.
    Entry.mOrder = (Options.mOrder == 0)
                       ? static_cast<int>(Bundle.mNextActions.size()) + 1
                       : Options.mOrder;
    Entry.mStatement = Options.mStatement;
    Entry.mRationale = Options.mRationale;
    Entry.mOwner = Options.mOwner;
    if (Options.opStatus.has_value())
        Entry.mStatus = *Options.opStatus;
    Entry.mTargetDate = Options.mTargetDate;

    Bundle.mNextActions.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mNextActions.size() - 1;
    const std::string Target = "next_actions[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan, "next_action added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// ---------------------------------------------------------------------------
// next-action set
// ---------------------------------------------------------------------------

int RunNextActionSetCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FNextActionSetOptions Options = ParseNextActionSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mNextActions.size(),
                           "Next-action"))
        return 1;

    FNextActionEntry &Entry =
        Bundle.mNextActions[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        "next_actions[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (Options.mOrder >= 0)
    {
        Changes.push_back(
            {"order",
             {std::to_string(Entry.mOrder), std::to_string(Options.mOrder)}});
        Entry.mOrder = Options.mOrder;
    }
    if (!Options.mStatement.empty())
    {
        Changes.push_back(
            {"statement", {Entry.mStatement, Options.mStatement}});
        Entry.mStatement = Options.mStatement;
    }
    if (!Options.mRationale.empty())
    {
        Changes.push_back(
            {"rationale", {Entry.mRationale, Options.mRationale}});
        Entry.mRationale = Options.mRationale;
    }
    if (!Options.mOwner.empty())
    {
        Changes.push_back({"owner", {Entry.mOwner, Options.mOwner}});
        Entry.mOwner = Options.mOwner;
    }
    if (Options.opStatus.has_value())
    {
        const EActionStatus NewStatus = *Options.opStatus;
        Changes.push_back(
            {"status", {ToString(Entry.mStatus), ToString(NewStatus)}});
        Entry.mStatus = NewStatus;
    }
    if (!Options.mTargetDate.empty())
    {
        Changes.push_back(
            {"target_date", {Entry.mTargetDate, Options.mTargetDate}});
        Entry.mTargetDate = Options.mTargetDate;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// next-action remove
// ---------------------------------------------------------------------------

int RunNextActionRemoveCommand(const std::vector<std::string> &InArgs,
                               const std::string &InRepoRoot)
{
    const FNextActionRemoveOptions Options =
        ParseNextActionRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mNextActions.size(),
                           "Next-action"))
        return 1;

    const std::string Target =
        "next_actions[" + std::to_string(Options.mIndex) + "]";
    const FNextActionEntry Removed =
        Bundle.mNextActions[static_cast<size_t>(Options.mIndex)];
    Bundle.mNextActions.erase(Bundle.mNextActions.begin() + Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "next_action removed: " + Target + " (" +
                            Removed.mStatement + ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_statement", Removed.mStatement, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// next-action list
// ---------------------------------------------------------------------------

int RunNextActionListCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FNextActionListOptions Options = ParseNextActionListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::vector<FNextActionEntry> Filtered;
    std::vector<size_t> OriginalIndices;
    for (size_t I = 0; I < Bundle.mNextActions.size(); ++I)
    {
        const FNextActionEntry &A = Bundle.mNextActions[I];
        if (Options.opStatusFilter.has_value() &&
            A.mStatus != *Options.opStatusFilter)
            continue;
        Filtered.push_back(A);
        OriginalIndices.push_back(I);
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Filtered.size());
    EmitNextActionsJson("next_actions", Filtered, true, &OriginalIndices);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// acceptance-criterion add
// ---------------------------------------------------------------------------

int RunAcceptanceCriterionAddCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot)
{
    const FAcceptanceCriterionAddOptions Options =
        ParseAcceptanceCriterionAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FAcceptanceCriterionEntry Entry;
    Entry.mId = Options.mId;
    Entry.mStatement = Options.mStatement;
    if (Options.opStatus.has_value())
        Entry.mStatus = *Options.opStatus;
    Entry.mMeasure = Options.mMeasure;
    Entry.mEvidence = Options.mEvidence;

    Bundle.mMetadata.mAcceptanceCriteria.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mMetadata.mAcceptanceCriteria.size() - 1;
    const std::string Target =
        "acceptance_criteria[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "acceptance_criterion added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// ---------------------------------------------------------------------------
// acceptance-criterion set
// ---------------------------------------------------------------------------

int RunAcceptanceCriterionSetCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot)
{
    const FAcceptanceCriterionSetOptions Options =
        ParseAcceptanceCriterionSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mAcceptanceCriteria.size(),
                           "Acceptance-criterion"))
        return 1;

    FAcceptanceCriterionEntry &Entry =
        Bundle.mMetadata
            .mAcceptanceCriteria[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        "acceptance_criteria[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (!Options.mId.empty())
    {
        Changes.push_back({"id", {Entry.mId, Options.mId}});
        Entry.mId = Options.mId;
    }
    if (!Options.mStatement.empty())
    {
        Changes.push_back(
            {"statement", {Entry.mStatement, Options.mStatement}});
        Entry.mStatement = Options.mStatement;
    }
    if (Options.opStatus.has_value())
    {
        const ECriterionStatus NewStatus = *Options.opStatus;
        Changes.push_back(
            {"status", {ToString(Entry.mStatus), ToString(NewStatus)}});
        Entry.mStatus = NewStatus;
    }
    if (!Options.mMeasure.empty())
    {
        Changes.push_back({"measure", {Entry.mMeasure, Options.mMeasure}});
        Entry.mMeasure = Options.mMeasure;
    }
    if (!Options.mEvidence.empty())
    {
        Changes.push_back({"evidence", {Entry.mEvidence, Options.mEvidence}});
        Entry.mEvidence = Options.mEvidence;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// acceptance-criterion remove
// ---------------------------------------------------------------------------

int RunAcceptanceCriterionRemoveCommand(const std::vector<std::string> &InArgs,
                                        const std::string &InRepoRoot)
{
    const FAcceptanceCriterionRemoveOptions Options =
        ParseAcceptanceCriterionRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mAcceptanceCriteria.size(),
                           "Acceptance-criterion"))
        return 1;

    const std::string Target =
        "acceptance_criteria[" + std::to_string(Options.mIndex) + "]";
    const FAcceptanceCriterionEntry Removed =
        Bundle.mMetadata
            .mAcceptanceCriteria[static_cast<size_t>(Options.mIndex)];
    Bundle.mMetadata.mAcceptanceCriteria.erase(
        Bundle.mMetadata.mAcceptanceCriteria.begin() + Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "acceptance_criterion removed: " + Target + " (" +
                            Removed.mStatement + ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_statement", Removed.mStatement, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// acceptance-criterion list
// ---------------------------------------------------------------------------

int RunAcceptanceCriterionListCommand(const std::vector<std::string> &InArgs,
                                      const std::string &InRepoRoot)
{
    const FAcceptanceCriterionListOptions Options =
        ParseAcceptanceCriterionListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::vector<FAcceptanceCriterionEntry> Filtered;
    std::vector<size_t> OriginalIndices;
    for (size_t I = 0; I < Bundle.mMetadata.mAcceptanceCriteria.size(); ++I)
    {
        const FAcceptanceCriterionEntry &C =
            Bundle.mMetadata.mAcceptanceCriteria[I];
        if (Options.opStatusFilter.has_value() &&
            C.mStatus != *Options.opStatusFilter)
            continue;
        Filtered.push_back(C);
        OriginalIndices.push_back(I);
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Filtered.size());
    EmitAcceptanceCriteriaJson("acceptance_criteria", Filtered, true,
                               &OriginalIndices);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ===================================================================
// v0.98.0 — priority-grouping / runbook / residual-risk mutation
// runners. Same pattern as the v0.89.0 risk group: load bundle, mutate
// typed vector, AppendAutoChangelog, WriteBundleBack, emit JSON.
// ===================================================================

namespace
{

// Render a phase-indices vector as a "[0,1,2]" string for change logs.
std::string FormatPhaseIndices(const std::vector<int> &InIndices)
{
    std::string Out = "[";
    for (size_t I = 0; I < InIndices.size(); ++I)
    {
        if (I > 0)
            Out += ",";
        Out += std::to_string(InIndices[I]);
    }
    Out += "]";
    return Out;
}

// Render a commands vector as "[N commands]" for change logs. Avoids
// dumping the full shell text into the changelog line; the runbook
// entry itself carries the exact commands.
std::string FormatCommandCount(const std::vector<std::string> &InCommands)
{
    return "[" + std::to_string(InCommands.size()) + " cmd" +
           (InCommands.size() == 1 ? "" : "s") + "]";
}

} // namespace

// -----------------------------------------------------------------------
// priority-grouping add
// -----------------------------------------------------------------------

int RunPriorityGroupingAddCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InRepoRoot)
{
    const FPriorityGroupingAddOptions Options =
        ParsePriorityGroupingAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FPriorityGrouping Entry;
    Entry.mID = Options.mID;
    Entry.mPhaseIndices = Options.mPhaseIndices;
    Entry.mRule = Options.mRule;

    Bundle.mMetadata.mPriorityGroupings.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mMetadata.mPriorityGroupings.size() - 1;
    const std::string Target =
        "priority_groupings[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "priority_grouping added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// -----------------------------------------------------------------------
// priority-grouping set
// -----------------------------------------------------------------------

int RunPriorityGroupingSetCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InRepoRoot)
{
    const FPriorityGroupingSetOptions Options =
        ParsePriorityGroupingSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mPriorityGroupings.size(),
                           "Priority-grouping"))
        return 1;

    FPriorityGrouping &Entry = Bundle.mMetadata.mPriorityGroupings[static_cast<
        size_t>(Options.mIndex)];
    const std::string Target =
        "priority_groupings[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (!Options.mID.empty())
    {
        Changes.push_back({"id", {Entry.mID, Options.mID}});
        Entry.mID = Options.mID;
    }
    if (Options.mbPhaseIndicesSet)
    {
        Changes.push_back({"phase_indices",
                           {FormatPhaseIndices(Entry.mPhaseIndices),
                            FormatPhaseIndices(Options.mPhaseIndices)}});
        Entry.mPhaseIndices = Options.mPhaseIndices;
    }
    if (!Options.mRule.empty())
    {
        Changes.push_back({"rule", {Entry.mRule, Options.mRule}});
        Entry.mRule = Options.mRule;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// -----------------------------------------------------------------------
// priority-grouping remove
// -----------------------------------------------------------------------

int RunPriorityGroupingRemoveCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot)
{
    const FPriorityGroupingRemoveOptions Options =
        ParsePriorityGroupingRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mPriorityGroupings.size(),
                           "Priority-grouping"))
        return 1;

    const std::string Target =
        "priority_groupings[" + std::to_string(Options.mIndex) + "]";
    const FPriorityGrouping Removed = Bundle.mMetadata.mPriorityGroupings[static_cast<
        size_t>(Options.mIndex)];
    Bundle.mMetadata.mPriorityGroupings.erase(
        Bundle.mMetadata.mPriorityGroupings.begin() + Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "priority_grouping removed: " + Target + " (" +
                            Removed.mID + ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_id", Removed.mID, false);
    std::cout << "}\n";
    return 0;
}

// -----------------------------------------------------------------------
// priority-grouping list
// -----------------------------------------------------------------------

int RunPriorityGroupingListCommand(const std::vector<std::string> &InArgs,
                                   const std::string &InRepoRoot)
{
    const FPriorityGroupingListOptions Options =
        ParsePriorityGroupingListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Bundle.mMetadata.mPriorityGroupings.size());
    EmitPriorityGroupingsJson("priority_groupings",
                              Bundle.mMetadata.mPriorityGroupings);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// -----------------------------------------------------------------------
// runbook add
// -----------------------------------------------------------------------

int RunRunbookAddCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FRunbookAddOptions Options = ParseRunbookAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FRunbookProcedure Entry;
    Entry.mName = Options.mName;
    Entry.mTrigger = Options.mTrigger;
    Entry.mCommands = Options.mCommands;
    Entry.mDescription = Options.mDescription;

    Bundle.mMetadata.mRunbooks.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mMetadata.mRunbooks.size() - 1;
    const std::string Target = "runbooks[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan, "runbook added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// -----------------------------------------------------------------------
// runbook set
// -----------------------------------------------------------------------

int RunRunbookSetCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FRunbookSetOptions Options = ParseRunbookSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mMetadata.mRunbooks.size(),
                           "Runbook"))
        return 1;

    FRunbookProcedure &Entry =
        Bundle.mMetadata.mRunbooks[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        "runbooks[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (!Options.mName.empty())
    {
        Changes.push_back({"name", {Entry.mName, Options.mName}});
        Entry.mName = Options.mName;
    }
    if (!Options.mTrigger.empty())
    {
        Changes.push_back({"trigger", {Entry.mTrigger, Options.mTrigger}});
        Entry.mTrigger = Options.mTrigger;
    }
    if (Options.mbCommandsSet)
    {
        Changes.push_back({"commands",
                           {FormatCommandCount(Entry.mCommands),
                            FormatCommandCount(Options.mCommands)}});
        Entry.mCommands = Options.mCommands;
    }
    if (!Options.mDescription.empty())
    {
        Changes.push_back(
            {"description", {Entry.mDescription, Options.mDescription}});
        Entry.mDescription = Options.mDescription;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// -----------------------------------------------------------------------
// runbook remove
// -----------------------------------------------------------------------

int RunRunbookRemoveCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FRunbookRemoveOptions Options = ParseRunbookRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex, Bundle.mMetadata.mRunbooks.size(),
                           "Runbook"))
        return 1;

    const std::string Target =
        "runbooks[" + std::to_string(Options.mIndex) + "]";
    const FRunbookProcedure Removed =
        Bundle.mMetadata.mRunbooks[static_cast<size_t>(Options.mIndex)];
    Bundle.mMetadata.mRunbooks.erase(Bundle.mMetadata.mRunbooks.begin() +
                                     Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "runbook removed: " + Target + " (" + Removed.mName +
                            ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_name", Removed.mName, false);
    std::cout << "}\n";
    return 0;
}

// -----------------------------------------------------------------------
// runbook list
// -----------------------------------------------------------------------

int RunRunbookListCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FRunbookListOptions Options = ParseRunbookListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Bundle.mMetadata.mRunbooks.size());
    EmitRunbooksJson("runbooks", Bundle.mMetadata.mRunbooks);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// -----------------------------------------------------------------------
// residual-risk add
// -----------------------------------------------------------------------

int RunResidualRiskAddCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FResidualRiskAddOptions Options =
        ParseResidualRiskAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FResidualRiskEntry Entry;
    Entry.mArea = Options.mArea;
    Entry.mObservation = Options.mObservation;
    Entry.mWhyDeferred = Options.mWhyDeferred;
    Entry.mTargetPhase = Options.mTargetPhase;
    Entry.mRecordedDate = Options.mRecordedDate;
    Entry.mClosureSha = Options.mClosureSha;

    Bundle.mMetadata.mResidualRisks.push_back(std::move(Entry));
    const size_t NewIndex = Bundle.mMetadata.mResidualRisks.size() - 1;
    const std::string Target =
        "residual_risks[" + std::to_string(NewIndex) + "]";

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "residual_risk added: " + Target);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitArrayMutationAdded(Options.mTopic, Target, NewIndex);
    return 0;
}

// -----------------------------------------------------------------------
// residual-risk set
// -----------------------------------------------------------------------

int RunResidualRiskSetCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FResidualRiskSetOptions Options =
        ParseResidualRiskSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mResidualRisks.size(),
                           "Residual-risk"))
        return 1;

    FResidualRiskEntry &Entry = Bundle.mMetadata.mResidualRisks[static_cast<
        size_t>(Options.mIndex)];
    const std::string Target =
        "residual_risks[" + std::to_string(Options.mIndex) + "]";

    std::vector<Change> Changes;
    if (!Options.mArea.empty())
    {
        Changes.push_back({"area", {Entry.mArea, Options.mArea}});
        Entry.mArea = Options.mArea;
    }
    if (!Options.mObservation.empty())
    {
        Changes.push_back(
            {"observation", {Entry.mObservation, Options.mObservation}});
        Entry.mObservation = Options.mObservation;
    }
    if (!Options.mWhyDeferred.empty())
    {
        Changes.push_back(
            {"why_deferred", {Entry.mWhyDeferred, Options.mWhyDeferred}});
        Entry.mWhyDeferred = Options.mWhyDeferred;
    }
    if (!Options.mTargetPhase.empty())
    {
        Changes.push_back(
            {"target_phase", {Entry.mTargetPhase, Options.mTargetPhase}});
        Entry.mTargetPhase = Options.mTargetPhase;
    }
    if (!Options.mRecordedDate.empty())
    {
        Changes.push_back(
            {"recorded_date", {Entry.mRecordedDate, Options.mRecordedDate}});
        Entry.mRecordedDate = Options.mRecordedDate;
    }
    if (!Options.mClosureSha.empty())
    {
        Changes.push_back(
            {"closure_sha", {Entry.mClosureSha, Options.mClosureSha}});
        Entry.mClosureSha = Options.mClosureSha;
    }
    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// -----------------------------------------------------------------------
// residual-risk remove
// -----------------------------------------------------------------------

int RunResidualRiskRemoveCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot)
{
    const FResidualRiskRemoveOptions Options =
        ParseResidualRiskRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (!CheckIndexInRange(Options.mIndex,
                           Bundle.mMetadata.mResidualRisks.size(),
                           "Residual-risk"))
        return 1;

    const std::string Target =
        "residual_risks[" + std::to_string(Options.mIndex) + "]";
    const FResidualRiskEntry Removed = Bundle.mMetadata.mResidualRisks[static_cast<
        size_t>(Options.mIndex)];
    Bundle.mMetadata.mResidualRisks.erase(
        Bundle.mMetadata.mResidualRisks.begin() + Options.mIndex);

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "residual_risk removed: " + Target + " (" +
                            Removed.mArea + ")");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonField("removed_area", Removed.mArea, false);
    std::cout << "}\n";
    return 0;
}

// -----------------------------------------------------------------------
// residual-risk list
// -----------------------------------------------------------------------

int RunResidualRiskListCommand(const std::vector<std::string> &InArgs,
                               const std::string &InRepoRoot)
{
    const FResidualRiskListOptions Options =
        ParseResidualRiskListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, InRepoRoot);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldSizeT("count", Bundle.mMetadata.mResidualRisks.size());
    EmitResidualRisksJson("residual_risks", Bundle.mMetadata.mResidualRisks);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

} // namespace UniPlan
