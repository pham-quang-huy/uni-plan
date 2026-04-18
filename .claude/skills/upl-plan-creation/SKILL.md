---
name: upl-plan-creation
description: Create V4 .Plan.json topic bundles. Use this skill when creating a new plan topic for uni-plan development, adding a plan for a new feature or CLI command, or establishing a new topic bundle.
implicit_invocation: true
---

# UPL Plan Creation

Use this skill to create governed V4 topic bundles for uni-plan's own development.

Use index-based entity references inside the bundle (`phases[n]`, `lanes[n]`, `waves[n]`, `jobs[n]`, `tasks[n]`). Legacy keys like `P0` belong only in quoted archival filenames or historical notes.

## Workflow

### Step 1: Investigation

1. **Duplicate check**: Run `uni-plan topic list --human` to verify no existing plan covers this scope
2. **Topic naming**: Use `TopicPascalCase` (e.g., `WatchModeRefactor`, `ValidationExpansion`)
3. **New feature checklist**:

| Check | Question |
|-------|----------|
| C1 | Does this affect validation evaluators (`Source/UniPlanValidation.cpp`)? |
| C2 | Does this affect watch mode (`Source/UniPlanWatch*.cpp`)? |
| C3 | Does this affect output formatters (JSON/text/human)? |
| C4 | Does this affect domain types (`Source/UniPlanTopicTypes.h`)? |
| C5 | Does this require a version bump? |
| C6 | Does this affect existing plan topics? |

### Step 2: Author the .Plan.json Bundle

Create `Docs/Plans/<TopicPascalCase>.Plan.json` with the V4 bundle schema.

**FTopicBundle structure**:

```json
{
  "$schema": "plan-v4",
  "topic": "TopicPascalCase",
  "status": "not_started",
  "title": "descriptive_snake_case_title",
  "summary": "Prose paragraph describing the plan goals and approach.",
  "goals": "Goal 1\nGoal 2\nGoal 3",
  "non_goals": "Non-goal 1\nNon-goal 2",
  "risks": "Risk | Impact | Mitigation (pipe-separated rows)",
  "acceptance_criteria": "AC-1 | Description\nAC-2 | Description",
  "problem_statement": "",
  "validation_commands": "",
  "baseline_audit": "",
  "execution_strategy": "",
  "locked_decisions": "",
  "source_references": "",
  "dependencies": "",
  "phases": [
    {
      "scope": "Phase 0 scope description",
      "output": "Expected deliverables",
      "status": "not_started",
      "done": "",
      "remaining": "",
      "blockers": "",
      "started_at": "",
      "completed_at": "",
      "agent_context": "",
      "lanes": [],
      "jobs": [],
      "testing": [],
      "file_manifest": [],
      "investigation": "",
      "code_snippets": "",
      "dependencies": "",
      "readiness_gate": "",
      "handoff": "",
      "code_entity_contract": "",
      "best_practices": "",
      "validation_commands": "",
      "multi_platforming": ""
    }
  ],
  "next_actions": "Prioritized next steps",
  "changelogs": [],
  "verifications": []
}
```

### Step 3: Register Initial Evidence

```bash
uni-plan changelog add --topic <topic> --change "Plan created" --type feat
uni-plan verification add --topic <topic> --check "Bundle validates" --result pass --detail "uni-plan validate passes"
```

### Optional: Extend with Additional Phases

Use `uni-plan phase add` to append trailing phases via CLI (auto-changelog, typed serializer) instead of hand-editing JSON:

```bash
uni-plan phase add --topic <topic> --scope "Phase scope" --output "Expected deliverables"
```

Default status is `not_started`. Follow with `uni-plan phase set` to populate design material fields (`--investigation`, `--readiness-gate`, `--handoff`, etc.).

### Step 4: Validation

```bash
uni-plan validate --topic <topic> --human
```

Fix all findings before considering the plan bundle complete.

## Placement Rules

| Doc Type | Location |
|----------|----------|
| V4 topic bundles | `Docs/Plans/<TopicPascalCase>.Plan.json` |

## Rules

- Always validate with `uni-plan validate --topic <topic>` after creation
- Summary field must be a prose paragraph (no pipe-table formatting)
- Phase indices are 0-based integers
- Status values: `not_started`, `in_progress`, `completed`, `blocked`, `canceled`
