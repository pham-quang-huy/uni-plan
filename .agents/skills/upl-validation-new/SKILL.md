---
name: upl-validation-new
description: Scaffold a new validation check for the uni-plan CLI. Use this skill when adding a new validation rule — guides you through adding the check ID, Evaluate function, wiring, and version bump.
implicit_invocation: true
---

# UPL Validation New

Use this skill to add a new validation check to the uni-plan CLI.

## Required Context

Before adding a check, read:
1. `Source/UniPlanValidation.cpp` — existing 18 evaluators and `ValidateAllBundles()`
2. `Source/UniPlanTypes.h` — `kCliVersion` and `ValidateCheck` struct
3. `Source/UniPlanTopicTypes.h` — `FTopicBundle`, `FPhaseRecord`, `FPhaseLifecycle`, `FPhaseDesignMaterial`
4. `Source/UniPlanEnums.h` — `EValidationSeverity` (ErrorMajor, ErrorMinor, Warning)

## Workflow

### Step 1: Design the Check

Define the check with:

| Field | Value |
|-------|-------|
| Check ID | Short snake_case identifier (e.g., `phase_scope_empty`, `changelog_date_format`) |
| Severity | `ErrorMajor` (bundle broken), `ErrorMinor` (field violation), or `Warning` (advisory) |
| Description | Human-readable explanation of what it validates |

### Step 2: Add the Evaluator Function

In `Source/UniPlanValidation.cpp`, add a new evaluator following existing patterns:

```cpp
static void Eval<CheckName>(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        // Validation logic — iterate phases, jobs, tasks as needed
        // Use the Fail() helper to record failures:
        Fail(OutChecks, "check_id", EValidationSeverity::ErrorMinor,
             B.mTopicKey, "path.to.field", "detail message");
    }
}
```

Key patterns from existing evaluators:
- Access plan metadata: `B.mMetadata.mTitle`, `B.mMetadata.mSummary`
- Access phase lifecycle: `Phase.mLifecycle.mStatus`, `Phase.mLifecycle.mDone`
- Access phase design: `Phase.mDesign.mInvestigation`, `Phase.mDesign.mCodeEntityContract`
- Access jobs/tasks: `Phase.mJobs[J].mTasks[T]`
- Use `Fail()` helper for consistent error formatting
- Use `IsValidISODate()` / `IsValidISOTimestamp()` for date validation

### Step 3: Wire Into ValidateAllBundles

In `ValidateAllBundles()`, add the call in the appropriate severity section:

```cpp
// ErrorMajor
Eval<CheckName>(InBundles, Checks);

// ErrorMinor
Eval<CheckName>(InBundles, Checks);

// Warning
Eval<CheckName>(InBundles, Checks);
```

### Step 4: Version Bump

Bump `kCliVersion` MINOR in `Source/UniPlanTypes.h` (new validation check = new feature).

### Step 5: Build and Verify

```bash
./build.sh
cd ~/code/FourImmortalsEngine && uni-plan validate --human
```

Verify:
- New check appears in validation output
- Check fires correctly on real bundle data
- Existing checks are not affected

## Naming Convention

- Check IDs: `snake_case` — e.g., `required_fields`, `phase_scope_empty`
- Evaluator functions: `Eval` + PascalCase — e.g., `EvalRequiredFields`, `EvalPhaseScope`
- Group by domain prefix: `required_*`, `phase_*`, `job_*`, `task_*`, `changelog_*`, `verification_*`

## Rules

- One check per evaluator function — don't combine unrelated validations
- Always include detail message with actionable failure information
- Use typed domain access (FTopicBundle, FPhaseRecord) — never parse markdown
- Build-verify before considering the check complete
