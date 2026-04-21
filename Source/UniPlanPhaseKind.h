#pragma once

// ---------------------------------------------------------------------------
// UniPlanPhaseKind — centralized phase-kind predicates + readiness gate
// applicability registry.
//
// Historically the `phase readiness` / `phase next` / `phase get` /
// validator code paths each hard-coded their own copy of the
// "is this phase code-bearing?" + "which gates apply?" logic. That
// drift produced the governance-phase false-positive class: a phase
// opted out of file_manifest via `phase set --no-file-manifest=true`
// was still flagged `code_entity_contract: fail` by `phase readiness`
// because the readiness checker had no knowledge of the opt-out. The
// robust fix is a single-source-of-truth registry that every consumer
// iterates — adding a new gate or a new opt-out signal becomes one
// edit in this module, not an archeology sweep across the codebase.
//
// Added in v0.96.0.
// ---------------------------------------------------------------------------

#include "UniPlanTopicTypes.h"

#include <cstdint>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Phase-kind predicates.
// ---------------------------------------------------------------------------

/**
 * @brief True when the phase declares itself code-bearing via populated
 *        `code_entity_contract` OR `code_snippets`. This is the signal
 *        author code already uses, so we key on it instead of adding a
 *        new bool flag. Used by `file_manifest_required_for_code_phases`
 *        and `phase complete` mutation-time file-manifest gate.
 */
bool IsCodeBearingPhase(const FPhaseRecord &InPhase);

/**
 * @brief True when the phase is explicitly opted out of file-manifest
 *        gates via `mbNoFileManifest=true` plus a documented reason.
 *        Governance / taxonomy / coordination phases that produce no
 *        code files use this signal to avoid being treated as
 *        code-bearing by readiness / next / validator consumers.
 */
bool IsGovernancePhase(const FPhaseRecord &InPhase);

// ---------------------------------------------------------------------------
// Readiness gate applicability model.
// ---------------------------------------------------------------------------

/**
 * @brief Tri-state readiness-gate result. `phase readiness` emits one
 *        per registered gate; `ready` aggregates to true only when
 *        every gate is Pass or NotApplicable (never Fail).
 */
enum class EReadinessGateStatus : std::uint8_t
{
    Pass,
    Fail,
    NotApplicable,
};

const char *ToString(EReadinessGateStatus InStatus);

/**
 * @brief Binds a gate name to two predicates: one checks whether the
 *        phase satisfies the gate's content, one decides whether the
 *        gate applies to this phase kind at all. A gate that does not
 *        apply reports NotApplicable — never Fail — so governance
 *        phases do not spuriously block on code-bearing requirements.
 */
struct FPhaseReadinessGate
{
    const char *mName;
    bool (*mPassCheck)(const FPhaseRecord &);
    bool (*mAppliesCheck)(const FPhaseRecord &);

    EReadinessGateStatus Evaluate(const FPhaseRecord &InPhase) const;
};

/**
 * @brief Canonical readiness-gate registry. Iterated by `phase readiness`,
 *        `phase next`, and the drift-guard unit test. Order is stable
 *        and forms the public contract for the response array.
 */
const std::vector<FPhaseReadinessGate> &GetPhaseReadinessGates();

/**
 * @brief Aggregate across the registry: true when every gate is Pass
 *        or NotApplicable. Used by `phase next` readiness and by
 *        `phase readiness` summary.
 */
bool AllReadinessGatesSatisfied(const FPhaseRecord &InPhase);

} // namespace UniPlan
