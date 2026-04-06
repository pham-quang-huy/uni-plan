---
name: upl-plan-creation
description: Create governance-compliant plan bundles with plan, implementation tracker, and detached sidecars. Use this skill when creating a new plan topic for uni-plan development, adding a plan for a new feature or CLI command, or establishing a new documentation bundle.
implicit_invocation: true
---

# UPL Plan Creation

Use this skill to create governed plan bundles for uni-plan's own development.

## Workflow

### Step 1: Investigation

1. **Duplicate check**: Run `uni-plan artifacts --topic <topic>` to verify no existing plan covers this scope
2. **Topic naming**: Use `TopicPascalCase` (e.g., `WatchModeRefactor`, `ValidationExpansion`)
3. **New feature checklist**:

| Check | Question |
|-------|----------|
| C1 | Does this affect validation checks (`Source/UniPlanValidation.cpp`)? |
| C2 | Does this affect schema files (`Schemas/*.Schema.md`)? |
| C3 | Does this affect watch mode (`Source/UniPlanWatch*.cpp`)? |
| C4 | Does this affect output formatters (JSON/text/human)? |
| C5 | Does this require a version bump? |
| C6 | Does this affect existing plan topics? |

### Step 2: Plan Authoring

Create `Docs/Plans/<TopicPascalCase>.Plan.md` following `Schemas/Plan.Schema.md`.

**Required sections** (from schema):

| Order | Section | Notes |
|-------|---------|-------|
| 1 | `section_menu` | Navigation table listing all H2 sections |
| 2 | `summary` | One-paragraph overview |
| 3 | `execution_strategy` | Must contain `### implementation_phases` (H3) |
| 4 | `risks_and_mitigations` | Known risks and mitigation strategies |
| 5 | `acceptance_criteria` | Definition-of-done criteria |
| 6 | `next_actions` | Prioritized upcoming steps |

### Step 3: Implementation Tracker

Create `Docs/Implementation/<TopicPascalCase>.Impl.md` following `Schemas/Implementation.Schema.md`.

**Required sections** (from schema):

| Order | Section | Notes |
|-------|---------|-------|
| 1 | `section_menu` | Navigation table |
| 2 | `linked_plan` | Path to plan |
| 3 | `progress_summary` | High-level status overview |
| 4 | `phase_tracking` | Columns: `Phase`, `Done`, `Remaining` |
| 5 | `change_log` | Reference to detached changelog |
| 6 | `next_actions` | Prioritized upcoming steps |
| 7 | `verification` | Evidence records or pointer to sidecar |

### Step 4: Sidecars (4 files)

| File | Location | Content |
|------|----------|---------|
| `<Topic>.Plan.ChangeLog.md` | `Docs/Plans/` | Plan change history |
| `<Topic>.Plan.Verification.md` | `Docs/Plans/` | Plan verification evidence |
| `<Topic>.Impl.ChangeLog.md` | `Docs/Implementation/` | Implementation change history |
| `<Topic>.Impl.Verification.md` | `Docs/Implementation/` | Implementation verification evidence |

Each sidecar starts with H1 + `## section_menu` + initial entry.

### Step 5: Playbook Authoring

When creating playbooks for plan phases, follow `Schemas/Playbook.Schema.md`.

**File**: `Docs/Playbooks/<TopicPascalCase>.<PhaseKey>.Playbook.md` + two sidecars

**Required sections** (from schema):

| Section | Content |
|---------|---------|
| `section_menu` | Navigation table |
| `linked_plan` | Table pointing to paired plan |
| `phase_binding` | Phase key, objective |
| `target_file_manifest` | Files to create/modify/remove |
| `linked_implementation` | Table pointing to paired impl tracker |
| `detached_evidence_documents` | Paths to playbook sidecars |
| `execution_lanes` | Lane table with Status, Scope, Exit Criteria |
| `handoff_points` | Outputs needed for next phase |
| `change_log` | Pointer to sidecar |
| `verification` | Pointer to sidecar |

### Step 6: Index Registration

Update `Docs/INDEX.md` — add new topic entry.

### Step 7: Validation

```bash
uni-plan lint
uni-plan artifacts --topic <topic>
uni-plan validate
```

Fix all findings before considering the plan bundle complete.

## Placement Rules

All plan documents go in the `Docs/` hierarchy:

| Doc Type | Location |
|----------|----------|
| Plans | `Docs/Plans/` |
| Implementation trackers | `Docs/Implementation/` |
| Playbooks + sidecars | `Docs/Playbooks/` |

## Rules

- Never create a plan without a paired implementation tracker
- Always include `section_menu` as the first section after H1
- Run `uni-plan lint` after creation to validate
- Use `uni-plan section schema --type <type>` to get the canonical schema
