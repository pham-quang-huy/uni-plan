# uni-plan Improvement Proposal: Dense-Plan Authoring Ergonomics For AI Agents

## Context

Proposal authored 2026-04-24 by an AI coding agent (Claude) operating in the
Square Bot repo (`~/code/square-bot`) while authoring the `MegaScan` topic
bundle end-to-end — 11 phases, ~381,000 chars of plan design content,
~500 individual `uni-plan` CLI invocations across topic / phase / lane /
job / task / testing / manifest / changelog / validate / readiness /
metric surfaces. The Square Bot repo applies a stricter authoring floor
(`design_chars >= 30000` code-bearing, `>= 20000` doc) plus an enhanced
`plan_board_hygiene.py` hook on top of native `uni-plan validate --strict`.

The proposals below are grounded in concrete pain observed in that
session, not speculative. Each item names the exact workflow that hit
the pain, the observed token / wall-clock cost, and a sketched interface
change. Estimated impact is labelled per proposal.

This file is a proposal, not a governed plan. It lives under
`Docs/Proposals/` per the author's suggestion. Promote to
`Docs/Plans/<Topic>.Plan.json` via `uni-plan topic add` if any proposal
is accepted for implementation.

## P1 — Quiet mutation responses (`--ack-only`)

### Observed pain

Every mutation (`topic set`, `phase set`, `lane add`, `job add`,
`task add`, `changelog add`, etc.) returns a response of shape:

```json
{"schema":"uni-plan-mutation-v1","ok":true,"topic":"<T>",
 "target":"phases[N]",
 "changes":[
   {"field":"investigation","old":"<FULL PRIOR CONTENT>","new":"<FULL NEW CONTENT>"},
   ...
 ],
 "auto_changelog":true}
```

For a single `phase set` that replaces `investigation` (9k chars) +
`code_entity_contract` (8k) + `code_snippets` (7k) + `best_practices` (10k)
+ `multi_platforming` (3k) + `readiness_gate` (2k) + `handoff` (2k) +
`validation_commands` (1k), the response echoes every prior field AND
every new field — ~84k chars of response for ~42k chars of incoming data.

Across 11 phases with typical 2-3 revisions per phase (linter scrubs,
hygiene-driven density bumps, sort-key correction sweep), I estimate the
session's transcript carried **~1.5-2M chars of pure mutation-echo noise**.
Every agent invocation downstream had to either truncate (losing context)
or pay the token cost.

### Sketch

Add an opt-in flag on every mutation command:

```bash
uni-plan phase set --topic T --phase N --investigation-file X.md --ack-only
```

Response shape when `--ack-only` is set:

```json
{"schema":"uni-plan-mutation-v1","ok":true,"topic":"T",
 "target":"phases[N]","changed_fields":["investigation"],
 "auto_changelog":true}
```

No `old` / `new` echo. Exit code unchanged. All validators and lock
semantics unchanged. The caller who needs the old content can fetch it
via `phase get` before the mutation (and caches it if they want to).

Optional variant: a repo-wide config toggle (`uni-plan.ini`) that makes
`--ack-only` the default, with `--verbose` to restore echo for humans.

### Estimated impact

Token savings per mutation-heavy session: **~30-40%** of total
transcript. Wall clock: negligible (mostly serialization bytes, not
work). Risk: very low; the data was always derivable from
`phase get` before the call.

### Priority

**HIGH** — single biggest AI-agent session cost observed.

## P2 — Hygiene profiles on `validate`

### Observed pain

Square Bot defines two authoring profiles (code-bearing at 30k chars,
doc at 20k chars) on top of uni-plan's native `hollow/thin/rich`
buckets. The enhanced gate lives in a separate Python hook:

```bash
python3 .claude/hooks/plan_board_hygiene.py \
    --topic MegaScan --phase 3 \
    --min-design-chars 30000 \
    --min-solid-words 80 \
    --min-recursive-words 3600 \
    --min-field-coverage-percent 75 \
    --min-evidence-items 5 \
    --require-human-testing \
    --require-automation-testing \
    --require-validation-commands \
    --forbid-dangling-modify-manifest
```

That's ~200 chars of flag boilerplate per invocation, run 11 times for
this topic, plus ~30 re-runs after density touch-ups = ~8kb of CLI
string noise per session. Every flag is also a separate shell-parse
cost + separate subprocess fork.

The hook is a subprocess that re-parses the entire bundle. `uni-plan
validate` already parses the bundle; running both doubles the parse
cost.

### Sketch

Introduce named profiles in `uni-plan.ini`:

```ini
[hygiene_profile.code_bearing]
min_design_chars = 30000
min_solid_words = 80
min_recursive_words = 3600
min_field_coverage_percent = 75
min_evidence_items = 5
require_human_testing = true
require_automation_testing = true
require_validation_commands = true
forbid_dangling_modify_manifest = true

[hygiene_profile.doc]
min_design_chars = 20000
min_solid_words = 24
min_recursive_words = 900
min_field_coverage_percent = 75
min_evidence_items = 5
require_human_testing = true
require_automation_testing = true
require_validation_commands = true
forbid_dangling_modify_manifest = true
```

Then:

```bash
uni-plan validate --topic MegaScan --phase 3 --hygiene-profile code_bearing
```

And a `--auto-profile` mode that picks code-bearing vs doc based on
`file_manifest_required_for_code_phases` evaluator output (or the
phase's `no_file_manifest` flag).

Batch mode:

```bash
uni-plan validate --topic MegaScan --all-phases --auto-profile
```

Returns per-phase PASS/FAIL with a single JSON array, one bundle parse
total.

### Estimated impact

Token savings: ~150 chars × ~40 invocations = ~6kb per dense session.
Wall clock: measurable (one bundle parse instead of N). Most valuable:
the hygiene gate becomes discoverable to agents who do not yet know
about the local hook.

### Priority

**HIGH** — rolls repo-specific floors into a first-class concept.

## P3 — Write-time validator parity

### Observed pain

`uni-plan validate` emits warnings for:

- `no_smart_quotes` (unicode em-dash, curly quotes)
- `no_dev_absolute_path` (literal `/Users/<name>/`)
- `no_duplicate_changelog` (exact-text-duplicate changelog entries)

All three fire at `validate` time, which means the offending content is
ALREADY in the bundle. For AI agents, this means:

1. Agent writes scratch file containing em-dash (comes naturally from
   Markdown writing habits, linter auto-insertions, linter fix
   side-effects).
2. Agent applies via `phase set --investigation-file`. Success.
3. Agent runs `validate` 10+ calls later. Warning appears.
4. Agent scrubs scratch, re-applies via `phase set`, runs `validate`
   again.

Cost of the full round-trip: 2 extra `phase set` calls + 2 extra
`validate` calls + the mutation-echo noise described in P1 =
~50kb of pure round-trip waste across the session.

### Sketch

Move the same validators to the mutation path. When `phase set
--investigation-file X.md` detects an em-dash or dev-absolute-path in
the incoming content, reject with:

```json
{"schema":"uni-plan-mutation-v1","ok":false,
 "error_code":"content_hygiene_violation",
 "detail":"unicode smart char '—' at offset 1234; use ' - ' instead",
 "offending_file":"X.md","offending_field":"investigation"}
```

Exit code 3 (new, distinct from 1=collision and 2=UsageError). The
caller fixes the scratch file and retries — one round-trip instead of
four.

For `no_duplicate_changelog`, the auto-changelog path could dedupe
adjacent identical entries silently (idempotent re-applies stop
producing warnings).

### Estimated impact

Token savings: ~5-15% per session depending on how often the agent
accidentally includes an em-dash. Wall clock: positive — avoids the
scrub-and-reapply cycle. Risk: need a bypass (`--allow-smart-quotes`)
for content that legitimately contains em-dashes (historical doc
quotations).

### Priority

**MEDIUM-HIGH** — biggest "papercut" class.

## P4 — Narrow field readers (`phase get --field <name>`)

### Observed pain

To patch one paragraph in `phases[0].investigation`, the agent must:

```bash
uni-plan phase get --topic MegaScan --phase 0 --design
```

Response: all seven design fields (investigation, code_entity_contract,
code_snippets, best_practices, multi_platforming, readiness_gate,
handoff) — typically 30-50kb of JSON per call.

When doing a sweep to patch a specific phrase across multiple phases,
this means ~500kb of response for information where only ~5kb was
actually needed.

### Sketch

```bash
uni-plan phase get --topic T --phase N --field investigation
```

Response:

```json
{"schema":"uni-plan-phase-field-v1","topic":"T","phase_index":N,
 "field":"investigation","chars":9477,"value":"<content>"}
```

Or for multiple fields:

```bash
uni-plan phase get --topic T --phase N --fields "investigation,handoff"
```

Complement with `--fields-list` (boolean) that returns just the names
of non-empty fields:

```json
{"schema":"uni-plan-phase-fields-list-v1","phase_index":N,
 "non_empty_fields":["investigation","code_entity_contract","..."]}
```

### Estimated impact

Token savings on patch-heavy sessions: ~15-25% when doing targeted
corrections. Wall clock: neutral. Enables smarter caching strategies
(agent can confirm which fields are worth pulling).

### Priority

**MEDIUM** — very useful but less total volume than P1/P2.

## P5 — Append / substring mutations

### Observed pain

Every `--*-file` flag on `phase set` is a full replace. To append one
paragraph to an existing `investigation` (9k chars), the agent must:

1. Pull current content via `phase get --design`.
2. Write pulled content + new paragraph to a scratch file.
3. `phase set --investigation-file <scratch>`.

That's 9kb pulled + 9kb pushed for a 1kb semantic addition.

### Sketch

Append mode:

```bash
uni-plan phase set --topic T --phase N --investigation-append-file extra.md
```

Behavior: read existing `investigation`, concatenate with file contents
(auto-insert blank line at seam), write result.

Substring replace:

```bash
uni-plan phase set --topic T --phase N --investigation-replace-substring-file patch.json
```

Where `patch.json` is `{"from":"...","to":"..."}` with exact-match
semantics. Exit 1 on zero-match or multi-match ambiguity.

Both variants compute the new content server-side; no pull-cycle
required.

### Estimated impact

Token savings on iterative authoring: ~30-50% during the "add a small
section" operations. Wall clock: positive. Risk: append semantics need
careful handling of trailing whitespace / newlines.

### Priority

**MEDIUM** — high impact when it applies, but applies less often than
P1.

## P6 — Idempotent mutations do not spam changelog

### Observed pain

After applying a sort-key canonical-correction sweep that touched many
fields twice (once via initial edit, once after a linter scrubbed
em-dashes), `uni-plan validate` emitted 6+ `no_duplicate_changelog`
warnings for adjacent identical entries like `phases[4] updated:
investigation, code_entity_contract, code_snippets, best_practices,
multi_platforming, readiness_gate, handoff`.

### Sketch

Auto-changelog dedupe logic: if the new entry's `change` string exactly
matches the immediately-previous auto-generated entry for the same
target, append a monotone counter to the existing entry's change text
(e.g. `phases[4] updated: investigation (×2)`) instead of emitting a
new changelog row.

OR: opt-out via `--no-auto-changelog` flag on mutation commands when
the author knows the mutation is an idempotent retry.

### Estimated impact

Reduces `validate` noise. Low token impact per session but improves
signal-to-noise on the warning list.

### Priority

**LOW** — cosmetic, but polishes the validate output.

## P7 — Topic-wide sweep commands

### Observed pain

Per-phase operations required shell loops:

```bash
for N in 0 1 2 3 4 5 6 7 8 9 10; do
  uni-plan phase readiness --topic MegaScan --phase $N 2>&1 | tail -1 | ...
done
```

Every phase iteration forks a uni-plan process (~30-50ms cold).

### Sketch

Batch readiness:

```bash
uni-plan phase readiness --topic MegaScan --all-phases
```

Returns one JSON array with `phase_index` + `ready` + `gates` per phase
— one process fork instead of 11.

Same pattern for `phase metric --all-phases`, `phase get --all-phases
--field <name>`.

### Estimated impact

Wall clock: noticeable on dense-plan sessions (saves ~300-500ms
cumulative per sweep). Token: mildly positive because JSON arrays are
more compact than N response envelopes.

### Priority

**MEDIUM** — quality-of-life win.

## P8 — Task description mutability

### Observed pain

Documented in Square Bot's `cli-gap-discipline.md`:

> `uni-plan task set` does not rewrite `description`; only `status`,
> `evidence`, `notes`.

Workaround: `task remove` + `task add`, which destroys audit history.
Acceptable pre-evidence; surprising post-evidence.

### Sketch

Allow `task set --description <text>` with a gate: if the task has
non-empty `evidence` or status other than `not_started`, require
`--force` + `--reason <text>`. Record the description change in
changelog with before/after.

### Estimated impact

Low token; high correctness. The current workaround is used rarely but
is high-stakes when it fires.

### Priority

**LOW** — not hit this session, but documented pain class.

## Proposal summary (token-impact ranking)

| # | Proposal | Est. session token savings | Est. session wall clock | Priority |
|---|---|---|---|---|
| P1 | `--ack-only` mutation responses | 30-40% | neutral | HIGH |
| P2 | Hygiene profiles on `validate` + `--all-phases` | 5-10% | positive | HIGH |
| P3 | Write-time validators mirror validate-time | 5-15% | positive | MEDIUM-HIGH |
| P4 | `phase get --field <name>` narrow reader | 15-25% (patch sessions) | neutral | MEDIUM |
| P5 | `--*-append-file` + substring replace | 30-50% (when applies) | positive | MEDIUM |
| P6 | Auto-changelog dedupe | ~1% | neutral | LOW |
| P7 | `--all-phases` batch sweeps | 2-5% | positive | MEDIUM |
| P8 | `task set --description` with guard | negligible | neutral | LOW |

## Non-goals of this proposal

- No changes to the bundle file format on disk. Every proposal here is
  additive on the CLI surface; existing `.Plan.json` bundles remain
  readable by earlier `uni-plan` versions (forward-compatible with
  `--ack-only` as opt-in).
- No changes to the existing lock-and-stale-check concurrency model
  (v0.99.0+). It is already correct for the observed workloads.
- No new persistence of hygiene profiles into the bundle. Profiles live
  in `uni-plan.ini` and are consumed at query time.
- No deprecation of any existing flag. All proposals are purely
  additive.

## Pointers to reproduction material

If the uni-plan maintainer wants to reproduce the session that drove
these observations, the relevant artifacts are:

- The resulting topic bundle: `~/code/square-bot/Docs/Plans/MegaScan.Plan.json`
- Square Bot commit introducing it: `62a6fea docs: Add MegaScan Plan And Raise Dense Authoring Threshold`
- Square Bot's enhanced hygiene hook: `~/code/square-bot/.claude/hooks/plan_board_hygiene.py`
- Square Bot's authoring skill (docs the agent followed): `~/code/square-bot/.claude/skills/square-bot-plan-creation/SKILL.md`

## Meta

This proposal is itself a candidate for promotion to a governed topic
(`uni-plan topic add --topic CliAgentErgonomics ...`) if the maintainer
wants to execute any subset. At that point this file becomes the
topic's initial `source_references` content and each proposal becomes
one phase. The proposal file can then be retired or kept as the
narrative companion to the typed bundle.
