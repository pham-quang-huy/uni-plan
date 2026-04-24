---
name: upl-plan-creation
description: Create V4 .Plan.json topic bundles. Use this skill when creating a new plan topic for uni-plan development, adding a plan for a new feature or CLI command, or establishing a new topic bundle.
implicit_invocation: true
---

# UPL Plan Creation

**HARD RULE - CLI-only access to `.Plan.json`.** Never `json.load` / raw JSON parsing on bundle files. All reads and writes go through the `uni-plan` CLI. See `AGENTS.md` for the full rule.

**`--help` is the authoritative per-command reference (v0.85.0+).** Run `uni-plan <cmd> [<sub>] --help` for usage, required/optional flags, output schema, exit codes, and examples. Use it to discover the full flag surface for every command referenced below.

Use this skill to create governed V4 topic bundles for uni-plan's own development.

Use index-based entity references inside the bundle (`phases[n]`, `lanes[n]`, `waves[n]`, `jobs[n]`, `tasks[n]`). Legacy keys like `P0` belong only in quoted archival filenames or historical notes.

## Agentic Plan Handoff Standard

A `.Plan.json` is a delegated work package for a future AI agent or junior
developer, not a reminder list for the author. Write every topic, phase, lane,
job, and task so that a competent junior developer can execute it without
guessing architecture, ownership, or acceptance criteria.

For code-bearing work, load the uni-plan refactor baseline as the quality
reference before authoring design material:

- Codex/project agents: `/Users/huypham/code/uni-plan/.agents/skills/upl-code-refactor/SKILL.md`
- Claude agents: `/Users/huypham/code/uni-plan/.claude/skills/upl-code-refactor/SKILL.md`

Reflect that baseline directly in the bundle: name target files/modules, new or
changed `F`/`E`/`I` domain types, invariants, sequencing, validation commands,
and SOLID/refactor risks. Split phases so structural cleanup comes before
behavior changes; do not hide god structs, monolith files, string-keyed domain
state, raw primitive domain concepts, duplication, or workaround debt inside
vague tasks like "clean up code" or "implement feature".

Before handing off a newly authored phase, run
`uni-plan phase metric --topic <Topic> --phase <N>` and treat weak
`solid_words`, recursive words, field coverage, work items, tests, files, or
evidence as an authoring gap. These are runtime-only advisory signals; do not
add metric fields to `.Plan.json`.

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

### Step 2: Create The Bundle Via CLI (v0.94.0+)

Use `uni-plan topic add` to instantiate `Docs/Plans/<TopicPascalCase>.Plan.json`. The CLI writes through the typed serializer, auto-stamps a creation changelog, enforces PascalCase key regex at parse time, and refuses collisions. **Do not hand-write the JSON** - that path violates `hard_rule_cli_only` in `AGENTS.md`.

```bash
uni-plan topic add --topic <TopicPascalCase> --title "<descriptive title>" \
  [--summary-file <path>] [--goals-file <path>] \
  [--non-goals-file <path>] [--problem-statement-file <path>] \
  [--baseline-audit-file <path>] [--execution-strategy-file <path>] \
  [--locked-decisions-file <path>] [--source-references-file <path>]
```

Flags:
- `--topic <PascalCase>` - topic key, must match `^[A-Z][A-Za-z0-9]*$` (regex enforced at parse time, `UsageError` exit 2 on violation). Key also becomes the disk filename stem.
- `--title <text>` - required. Enforced by the `required_fields` ErrorMajor evaluator.
- All prose flags have `--<flag>-file <path>` siblings (read raw bytes, no shell expansion).

Exit codes: `0` bundle created; `1` collision (bundle already exists under repo root); `2` UsageError (bad key regex, missing `--topic`/`--title`).

### Step 3: Seed Phase 0

A fresh bundle has no phases and fails `uni-plan validate` with `phases_present` ErrorMajor - that is the expected governance signal. Seed the first phase next:

```bash
uni-plan phase add --topic <TopicPascalCase> \
  --scope-file <phase0-scope.txt> --output-file <phase0-output.txt>
```

Default status is `not_started`. Follow with `uni-plan phase set` to populate design material fields (`--investigation`, `--readiness-gate`, `--handoff`, etc.).

### Step 4: Register Initial Evidence

The creation changelog is auto-stamped by `topic add`. Add a verification entry manually:

```bash
uni-plan verification add --topic <topic> --check "Bundle validates" \
  --result pass --detail "uni-plan validate passes after Phase 0 seed"
```

### Optional: Extend With Additional Phases

Call `uni-plan phase add` again for each trailing phase. Auto-changelog + typed serializer handle the rest.

### Step 5: Validation

```bash
uni-plan validate --topic <topic> --human
uni-plan phase metric --topic <topic> --human
python3 .agents/hooks/plan_snippet_antipattern.py --topic <topic> --all --strict
```

Fix all findings before considering the plan bundle complete. The final
hook is a snippet anti-pattern lint that scans `code_snippets`,
`code_entity_contract`, and `best_practices` for stringly-typed
if-chains (3+ arms), enum switches with 7+ case arms, stringly-typed
handler args, raw `new F<Name>` without a smart-pointer factory, and
`goto`. Negative examples under an `## Anti-Pattern ...` heading or
prefixed with `// BAD:` are skipped. See
`.agents/rules/upl-plan-snippet-discipline.md`.

### Optional: Normalize Smart Characters

If design material was pasted from documents (word processors, web pages, chat), em-dashes, curly quotes, and NBSP may sneak in. Sweep a phase before validating:

```bash
uni-plan phase normalize --topic <topic> --phase <N> [--dry-run]
```

This replaces em/en/figure dashes with `-`, smart quotes with straight quotes, and NBSP with regular space - keeps the bundle clean against `no_smart_quotes`.

## Placement Rules

| Doc Type | Location |
|----------|----------|
| V4 topic bundles | `Docs/Plans/<TopicPascalCase>.Plan.json` |

## Rules

- Always validate with `uni-plan validate --topic <topic>` after creation
- Summary field must be a prose paragraph (no pipe-table formatting)
- Phase indices are 0-based integers
- Status values: `not_started`, `in_progress`, `completed`, `blocked`, `canceled`
