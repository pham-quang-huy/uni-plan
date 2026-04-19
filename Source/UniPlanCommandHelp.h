#pragma once

#include <ostream>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Command help output (v0.85.0).
//
// `uni-plan <cmd> --help` and `uni-plan <cmd> <sub> --help` both route
// through this function. Every dispatcher calls it BEFORE option
// parsing so downstream AI agents always get a clean help block on
// `--help`, never an error path.
//
// InSubcommand == ""  — print the group-level block: one-liner +
//                       subcommand index. Fallback for single-command
//                       groups (validate, timeline, blockers, legacy-gap)
//                       is the entry-level leaf fields.
// InSubcommand != ""  — look up the matching FSubcommandHelpEntry inside
//                       the group's subcommand array and print the full
//                       leaf block. Falls through to the group block if
//                       the subcommand isn't registered yet (safe
//                       degradation during rollout).
// Command not in kCommandHelp — fall back to global PrintUsage().
//
// Always writes to stdout for success paths and is expected to be
// followed by `return 0;`. Callers must not re-raise UsageError for
// --help.
// ---------------------------------------------------------------------------
void PrintCommandUsage(std::ostream &Out, const std::string &InCommand,
                       const std::string &InSubcommand = "");

// Global usage overview. Stays in UniPlanCommandDispatch.cpp because it
// enumerates top-level commands and is coupled to the dispatch table,
// but declared here so help-wire call sites don't need to include the
// dispatcher's private signatures.
void PrintUsage(std::ostream &Out);

} // namespace UniPlan
