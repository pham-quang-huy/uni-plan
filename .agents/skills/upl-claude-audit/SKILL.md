---
name: upl-Codex-audit
description: Audit the entire .Codex/ system for integrity and consistency. Use this skill to verify skills, hooks, rules, and settings.json are accurate, up-to-date, and consistent with AGENTS.md, CODING.md, and NAMING.md. Detects stale references, wrong conventions, missing files, frontmatter errors, and duplications.
implicit_invocation: true
---

# UPL Codex Audit

Use this skill to audit the `.Codex/` system for integrity, consistency, and accuracy.

## Scope

Audits all `.Codex/` artifacts:
- **Skills**: `.Codex/skills/*/SKILL.md` — frontmatter, referenced paths, build commands, conventions
- **Hooks**: `.Codex/hooks/*.sh` — JSON parsing patterns, exception files, exit codes
- **Rules**: `.Codex/rules/*.md` — consistency with CODING.md, NAMING.md, AGENTS.md
- **Settings**: `.Codex/settings.json` — all referenced hooks exist, valid JSON

## Audit Checklist

### 1. File Path Accuracy

For every file path referenced in skills and rules:
```bash
test -f "<path>" && echo "OK" || echo "MISSING: <path>"
```

### 2. Convention Consistency

Cross-reference against source-of-truth documents:
- `CODING.md` — formatting, memory ownership, SOLID principles
- `NAMING.md` — all prefix conventions
- `AGENTS.md` — project structure, skill registry, build commands

### 3. Command Accuracy

Verify all build commands and binary paths:
```bash
which uni-plan
uni-plan --version
./build.sh
```

### 4. Frontmatter Validation

Skills must have valid YAML frontmatter:
- Required fields: `name`, `description`, `implicit_invocation`

### 5. Hook Integrity

For each hook in `.Codex/hooks/`:
- Script is executable (`-x` permission)
- Script handles empty/malformed JSON gracefully
- PreToolUse hooks exit 0 (allow) or 2 (block)
- PostToolUse hooks always exit 0
- Referenced exception files/paths exist

### 6. Settings.json Integrity

```bash
jq . .Codex/settings.json > /dev/null
```

For each hook referenced in settings.json:
```bash
test -x ".Codex/hooks/<hookname>.sh" && echo "OK"
```

### 7. Cross-Reference Integrity

- Skills referencing other skills (e.g., `upl-plan-audit` in `upl-plan-execution`)
- Rules not contradicting each other
- No duplicated definitions across artifacts

### 8. No Foreign Repo References

```bash
rg -n '/Users/.*/code/|~/code/' .Codex/
```
Every hit must either point at the current repository or at `~/code/uni-plan`
when documenting the uni-plan CLI itself. No skill or rule may depend on a
different product repository's project-specific skills, namespaces, scripts, or
coding conventions.

### 9. Script Naming Convention

All scripts under `.Codex/` must use the `upl-` or `upl_` prefix.

### 10. Duplication and Collision Detection

- Same convention stated in multiple places — one source of truth should own it
- Conflicting definitions between artifacts
- Stale copies that drifted from source-of-truth docs

**Source-of-truth hierarchy**: `NAMING.md` > `CODING.md` > `AGENTS.md` > rules > skills

### 11. SKILL Length Hygiene (per `~/.Codex/rules/skill-length-discipline.md`)

Entry-point `SKILL.md` files should stay ≤ 300 lines; detail belongs in `references/*.md` per the rule. Scan:

```bash
find .Codex/skills .agents/skills -name SKILL.md -not -path "*/references/*" 2>/dev/null | \
  xargs -I {} bash -c 'lines=$(wc -l < "{}"); if [ "$lines" -gt 300 ]; then echo "$lines {}"; fi'
```

Each over-budget SKILL is a LOW finding (budget exceedance) or MEDIUM finding (if the bloat is `--help`-duplicating content — a second signal worth flagging). Check for `--help`-duplicating patterns inside each bloated SKILL:

```bash
# Markdown tables with Required / Options / Exit codes columns, or
# `Usage: <cli> <cmd>` code blocks that reproduce --help output.
grep -c -E "Usage:|Required:|Options:|Exit codes:" <skill>
```

Remediation: split into `references/command-map.md` (command reference) + `references/repair-recipes.md` (scenarios, workflows) + trimmed entry-point SKILL. The PreToolUse hook (`~/.claude/hooks/guard-skill-length.py` — Claude side) prevents net growth of over-budget SKILLs going forward — compaction passes are always allowed, so incremental refactor works without disabling the guard. Codex-side sessions: use the rule as discipline; the hook only runs under the Claude harness.

## Severity Levels

| Severity | Meaning | Example |
|----------|---------|---------|
| CRITICAL | Factually wrong, will cause errors | Referenced file doesn't exist, wrong function name |
| HIGH | Stale reference, outdated convention | Old file path, outdated naming example |
| MEDIUM | Inconsistency between artifacts | Rule says X, skill says Y |
| LOW | Incomplete coverage | New convention not covered by any rule |

## Output Format

| # | Severity | Artifact | Finding | Fix |
|---|----------|----------|---------|-----|
| 1 | CRITICAL | upl-guard-using-namespace.sh | Exception file doesn't exist | Update exception list |

### Verdict

- **PASS**: No CRITICAL or HIGH findings
- **NEEDS UPDATE**: HIGH findings only (functional but stale)
- **STALE**: CRITICAL findings — must fix before relying on system

## Rules

- Read-only by default
- Always check file existence before recommending
- Prefer concrete evidence over assumptions
- Report in table format
