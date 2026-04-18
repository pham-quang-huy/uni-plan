# verification_schema

> **DEPRECATED — V3 legacy markdown schema.**
>
> This document describes a retired markdown artifact format used by
> `uni-plan lint` for backward-compatibility filename checking only.
> **It is NOT canonical authoring guidance.** The canonical V4 contract
> is the `.Plan.json` topic bundle (`$schema: plan-v4`) under
> `Docs/Plans/`, which carries all phases, changelogs, and verifications
> inline — there are no separate plan/implementation/playbook/sidecar
> files in V4. See the V4 bundle model in the owning repository's
> `AGENTS.md`/`CLAUDE.md`.

## lint_scope

`uni-plan lint` consumes only these two rules from this schema:

| Rule | Value |
| --- | --- |
| Allowed filename pattern | `<TopicPascalCase>.<Slot>.Verification.md` |
| First non-empty line | Must be an H1 (`# ...`) |

No other field-level schema content is consumed by any runtime tool. All
V3 section/table/lifecycle content previously documented here has been
retired; do not author new documents against the retired structure.
