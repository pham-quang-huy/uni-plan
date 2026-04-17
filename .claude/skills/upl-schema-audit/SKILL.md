---
name: upl-schema-audit
description: Audit Schemas/*.Schema.md files for consistency, completeness, and alignment with the C++ parser. Use this skill to verify schema files are valid, parseable, and correctly referenced by validation checks.
implicit_invocation: true
---

# UPL Schema Audit

**Note**: Schema.md files are V3 legacy artifacts. They are still read by `uni-plan lint` for markdown document filename pattern validation, but are NOT used by V4 bundle validation (`ValidateAllBundles`). V4 validation runs **28 evaluators** against `FTopicBundle` JSON data — 15 structural checks, 3 structural warnings, and 13 content-hygiene checks (regex-based prose scans for V3 drift, agent-safety hazards, format consistency, and cross-reference integrity).

Use this skill to audit the `Schemas/` directory for integrity and consistency (lint-only scope).

## Scope

Audits all 10 `Schemas/*.Schema.md` files:

| Schema File | Doc Type |
|-------------|----------|
| `Doc.Schema.md` | Base document structure |
| `Plan.Schema.md` | Plan documents |
| `Playbook.Schema.md` | Playbook documents |
| `Implementation.Schema.md` | Implementation trackers |
| `PlanChangeLog.Schema.md` | Plan change log sidecars |
| `PlanVerification.Schema.md` | Plan verification sidecars |
| `PlaybookChangeLog.Schema.md` | Playbook change log sidecars |
| `PlaybookVerification.Schema.md` | Playbook verification sidecars |
| `ImplChangeLog.Schema.md` | Implementation change log sidecars |
| `ImplVerification.Schema.md` | Implementation verification sidecars |

## Audit Checklist

### 1. File Presence

All 10 schema files must exist in `Schemas/`:

```bash
for f in Doc Plan Playbook Implementation PlanChangeLog PlanVerification PlaybookChangeLog PlaybookVerification ImplChangeLog ImplVerification; do
    test -f "Schemas/$f.Schema.md" && echo "OK: $f" || echo "MISSING: $f"
done
```

### 2. Canonical Section Order Table

Each schema must contain a `canonical_section_order` section with a markdown table in this format:

| Column | Required |
|--------|----------|
| Order | Yes — integer position |
| Section ID | Yes — snake_case section identifier |
| Requirement | Yes — `required` or `optional` |

Verify the table is parseable by checking:
- Table has header row + divider row + data rows
- All three columns are present
- Order values are sequential integers starting from 1
- Section IDs are snake_case
- Requirement values are either `required` or `optional`

### 3. Required Section Consistency

Cross-reference required sections across schemas:
- `section_menu` should be required in all doc type schemas
- Each schema's required sections should be a superset of `Doc.Schema.md` base requirements
- No duplicate section IDs within a single schema

### 4. C++ Parser Alignment

Verify schemas match what `BuildSectionSchemaEntries()` in `Source/UniPlanParsing.cpp` actually parses:
- Grep for `TryParseSectionSchemaFromFile` calls to find which schemas are loaded
- Verify the schema file paths match the actual `Schemas/` files
- Check that section IDs in schemas are referenced by validation checks in `Source/UniPlanValidation.cpp`

### 5. File Naming Pattern Consistency

Each schema defines a file naming pattern (e.g., `*.Plan.md`, `*.Impl.md`). Verify:
- Patterns don't overlap
- Patterns match the constants in `Source/UniPlanTypes.h` (`kExtPlan`, `kExtImpl`, `kExtPlaybook`)

## Output Format

| # | Severity | Schema | Finding | Fix |
|---|----------|--------|---------|-----|
| 1 | CRITICAL | Plan.Schema.md | Missing `canonical_section_order` table | Add table per format spec |

### Verdict

- **PASS**: All 10 schemas present, parseable, and consistent
- **NEEDS UPDATE**: Minor inconsistencies but functional
- **BROKEN**: Missing schemas or unparseable tables

## Rules

- Read-only audit — do not modify schema files unless user explicitly requests
- Always validate against the C++ parser, not just document format
- Report in table format
