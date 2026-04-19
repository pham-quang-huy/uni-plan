#pragma once

// ---------------------------------------------------------------------------
// UniPlanTypes.h — umbrella header.
//
// Historical god-header split by domain in v0.71.7. This umbrella preserves
// the single include point used by legacy call sites while exposing the
// decomposed sub-headers for callers that want tighter, narrower includes.
//
// Domain layout:
//   UniPlanCliConstants.h    — schemas, colors, sidecar extensions, human
//                              labels, canonical mutation targets
//   UniPlanOptionTypes.h     — BaseOptions + every F*Options struct +
//                              UsageError
//   UniPlanInventoryTypes.h  — V3 markdown inventory types (DocumentRecord,
//                              SidecarRecord, Inventory, etc) + DocConfig +
//                              IniData
//   UniPlanResultTypes.h     — per-command result/data structs
//                              (CacheInfoResult, PhaseItem, ValidateCheck...)
//
// Prefer including the focused header your TU actually needs.
// ---------------------------------------------------------------------------

#include "UniPlanCliConstants.h"   // IWYU pragma: export
#include "UniPlanEnums.h"          // IWYU pragma: export
#include "UniPlanInventoryTypes.h" // IWYU pragma: export
#include "UniPlanOptionTypes.h"    // IWYU pragma: export
#include "UniPlanResultTypes.h"    // IWYU pragma: export
#include "UniPlanTaxonomyTypes.h"  // IWYU pragma: export
