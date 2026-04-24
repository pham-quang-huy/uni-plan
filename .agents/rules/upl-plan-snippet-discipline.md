# uni-plan Plan Snippet Discipline

When authoring `code_snippets`, `code_entity_contract`, or `best_practices`
fields in a `.Plan.json` phase, the same anti-patterns that `CODING.md` and
the per-repo coding-principles rule forbid in real source code also apply to
the design-material C/C++ snippets. A snippet that "demonstrates" a rejected
pattern without marking it as such trains later executors to reproduce the
pattern.

## Core rule

Snippets in design-material fields must be written as if they were about to
be merged into the repo. They go through the same anti-pattern checks as
source files. The canonical traps for plan authoring:

1. **No stringly-typed if-chains.** A chain of `if (X == "literal") return Y;`
   arms that grows past three is an `NO IF/ELSE HELL` violation from
   `CODING.md` rule 5. The accepted shape is a registration table
   (`std::unordered_map<std::string_view, T>` for static mappings, or a
   small `std::array<FEntry>` for composites with a typed resolver pointer).
2. **No large enum switches with duplicated bodies.** A `switch` whose case
   arms differ only by which pointer they dereference is the canonical
   registration-table target. Case count >= 7 triggers the guard regardless
   of body shape.
3. **No stringly-typed handler args.** Handlers take typed `F<Command>Args`
   structs, not `std::map<std::string, std::string>`.
4. **No raw ownership.** `std::make_unique` for owned heap objects,
   `std::make_shared` only where shared ownership is genuinely required.
5. **No `bHasX + mX` flag pairs.** Use `std::optional<T>`.
6. **No `std::regex` on hot paths.** Handwritten walkers or precompiled
   indexes replace it.
7. **No goto.** Structured control flow only.

## Documented negative examples are allowed

A snippet that deliberately shows a rejected pattern for pedagogical or
regression-guard purposes is permitted, and the lint must skip it, when
either condition holds:

- The enclosing markdown section heading contains "Anti-Pattern" or
  "Rejected" (case-insensitive).
- The first non-blank line inside the code block is `// BAD:` or
  `// REJECTED:` (case-insensitive).

## Rationalization is not a waiver

The best-practices field must not contain sentences like "for this phase,
an N-arm switch is acceptable" or "this is fine for now". If a real design
requires a shape that looks like a violation, the design is wrong and
needs to land as a registration-table equivalent.

## Mechanical enforcement

`python3 .claude/hooks/plan_snippet_antipattern.py --topic <T> --phase <N>`
runs the lint against a single phase's design material. Add `--all` to scan
every phase in the topic. `--strict` exits 1 on any finding.

The lint is invoked at three gates: plan creation (`upl-plan-creation` Exit
Check), plan audit (`upl-plan-audit` layers), and plan execution
(`upl-plan-execution` pre-`phase start`). All three skills should fail fast
on lint findings rather than claim the phase ready.

