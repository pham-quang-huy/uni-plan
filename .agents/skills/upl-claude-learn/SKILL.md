---
name: upl-Codex-learn
description: Learn from the current session and propose updates to the .Codex/ system. Use this skill after a productive session to capture new patterns, conventions, anti-patterns discovered, and rules violated. Proposes changes to skills, hooks, rules, and AGENTS.md for user review.
implicit_invocation: true
---

# UPL Codex Learn

Use this skill after a productive session to analyze what happened and propose improvements to the `.Codex/` system.

## Workflow

### 1. Session Analysis

Gather evidence from the current session:

```bash
git log --oneline -20
git diff HEAD~5..HEAD --stat
git diff HEAD~5..HEAD --name-only
```

Analyze the session for:
- **New coding patterns** established or validated
- **Anti-patterns** discovered and fixed
- **Convention changes** — any naming/formatting decisions made
- **Rules violated** — any guard hooks that fired or should have fired
- **Skills used** — which skills were invoked and their effectiveness
- **Missing coverage** — situations where a rule/hook/skill would have helped

### 2. Categorize Findings

| Category | Target | Requires |
|----------|--------|----------|
| **Lessons Learned** | Skill reference material | Append only |
| **Convention Update** | CODING.md or NAMING.md first, then cascade to rules | User approval |
| **Path Fix** | Any artifact with stale file paths | Auto-apply safe |
| **New Rule** | New `.Codex/rules/` file | User approval |
| **New Hook** | New `.Codex/hooks/` script + settings.json entry | User approval |
| **New Skill** | New `.Codex/skills/` directory | User approval |

### 3. Draft Proposals

For each finding, draft a specific proposal:

```markdown
### Proposal: <title>

**Category**: <category>
**Target**: <file path>
**Evidence**: <what happened in this session that prompted this>
**Change**: <specific diff or addition>
**Why**: <why this improves the system>
```

### 4. Present for Review

Present all proposals grouped by category. The user approves, modifies, or rejects each one.

### 5. Apply Approved Changes

Apply only user-approved changes. For each applied change:
- Update the target file
- If it's a convention change: cascade to all affected rules/skills
- If it's a new hook: update `.Codex/settings.json`

### 6. Verify

After applying changes, run `upl-Codex-audit` to verify system consistency.

## What to Learn From

### Code Changes
- New domain types introduced → update naming examples in rules
- New source files created → verify naming and placement conventions
- New validation checks added → update skill references

### Bug Fixes
- Root cause was a missing guard → propose new hook
- Root cause was convention violation → strengthen relevant rule
- Root cause was structural debt → add to refactor detection patterns

### Session Friction
- Had to explain a convention repeatedly → add to rules
- A skill was invoked but didn't cover the case → update skill
- A hook should have fired but didn't → update hook patterns

## Rules

- **Conservative**: Propose changes for review, never silently modify
- **Evidence-based**: Every proposal must cite session evidence
- **Cascading**: Convention changes cascade to all affected artifacts
- **Verify after**: Always run `upl-Codex-audit` after applying changes
- **No speculation**: Only propose changes based on actual session events, not hypotheticals
