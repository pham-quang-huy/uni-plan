#include "UniPlanPhaseKind.h"

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Phase-kind predicates — the two signals every gate consumer reads.
// ---------------------------------------------------------------------------

bool IsCodeBearingPhase(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mCodeEntityContract.empty() ||
           !InPhase.mDesign.mCodeSnippets.empty();
}

bool IsGovernancePhase(const FPhaseRecord &InPhase)
{
    // The `mbNoFileManifest` boolean is a deliberate signal (set via
    // `phase set --no-file-manifest=true --no-file-manifest-reason`)
    // that the phase produces no files — governance, doc-only, or
    // taxonomy coordination work. The JSON deserializer enforces that
    // mFileManifestSkipReason is non-empty when the bool is set, so
    // the signal is self-justifying at load time.
    return InPhase.mbNoFileManifest;
}

// ---------------------------------------------------------------------------
// Tri-state status helpers.
// ---------------------------------------------------------------------------

const char *ToString(EReadinessGateStatus InStatus)
{
    switch (InStatus)
    {
        case EReadinessGateStatus::Pass:
            return "pass";
        case EReadinessGateStatus::Fail:
            return "fail";
        case EReadinessGateStatus::NotApplicable:
            return "not_applicable";
    }
    return "?";
}

EReadinessGateStatus
FPhaseReadinessGate::Evaluate(const FPhaseRecord &InPhase) const
{
    // A gate that does not apply to this phase kind reports
    // NotApplicable — never Fail. This is the mechanism that keeps
    // governance phases from being spuriously blocked by code-bearing
    // gate requirements.
    if (mAppliesCheck != nullptr && !mAppliesCheck(InPhase))
    {
        return EReadinessGateStatus::NotApplicable;
    }
    return mPassCheck(InPhase) ? EReadinessGateStatus::Pass
                               : EReadinessGateStatus::Fail;
}

// ---------------------------------------------------------------------------
// Pass predicates — one per registered gate.
// ---------------------------------------------------------------------------

static bool InvestigationPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mInvestigation.empty();
}

static bool CodeEntityContractPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mCodeEntityContract.empty();
}

static bool CodeSnippetsPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mCodeSnippets.empty();
}

static bool BestPracticesPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mBestPractices.empty();
}

static bool MultiPlatformingPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mDesign.mMultiPlatforming.empty();
}

static bool TestingPopulated(const FPhaseRecord &InPhase)
{
    return !InPhase.mTesting.empty();
}

// ---------------------------------------------------------------------------
// Applicability predicates — decide whether a gate applies to this
// phase's kind. Governance phases (mbNoFileManifest=true) opt out of
// code-bearing expectations.
// ---------------------------------------------------------------------------

static bool AppliesToAllPhases(const FPhaseRecord &)
{
    return true;
}

static bool AppliesToCodeBearingOrUnclassified(const FPhaseRecord &InPhase)
{
    // A gate that only applies to code-producing phases. Governance
    // phases (explicit opt-out) return NotApplicable; every other
    // phase must satisfy the gate. We do not require
    // `IsCodeBearingPhase` to be true because the gate's own pass
    // predicate is precisely what would make it code-bearing —
    // asking for CEC to be populated on phases that have not yet
    // been authored is the entire point of the gate.
    return !IsGovernancePhase(InPhase);
}

// ---------------------------------------------------------------------------
// Canonical registry. The order is stable and forms the public
// contract for the `gates` array emitted by `phase readiness`.
// ---------------------------------------------------------------------------

const std::vector<FPhaseReadinessGate> &GetPhaseReadinessGates()
{
    static const std::vector<FPhaseReadinessGate> kGates = {
        // Investigation is required on every phase — governance
        // decisions need justification just as much as code phases do.
        {"investigation", InvestigationPopulated, AppliesToAllPhases},

        // code_entity_contract + code_snippets are code-phase gates.
        // Governance phases opted out via mbNoFileManifest=true
        // report NotApplicable here, never Fail.
        {"code_entity_contract", CodeEntityContractPopulated,
         AppliesToCodeBearingOrUnclassified},
        {"code_snippets", CodeSnippetsPopulated,
         AppliesToCodeBearingOrUnclassified},

        // Best practices applies universally: framework decisions,
        // governance decisions, and code all need articulated rationale.
        {"best_practices", BestPracticesPopulated, AppliesToAllPhases},

        // Multi-platforming only matters when the phase ships code that
        // can diverge across OS targets. Governance / doc-only phases
        // have no platform dimension.
        {"multi_platforming", MultiPlatformingPopulated,
         AppliesToCodeBearingOrUnclassified},

        // Testing applies universally; governance phases validate via
        // `uni-plan validate --strict` + topic-query exercises, which
        // still count as authored testing rows.
        {"testing", TestingPopulated, AppliesToAllPhases},
    };
    return kGates;
}

bool AllReadinessGatesSatisfied(const FPhaseRecord &InPhase)
{
    for (const auto &Gate : GetPhaseReadinessGates())
    {
        // Only Fail blocks readiness. NotApplicable passes the
        // aggregate because the gate does not apply to this phase.
        if (Gate.Evaluate(InPhase) == EReadinessGateStatus::Fail)
        {
            return false;
        }
    }
    return true;
}

} // namespace UniPlan
