#include "UniPlanPhaseMetrics.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace UniPlan
{

static void AppendText(std::vector<std::string> &OutTexts,
                       const std::string &InText)
{
    if (!InText.empty())
    {
        OutTexts.push_back(InText);
    }
}

static std::vector<std::string> TokenizeLower(const std::string &InText)
{
    std::vector<std::string> Tokens;
    std::string Token;

    const auto Flush = [&]()
    {
        if (!Token.empty())
        {
            Tokens.push_back(Token);
            Token.clear();
        }
    };

    for (const char Character : InText)
    {
        const unsigned char Byte = static_cast<unsigned char>(Character);
        if (std::isalnum(Byte))
        {
            Token.push_back(static_cast<char>(std::tolower(Byte)));
        }
        else
        {
            Flush();
        }
    }
    Flush();
    return Tokens;
}

static std::string JoinTextCorpus(const std::vector<std::string> &InTexts)
{
    std::string Text;
    for (const std::string &Part : InTexts)
    {
        if (!Text.empty())
        {
            Text.push_back(' ');
        }
        Text += Part;
    }
    return Text;
}

static size_t CountWords(const std::vector<std::string> &InTexts)
{
    size_t Count = 0;
    for (const std::string &Text : InTexts)
    {
        Count += TokenizeLower(Text).size();
    }
    return Count;
}

static const std::vector<std::vector<std::string>> &GetSolidPhrases()
{
    static const std::vector<std::vector<std::string>> Phrases = {
        {"single", "source", "of", "truth"},
        {"single", "responsibility"},
        {"open", "closed", "principle"},
        {"open", "closed"},
        {"liskov", "substitution"},
        {"interface", "segregation"},
        {"dependency", "inversion"},
        {"dependency", "injection"},
        {"separation", "of", "concerns"},
        {"domain", "boundary"},
        {"typed", "domain"},
        {"domain", "model"},
        {"value", "type"},
        {"raw", "primitive"},
        {"string", "enum"},
        {"god", "struct"},
        {"monolith", "file"},
        {"structural", "cleanup"},
        {"behavior", "change"},
        {"source", "of", "truth"},
        {"ownership", "boundary"},
        {"module", "boundary"},
        {"public", "contract"},
        {"typed", "contract"},
        {"pure", "virtual"},
        {"no", "coupling"},
        {"low", "coupling"},
        {"high", "cohesion"}};
    return Phrases;
}

static const std::unordered_set<std::string> &GetSolidTerms()
{
    static const std::unordered_set<std::string> Terms = {
        "solid",      "cohesion",    "coupling",   "abstraction",
        "contract",   "interface",   "invariant",  "encapsulation",
        "ownership",  "lifetime",    "boundary",   "typed",
        "enum",       "struct",      "domain",     "refactor",
        "validation", "guardrail",   "immutable",  "polymorphism",
        "modular",    "module",      "dependency", "dependencies",
        "decoupled",  "composition", "adapter",    "registry",
        "facade",     "isolation"};
    return Terms;
}

static bool MatchesPhraseAt(const std::vector<std::string> &InTokens,
                            const std::vector<std::string> &InPhrase,
                            const size_t InOffset)
{
    if (InOffset + InPhrase.size() > InTokens.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < InPhrase.size(); ++Index)
    {
        if (InTokens[InOffset + Index] != InPhrase[Index])
        {
            return false;
        }
    }
    return true;
}

static bool AnyClaimed(const std::vector<bool> &InClaimed, size_t InOffset,
                       size_t InCount)
{
    for (size_t Index = 0; Index < InCount; ++Index)
    {
        if (InClaimed[InOffset + Index])
        {
            return true;
        }
    }
    return false;
}

static size_t CountSolidWords(const std::vector<std::string> &InTexts)
{
    const std::vector<std::string> Tokens =
        TokenizeLower(JoinTextCorpus(InTexts));
    std::vector<bool> Claimed(Tokens.size(), false);
    size_t Count = 0;

    for (const std::vector<std::string> &Phrase : GetSolidPhrases())
    {
        if (Phrase.empty())
        {
            continue;
        }
        for (size_t Offset = 0; Offset + Phrase.size() <= Tokens.size();
             ++Offset)
        {
            if (!MatchesPhraseAt(Tokens, Phrase, Offset))
            {
                continue;
            }
            if (AnyClaimed(Claimed, Offset, Phrase.size()))
            {
                continue;
            }
            for (size_t Index = 0; Index < Phrase.size(); ++Index)
            {
                Claimed[Offset + Index] = true;
            }
            Count += Phrase.size();
        }
    }

    const std::unordered_set<std::string> &Terms = GetSolidTerms();
    for (size_t Index = 0; Index < Tokens.size(); ++Index)
    {
        if (!Claimed[Index] && Terms.count(Tokens[Index]) > 0)
        {
            Count++;
        }
    }
    return Count;
}

static void AddPhaseTextCorpus(const FTopicBundle &InBundle,
                               size_t InPhaseIndex,
                               std::vector<std::string> &OutTexts)
{
    const FPhaseRecord &Phase = InBundle.mPhases[InPhaseIndex];
    AppendText(OutTexts, Phase.mScope);
    AppendText(OutTexts, Phase.mOutput);
    AppendText(OutTexts, Phase.mLifecycle.mDone);
    AppendText(OutTexts, Phase.mLifecycle.mRemaining);
    AppendText(OutTexts, Phase.mLifecycle.mBlockers);
    AppendText(OutTexts, Phase.mLifecycle.mAgentContext);

    AppendText(OutTexts, Phase.mDesign.mInvestigation);
    AppendText(OutTexts, Phase.mDesign.mCodeEntityContract);
    AppendText(OutTexts, Phase.mDesign.mCodeSnippets);
    AppendText(OutTexts, Phase.mDesign.mBestPractices);
    AppendText(OutTexts, Phase.mDesign.mMultiPlatforming);
    AppendText(OutTexts, Phase.mDesign.mReadinessGate);
    AppendText(OutTexts, Phase.mDesign.mHandoff);
    for (const FBundleReference &Reference : Phase.mDesign.mDependencies)
    {
        AppendText(OutTexts, Reference.mNote);
    }
    for (const FValidationCommand &Command : Phase.mDesign.mValidationCommands)
    {
        AppendText(OutTexts, Command.mCommand);
        AppendText(OutTexts, Command.mDescription);
    }

    for (const FLaneRecord &Lane : Phase.mLanes)
    {
        AppendText(OutTexts, Lane.mScope);
        AppendText(OutTexts, Lane.mExitCriteria);
    }
    for (const FJobRecord &Job : Phase.mJobs)
    {
        AppendText(OutTexts, Job.mScope);
        AppendText(OutTexts, Job.mOutput);
        AppendText(OutTexts, Job.mExitCriteria);
        for (const FTaskRecord &Task : Job.mTasks)
        {
            AppendText(OutTexts, Task.mDescription);
            AppendText(OutTexts, Task.mEvidence);
            AppendText(OutTexts, Task.mNotes);
        }
    }
    for (const FTestingRecord &Test : Phase.mTesting)
    {
        AppendText(OutTexts, Test.mSession);
        AppendText(OutTexts, Test.mStep);
        AppendText(OutTexts, Test.mAction);
        AppendText(OutTexts, Test.mExpected);
        AppendText(OutTexts, Test.mEvidence);
    }
    for (const FFileManifestItem &Item : Phase.mFileManifest)
    {
        AppendText(OutTexts, Item.mDescription);
    }
    AppendText(OutTexts, Phase.mFileManifestSkipReason);

    for (const FChangeLogEntry &Entry : InBundle.mChangeLogs)
    {
        if (Entry.mPhase == static_cast<int>(InPhaseIndex))
        {
            AppendText(OutTexts, Entry.mChange);
            AppendText(OutTexts, Entry.mAffected);
        }
    }
    for (const FVerificationEntry &Entry : InBundle.mVerifications)
    {
        if (Entry.mPhase == static_cast<int>(InPhaseIndex))
        {
            AppendText(OutTexts, Entry.mCheck);
            AppendText(OutTexts, Entry.mResult);
            AppendText(OutTexts, Entry.mDetail);
        }
    }
}

static bool
HasValidationCommandText(const std::vector<FValidationCommand> &InCommands)
{
    for (const FValidationCommand &Command : InCommands)
    {
        if (!Command.mCommand.empty() || !Command.mDescription.empty())
        {
            return true;
        }
    }
    return false;
}

static bool HasDependencyText(const std::vector<FBundleReference> &InRefs)
{
    for (const FBundleReference &Reference : InRefs)
    {
        if (!Reference.mTopic.empty() || !Reference.mPath.empty() ||
            !Reference.mNote.empty())
        {
            return true;
        }
    }
    return false;
}

static size_t CountAuthoredFieldBuckets(const FTopicBundle &InBundle,
                                        size_t InPhaseIndex,
                                        const size_t InTaskCount,
                                        const size_t InChangelogCount,
                                        const size_t InVerificationCount)
{
    const FPhaseRecord &Phase = InBundle.mPhases[InPhaseIndex];
    size_t Count = 0;

    const auto AddIf = [&Count](const bool InCondition)
    {
        if (InCondition)
        {
            Count++;
        }
    };

    AddIf(!Phase.mScope.empty());
    AddIf(!Phase.mOutput.empty());
    AddIf(!Phase.mLifecycle.mDone.empty());
    AddIf(!Phase.mLifecycle.mRemaining.empty());
    AddIf(!Phase.mLifecycle.mBlockers.empty());
    AddIf(!Phase.mLifecycle.mAgentContext.empty());
    AddIf(!Phase.mDesign.mInvestigation.empty());
    AddIf(!Phase.mDesign.mCodeEntityContract.empty());
    AddIf(!Phase.mDesign.mCodeSnippets.empty());
    AddIf(!Phase.mDesign.mBestPractices.empty());
    AddIf(!Phase.mDesign.mMultiPlatforming.empty());
    AddIf(!Phase.mDesign.mReadinessGate.empty());
    AddIf(!Phase.mDesign.mHandoff.empty());
    AddIf(HasDependencyText(Phase.mDesign.mDependencies));
    AddIf(HasValidationCommandText(Phase.mDesign.mValidationCommands));
    AddIf(!Phase.mLanes.empty());
    AddIf(!Phase.mJobs.empty());
    AddIf(InTaskCount > 0);
    AddIf(!Phase.mTesting.empty());
    AddIf(!Phase.mFileManifest.empty() ||
          (Phase.mbNoFileManifest && !Phase.mFileManifestSkipReason.empty()));
    AddIf(InChangelogCount > 0);
    AddIf(InVerificationCount > 0);

    return Count;
}

FPhaseRuntimeMetrics ComputePhaseDepthMetrics(const FTopicBundle &InBundle,
                                              size_t InPhaseIndex)
{
    FPhaseRuntimeMetrics Metrics;
    if (InPhaseIndex >= InBundle.mPhases.size())
    {
        return Metrics;
    }

    const FPhaseRecord &Phase = InBundle.mPhases[InPhaseIndex];
    Metrics.mDesignChars = ComputePhaseDesignChars(Phase);
    Metrics.mLaneCount = Phase.mLanes.size();
    Metrics.mJobCount = Phase.mJobs.size();
    Metrics.mTestingRecordCount = Phase.mTesting.size();
    Metrics.mFileManifestCount = Phase.mFileManifest.size();

    for (const FJobRecord &Job : Phase.mJobs)
    {
        Metrics.mTaskCount += Job.mTasks.size();
        for (const FTaskRecord &Task : Job.mTasks)
        {
            if (!Task.mEvidence.empty())
            {
                Metrics.mEvidenceItemCount++;
            }
        }
    }
    for (const FTestingRecord &Test : Phase.mTesting)
    {
        if (!Test.mEvidence.empty())
        {
            Metrics.mEvidenceItemCount++;
        }
    }
    for (const FChangeLogEntry &Entry : InBundle.mChangeLogs)
    {
        if (Entry.mPhase == static_cast<int>(InPhaseIndex))
        {
            Metrics.mChangelogCount++;
            Metrics.mEvidenceItemCount++;
        }
    }
    for (const FVerificationEntry &Entry : InBundle.mVerifications)
    {
        if (Entry.mPhase == static_cast<int>(InPhaseIndex))
        {
            Metrics.mVerificationCount++;
            Metrics.mEvidenceItemCount++;
        }
    }

    Metrics.mWorkItemCount =
        Metrics.mLaneCount + Metrics.mJobCount + Metrics.mTaskCount;
    Metrics.mAuthoredFieldCount = CountAuthoredFieldBuckets(
        InBundle, InPhaseIndex, Metrics.mTaskCount, Metrics.mChangelogCount,
        Metrics.mVerificationCount);
    Metrics.mFieldCoveragePercent =
        Metrics.mAuthoredFieldTotal == 0
            ? 0
            : (Metrics.mAuthoredFieldCount * 100) / Metrics.mAuthoredFieldTotal;

    std::vector<std::string> Texts;
    AddPhaseTextCorpus(InBundle, InPhaseIndex, Texts);
    Metrics.mRecursiveWordCount = CountWords(Texts);
    Metrics.mSolidWordCount = CountSolidWords(Texts);

    return Metrics;
}

} // namespace UniPlan
