---
name: upl-commit
description: Stage and commit changes with proper formatting and safety checks. Use this skill when the user requests a commit. Validates commit message format, prevents accidental staging of sensitive files, and enforces SemVer discipline for CLI version bumps.
implicit_invocation: true
---

# UPL Commit

Use this skill when the user explicitly requests a commit.

## Pre-Commit Checks

1. **Never auto-commit** — only when user explicitly requests
2. **Never `git add -A`** or `git add .` — stage specific files by name
3. **Never stage sensitive files**: `.env`, credentials, tokens
4. **Never amend** unless user explicitly asks — always create new commits
5. **Never push** unless user explicitly asks
6. **Never revert changes** — do not unstage, restore, or discard any modifications
7. **Session-scoped commits only** — only stage files changed in the current session

## Commit Message Format

```
type: Subject in Title Case
- Bullet point describing key change
- Another bullet point
```

**Types**: `feat`, `fix`, `chore`, `docs`, `refactor`, `test`, `perf`

**Rules**:
- Subject line: imperative mood, title case, under 72 characters
- `feat` = wholly new feature
- `fix` = bug fix
- `docs` = documentation only
- `refactor` = structural change, no behavior change
- `chore` = build, tooling, config changes

## Workflow

### 1. Review Changes

```bash
git status
git diff --staged
git diff
git log --oneline -5
```

### 2. Classify and Stage Files

Separate changes into three categories:

- **Session files**: files you modified in this session — stage these
- **Pre-existing staged**: files already staged before this session — leave as-is (do not unstage)
- **Other modified**: files changed in prior sessions or by other tools — **do not stage**; report them to the user after the commit

Stage only session-related files by explicit path.

### 3. Draft Message

Analyze staged changes and draft a commit message following the format above.

### 4. Commit

Use HEREDOC for proper formatting:
```bash
git commit -m "$(cat <<'EOF'
type: Subject in Title Case
- Key change description

Co-Authored-By: Codex Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

## SemVer Gate

Before committing, check if any staged files are CLI trigger files:

| Version source | Trigger files |
|---------------|---------------|
| `Source/UniPlanTypes.h` (`kCliVersion`) | All `Source/*.cpp` and `Source/*.h` |

**If any trigger file is staged AND `kCliVersion` was not bumped in the diff**:
1. Stop before committing
2. Warn the user: "CLI source files changed but kCliVersion was not bumped. MAJOR for breaking changes, MINOR for new features/options, PATCH for backward-compatible fixes."
3. Bump the version, rebuild, and verify before committing

## Safety Checks

- Verify no `.env` files are staged
- Verify no binary files are accidentally staged
- Verify commit message matches the type/subject format
- Verify CLI SemVer gate (see above)
- If pre-commit hook fails: fix the issue, re-stage, create a NEW commit (never amend)

## Post-Commit Report

After a successful commit, notify the user about remaining unstaged changes:

```
Remaining modified files (not part of this commit):
- <file1> (from prior session)
- <file2> (from prior session)
```
