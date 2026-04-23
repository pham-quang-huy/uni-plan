---
name: upl-validation-creation
description: Scaffold a new validation check for the uni-plan CLI. Use this skill when adding a new validation rule — guides you through adding the check ID, Evaluate function, wiring, and version bump.
implicit_invocation: true
---

# UPL Validation Creation

Use this skill to add a new validation check to the uni-plan CLI.

## Required Context

Before adding a check, read:
1. `Source/UniPlanValidation.cpp` — existing **28 evaluators** (15 structural + 3 warnings + 13 content-hygiene) and `ValidateAllBundles()`
2. `Source/UniPlanTypes.h` — `kCliVersion` and `ValidateCheck` struct

Note: inside C++ validator code, reading `FTopicBundle` fields directly is expected — that is the validator's job. The CLI-only rule binds authoring/audit skills that operate against `.Plan.json` externally, not the validator internals.
3. `Source/UniPlanTopicTypes.h` — `FTopicBundle`, `FPhaseRecord`, `FPhaseLifecycle`, `FPhaseDesignMaterial`
4. `Source/UniPlanEnums.h` — `EValidationSeverity` (ErrorMajor, ErrorMinor, Warning)
5. Existing check tiers — pick the right category for your new check:
   - **Structural** (ErrorMajor/ErrorMinor): field presence, index references, enum values, timestamp format
   - **Structural warnings**: advisory governance completeness checks
   - **Content-hygiene** (ErrorMinor/Warning): regex-scan prose fields for forbidden patterns, cross-reference integrity, duplicate detection

## Workflow

### Step 1: Design the Check

Define the check with:

| Field | Value |
|-------|-------|
| Check ID | Short snake_case identifier (e.g., `phase_scope_empty`, `no_smart_quotes`) |
| Severity | `ErrorMajor` (bundle broken), `ErrorMinor` (field violation), or `Warning` (advisory) |
| Tier | Structural / Structural warning / Content-hygiene |
| Scope | Which fields are scanned (topic metadata, phase design material, lanes, jobs, testing, changelogs) |

### Step 2: Add the Evaluator Function

In `Source/UniPlanValidation.cpp`, add a new evaluator. Pick the template that matches your check tier.

#### Template A — Structural check (access typed fields directly)

```cpp
static void Eval<CheckName>(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        // Iterate phases, jobs, tasks as needed.
        // Use the Fail() helper to record failures:
        Fail(OutChecks, "check_id", EValidationSeverity::ErrorMinor,
             B.mTopicKey, "path.to.field", "detail message");
    }
}
```

#### Template B — Content-hygiene check (regex-scan prose fields)

Use the three scan helpers introduced with the content-hygiene tier:

```cpp
static void Eval<CheckName>(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(R"(your-pattern-here)",
                                    std::regex_constants::icase);
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "check_id",
                       EValidationSeverity::Warning,
                       "detail prefix: ", OutChecks);
        ScanPhaseProse(B, Pattern, "check_id",
                       EValidationSeverity::Warning,
                       "detail prefix: ", OutChecks);
    }
}
```

`ScanPhaseProse` accepts two optional flags (`InIncludeLifecycle`, `InIncludeChildren`) to narrow the scan footprint. `ScanProseField` is the primitive — use it directly when you need field-specific scoping (e.g., scan only `validation_commands`).

#### Template C — Bundle-global integrity (cross-reference)

For checks that compare across bundles (e.g., topic-ref integrity, duplicate detection), follow the patterns in `EvalTopicRefIntegrity` and `EvalNoDuplicateChangelog`. Build a lookup set or map outside the per-bundle loop, then scan.

### Step 3: Wire Into ValidateAllBundles

In `ValidateAllBundles()`, add the call in the appropriate tier section:

```cpp
// ErrorMajor
EvalRequiredFields(InBundles, Checks);

// ErrorMinor
// ...

// Warning (structural)
EvalCanonicalEntityRef(InBundles, Checks);

// Content-hygiene (ErrorMinor + Warning)
Eval<YourNewCheck>(InBundles, Checks);
```

### Step 4: Write Unit Tests (MANDATORY)

Add two tests to `Test/UniPlanTestValidationContent.cpp` (content-hygiene) or relevant file (structural):

1. **Positive**: `CreateMinimalFixture`, inject offending content via `ReloadBundle` + field mutation + `WriteBundle`, run `RunBundleValidateCommand`, assert `FirstIssueWithId("check_id")` is non-empty, assert severity.
2. **Negative**: Same setup with clean content; assert `CountIssuesWithId("check_id") == 0`.

Test fixture helpers to reuse:
- `CountIssuesWithId(Json, "check_id")` — matches count
- `FirstIssueWithId(Json, "check_id")` — first match or empty
- `WriteBundle(mRepoRoot, "T", Bundle)` — persist mutated bundle

### Step 5: Version Bump

Bump `kCliVersion` MINOR in `Source/UniPlanTypes.h` (new validation check = new feature per SemVer table in CLAUDE.md).

### Step 6: Documentation

Update `CLAUDE.md` `validation_checks` section: add one row to the matching tier table with check ID, severity, and what it catches.

### Step 7: Build and Verify

```bash
./build.sh
./Build/CMake/uni-plan-tests --gtest_filter="*<CheckName>*"
uni-plan validate --strict --human
# Optional: validate an affected caller repo without hardcoding product repos.
uni-plan validate --strict --human --repo-root <path-to-repo>
```

Verify:
- Both positive and negative tests pass
- New check appears in validation output against real bundles
- `--strict` gates on the new check when severity is ErrorMinor or Warning
- Existing checks still pass

## Naming Convention

- Check IDs: `snake_case` — e.g., `required_fields`, `no_smart_quotes`, `topic_ref_integrity`
- Evaluator functions: `Eval` + PascalCase — e.g., `EvalRequiredFields`, `EvalNoSmartQuotes`
- Group by domain prefix:
  - Structural: `required_*`, `phase_*`, `job_*`, `task_*`, `changelog_*`, `verification_*`
  - Content-hygiene: `no_*` (forbidden pattern), `*_free` (absence assertion), `*_integrity` (cross-ref), `canonical_*` (shape assertion)

## Scan helper reference

| Helper | When to use |
|---|---|
| `ScanProseField(topic, path, content, pattern, id, severity, prefix, out)` | Scan one specific field — narrow scope (e.g., only `validation_commands`) |
| `ScanTopicProse(bundle, pattern, id, severity, prefix, out)` | Scan all 13 topic-level prose fields (summary, goals, risks, acceptance_criteria, etc.) |
| `ScanPhaseProse(bundle, pattern, id, severity, prefix, out, includeLifecycle=true, includeChildren=true)` | Scan all phase prose (scope/output + 9 design material + lifecycle + lanes + jobs) |

## --strict gate behavior

Without `--strict`, only `ErrorMajor` flips `valid=false`. With `--strict`, `ErrorMinor` and `Warning` also flip `valid=false` and the CLI exits 1. New checks automatically participate in this gate — no extra wiring needed.

## Rules

- **One check per evaluator function** — don't combine unrelated validations
- **Always include detail message** with actionable failure information (regex match fragment, expected form, offending value)
- **Use typed domain access** (`FTopicBundle`, `FPhaseRecord`) — never parse markdown
- **Always add positive + negative tests** — coverage matrix compliance
- **Always update `CLAUDE.md`** `validation_checks` section
- **Build-verify before considering the check complete**
- **Bump `kCliVersion` MINOR** for every new check
