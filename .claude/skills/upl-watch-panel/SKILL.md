---
name: upl-watch-panel
description: Add or modify watch mode TUI panels. Use this skill when creating a new panel class, modifying an existing panel's rendering, or adjusting the watch mode layout. Covers FTXUI element patterns and the panel architecture.
implicit_invocation: true
---

# UPL Watch Panel

Use this skill to add or modify panels in the watch mode TUI (`uni-plan watch`).

## Required Context

Before modifying panels, read:
1. `Source/UniPlanWatchPanels.h` — panel class declarations
2. `Source/UniPlanWatchPanels.cpp` — panel rendering implementations
3. `Source/UniPlanWatchApp.cpp` — layout orchestration and keyboard handling
4. `Source/UniPlanWatchSnapshot.h` — data structures fed to panels

For PHASE DETAIL metrics work, also read `Source/UniPlanPhaseMetrics.h/.cpp`.
Those values must stay shared with `uni-plan phase metric`; do not add a
watch-only metric calculation in panel code.

## Architecture

### Panel Class Pattern

Each panel is a thin class with a single `Render()` method:

```cpp
class <Name>Panel
{
public:
    ftxui::Element Render(const <DataType>& InData, /* optional indexes */) const;
};
```

Panels are stateless — they receive data and return an FTXUI element tree. State (selected indexes, toggle flags) lives in `UniPlanWatchApp.cpp`.

### Data Flow

```
BuildWatchSnapshot() → FDocWatchSnapshot → App selects relevant data → Panel::Render() → Element tree
```

PHASE DETAIL metric gauges use runtime-only data computed in
`ComputePhaseDepthMetrics()`. The `d` key toggles the metrics view.

### FTXUI Element Patterns

| Pattern | Usage |
|---------|-------|
| `vbox({...})` | Vertical stack of elements |
| `hbox({...})` | Horizontal row of elements |
| `window(title, content)` | Bordered box with title |
| `text("...") \| bold` | Styled text |
| `size(WIDTH, EQUAL, N)` | Fixed width constraint |
| `color(Color::Green)` | Color decoration |
| `separator()` | Horizontal line divider |

### Static Helpers

Key static helpers in `UniPlanWatchPanels.cpp`:
- `RenderSchemaBlock(title, FWatchDocSchemaResult)` — renders schema heading checks with color coding
- `RenderSidecarPanel(title, FWatchSidecarSummary*)` — renders sidecar summary (path, entries, date)
- `FindSidecar(FWatchPlanSummary, ownerKind, docKind)` — looks up sidecar by OwnerKind + DocKind

## Workflow

### Step 1: Define the Panel

Add the panel class declaration to `Source/UniPlanWatchPanels.h`:

```cpp
class <Name>Panel
{
public:
    ftxui::Element Render(const <DataType>& InData) const;
};
```

Choose the simplest signature that provides the data the panel needs.

### Step 2: Implement Render

Add the implementation to `Source/UniPlanWatchPanels.cpp`:

```cpp
Element <Name>Panel::Render(const <DataType>& InData) const
{
    if (/* no data */)
        return window(text(" <TITLE> ") | bold | dim, text("  No data") | dim);

    // Build content elements
    Elements Rows;
    // ... populate Rows ...

    return window(text(" <TITLE> ") | bold | color(Color::Cyan), vbox(Rows));
}
```

### Step 3: Wire Into Layout

In `Source/UniPlanWatchApp.cpp`:

1. Add panel instance alongside existing panels:
   ```cpp
   <Name>Panel Panel<Name>;
   ```

2. Add to the appropriate layout pane (main, schema, impl, etc.):
   ```cpp
   Panel<Name>.Render(SelectedPlan)
   ```

3. If the panel needs a keyboard toggle, add handling in the key event section.

### Step 4: Build and Verify

```bash
./build.sh  # must have UPLAN_WATCH=1 in CMake
uni-plan watch --repo <test-repo>
```

Verify:
- Panel renders correctly with data
- Panel shows empty/dim state when no data available
- Layout doesn't overflow at standard terminal widths (120+ columns)
- Keyboard navigation works if panel is interactive

## Layout Guidelines

- Schema pane panels: 60w per column, 2-column layout
- Impl pane panels: 60w, vertical stack
- Main pane: flexible width
- Use `size(WIDTH, EQUAL, N)` for consistent column widths
- Group related panels: Plan → Impl → Playbook ordering

## Rules

- Panels are stateless — no member variables, no side effects
- Build-verify with watch mode enabled
- Test with repos that have varying amounts of data (empty, partial, full)
- Follow existing panel patterns for consistent styling
