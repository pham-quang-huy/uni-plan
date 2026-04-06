---
name: upl-validation-new
description: Scaffold a new validation check for the uni-plan CLI. Use this skill when adding a new validation rule — guides you through adding the check ID, Evaluate function, wiring, and version bump.
implicit_invocation: true
---

# UPL Validation New

Use this skill to add a new validation check to the uni-plan CLI.

## Required Context

Before adding a check, read:
1. `Source/UniPlanValidation.cpp` — existing checks and patterns
2. `Source/UniPlanTypes.h` — `kCliVersion` and validation-related types
3. `Schemas/*.Schema.md` — if the check validates against a schema

## Workflow

### Step 1: Design the Check

Define the check with:

| Field | Value |
|-------|-------|
| Check ID | Short snake_case identifier (e.g., `playbook_blank_sections`) |
| Description | Human-readable explanation of what it validates |
| Severity | `critical` or `warning` |
| Doc types affected | Which document types this check applies to |

### Step 2: Add the Evaluate Function

In `Source/UniPlanValidation.cpp`, add a new `Evaluate*` function following existing patterns:

```cpp
static void Evaluate<CheckName>(
    const Inventory& InInventory,
    const std::string& InRepoRoot,
    std::vector<ValidateCheck>& OutChecks)
{
    ValidateCheck Check;
    Check.mCheckID = "<check_id>";
    Check.mDescription = "<description>";
    // ... validation logic ...
    Check.mbPassed = /* result */;
    OutChecks.push_back(Check);
}
```

Key patterns from existing code:
- Use `BuildSectionSchemaEntries()` for schema-driven checks
- Use `TryReadFileLines()` from `UniPlanHelpers.h` for file reading
- Use `SplitMarkdownTableRow()` for table parsing
- Set `Check.mDetails` with specific failure information

### Step 3: Wire Into Runner

Find the main validation runner function in `UniPlanValidation.cpp` and add the call:

```cpp
Evaluate<CheckName>(InInventory, InRepoRoot, OutChecks);
```

### Step 4: Version Bump

If this adds a new check (increasing the total check count):
- Bump `kCliVersion` MINOR in `Source/UniPlanTypes.h`
- Update the check count in `CLAUDE.md` if documented there

### Step 5: Build and Verify

```bash
./build.sh
uni-plan validate --repo <test-repo>
```

Verify:
- New check appears in validation output
- Check passes on conforming documents
- Check fails (with helpful details) on non-conforming documents
- Existing checks are not affected

## Naming Convention

- Check IDs: `snake_case` — e.g., `plan_required_sections`, `playbook_heading_naming`
- Evaluate functions: `Evaluate` + PascalCase — e.g., `EvaluatePlanRequiredSections`
- Group by doc type prefix: `plan_*`, `playbook_*`, `impl_*`, `doc_*`

## Rules

- One check per `Evaluate*` function — don't combine unrelated validations
- Always include `mDetails` with actionable failure information
- Use dynamic schema resolution (`BuildSectionSchemaEntries`) — never hardcode section lists
- Build-verify before considering the check complete
