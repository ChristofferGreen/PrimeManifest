# UI Render Pipeline Plan (PrimeFrame)

## Scope
- This document describes PrimeFrame: layout, rect hierarchy, event routing, focus, and integration with PrimeManifest.
- It excludes PrimeStage application logic and PrimeManifest renderer internals.

## Library Layers
- `PrimeManifest`: low-level renderer that draws SDF shapes and text to a framebuffer.
- `PrimeFrame`: layout + rect hierarchy system that builds `RenderBatch`, runs layout, and produces optimized render data.
- `PrimeStage`: ground-truth UI system (window manager, widget data, state) that emits initial rect hierarchies for `PrimeFrame`.
  - `PrimeStage` is not yet implemented.
  - These live in separate git repos to enforce separation of concerns (helps both humans and AI agents).
- `PrimeStage` stores its ground truth in `PathSpace`, a hierarchical, thread-safe data store that supports insert/take/wait operations and lambda-based multithreaded execution.
  - `PathSpace` can mount filesystems or other PathSpace instances (including distributed clients) under the same API surface.
  - Enables unified access patterns for assets and multiplayer/shared state.
- `PrimeHost`: operating system integration (window creation, IO) that feeds input into PrimeStage's ground truth.

### Responsibilities
- `PrimeManifest`
  - Render SDF shapes and text to a framebuffer.
  - Optimize render batches and execute optimized draws.
- `PrimeFrame`
  - Maintain rect hierarchy and layout passes.
  - Handle event propagation, focus, hit testing.
  - Produce `RenderBatch` + `OptimizedBatch` for `PrimeManifest`.
- `PrimeStage`
  - Own ground-truth state, widget data, and window management.
  - Emit rect hierarchies for `PrimeFrame` and handle business logic/state updates.
- `PrimeHost`
  - Handle OS IO, window creation, and input dispatch into PrimeStage's ground truth.

## Goals
- Provide a low-latency path for UI and large dynamic scenes using the existing 2D renderer + optimizer.
- Keep data ownership clear and avoid shared mutable state across threads.
- Allow reuse of cached optimized data when UI does not change.
- Support large dynamic scenes (e.g., ~250k items) with predictable per-frame costs.

## Non-Goals (for now)
- Incremental optimizer updates (partial re-binning).
- Renderer consuming only optimized output with no access to batch data.
- Cross-process or serialized render packets.

## Current Renderer Constraints
- `RenderOptimized` still needs `RenderBatch` for geometry, glyphs, palette, and clear data.
- `OptimizedBatch` is a derived cache + tile binning, not a full render payload.
- Tile binning/streaming is required for rendering.

## Proposed High-Level Flow
1. Build or update a `RenderBatch` from UI/widget tree + dynamic scene sources.
2. Run `OptimizeRenderBatch` to produce/update `OptimizedBatch`.
3. Call `RenderOptimized` with both `RenderBatch` and `OptimizedBatch`.
4. If no changes since last frame, skip optimization and reuse cached `OptimizedBatch`.

## Ownership Model
- The renderer uses read-only data for the duration of a frame.
- Avoid shared mutable ownership of live simulation/UI data.
- Prefer a single-thread frame build or double-buffered SoA if multi-threaded.

## Data Preparation Strategy
- Pre-allocate `RenderBatch` stores to worst-case sizes to avoid per-frame allocations.
- Keep stable indices for items whenever possible.
- Update only the fields that change (e.g., positions, opacity), leave static fields untouched.
- UI state is owned by ground-truth data; tree view rows are rebuilt from ground truth.
- Ephemeral UI state (e.g., live edit buffers, hover/drag) may live in callbacks; essential state must be committed to ground truth.
- Edit buffers may be temporary while focused; on commit (blur/Enter/accept), callbacks push updates to ground truth.

## Data Model Notes
- Nodes are stored as AoS with a stable `NodeId` handle (index + generation).
- Roots are `std::vector<NodeId>` to allow reordering without invalidating IDs.
- Callbacks are stored out-of-line; nodes hold a callback handle or index into a callback table.
- Each node may own multiple render primitives (rect + optional text or image per primitive).
- Layout outputs are stored separately per node (absolute rect, clip rect, visibility, local order).
- Layout uses float positions for animation, then snaps/rounds to int for rendering and hit testing.

### Simplified Layout Output Example
```cpp
using NodeId = uint32_t;

struct Node {
  NodeId id;
  std::vector<NodeId> children;
  float localX = 0.0f;
  float localY = 0.0f;
  float hintedW = 0.0f;
  float hintedH = 0.0f;
};

struct LayoutOut {
  float absX = 0.0f;
  float absY = 0.0f;
  float absW = 0.0f;
  float absH = 0.0f;
  bool visible = true;
};

std::vector<Node> nodes;
std::vector<LayoutOut> layoutOut; // index by NodeId

void layout(NodeId id, float parentX, float parentY) {
  Node const& n = nodes[id];
  LayoutOut& out = layoutOut[id];
  out.absX = parentX + n.localX;
  out.absY = parentY + n.localY;
  out.absW = n.hintedW;
  out.absH = n.hintedH;
  out.visible = true;

  for (NodeId child : n.children) {
    layout(child, out.absX, out.absY);
  }
}
```

## PrimeFrame Data Model (Draft)
- `NodeId`: stable handle (index + generation).
- Nodes stored as AoS; callbacks live in a separate table and are referenced by `CallbackId`.
- Nodes own a list of primitive indices; primitives can be rects, text, or images.
- Themes resolve style tokens to palette/typography.

### Node (Authoring)
```cpp
struct Node {
  NodeId id{};
  NodeId parent{};
  std::vector<NodeId> children;

  LayoutType layout = LayoutType::None;
  SizeHint sizeHint;
  Insets padding;
  float gap = 0.0f;

  bool visible = true;
  bool focusable = false;
  bool clipChildren = true;
  int32_t tabIndex = -1;
  ThemeId theme = 0;
  CallbackId callbacks = 0;

  float localX = 0.0f;
  float localY = 0.0f;

  std::vector<uint32_t> primitives; // indices into PrimitiveStore
};
```

### Primitive (Authoring)
```cpp
enum class PrimitiveType : uint8_t { Rect, Text, Image };

struct Primitive {
  PrimitiveType type = PrimitiveType::Rect;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float width = 0.0f;   // if 0, use node width
  float height = 0.0f;  // if 0, use node height

  RectStyle rect;
  std::string text;
  TextStyle textStyle;
  ImageId image = 0;
  ImageStyle imageStyle;
};
```

### Layout Output (Computed)
```cpp
struct LayoutOut {
  float absX = 0.0f;
  float absY = 0.0f;
  float absW = 0.0f;
  float absH = 0.0f;
  bool visible = true;

  bool clipEnabled = false;
  float clipX0 = 0.0f;
  float clipY0 = 0.0f;
  float clipX1 = 0.0f;
  float clipY1 = 0.0f;

  uint32_t localOrder = 0;
  ThemeId theme = 0;
};
```

## Flattening Contract (PrimeFrame -> RenderBatch)
- Traversal order:
  - Roots are visited in root stack order (vector order).
  - Nodes are visited in pre-order within each root.
  - Primitives are emitted in stored list order.
- Visibility:
  - If `LayoutOut.visible` is false, skip node and its primitives.
  - If final width/height <= 0, skip that primitive.
- Positioning:
  - `px = LayoutOut.absX + Primitive.offsetX`
  - `py = LayoutOut.absY + Primitive.offsetY`
  - `pw = Primitive.width` (if 0, use `LayoutOut.absW`)
  - `ph = Primitive.height` (if 0, use `LayoutOut.absH`)
  - Convert to int by rounding endpoints:
    - `x0 = round(px)`, `y0 = round(py)`
    - `x1 = round(px + pw)`, `y1 = round(py + ph)`
- Clipping (default on):
  - Nodes default to `clipChildren = true`; node primitives are clipped to node bounds.
  - If clipping is enabled, set `RectFlagClip` / `TextFlagClip` and write clip rect.
  - Clip rect is the node bounds (or explicit clip) rounded to int.
- Rect primitives:
  - Resolve `RectStyle` via theme to palette indices, gradients, opacity.
  - Emit `RectStore` + `CommandType::Rect`.
- Text primitives:
  - Resolve `TextStyle` via theme to typography + palette.
  - Bake glyph runs, emit `TextStore` + `TextRunStore` + `GlyphStore` + `CommandType::Text`.
- Image primitives:
  - Not yet supported in `PrimeManifest`.
  - For now, skip or emit a placeholder rect; later extend RenderBatch with an image store.

### UI (Mostly Static)
- Build once and cache `RenderBatch` + `OptimizedBatch`.
- On change:
  - Update changed rectangles/text runs.
  - Increment `RenderBatch::revision`.
  - Re-run `OptimizeRenderBatch`.
  - Structural changes rebuild the affected root only; local visual tweaks patch existing rects.

### Large Dynamic Scene (~250k items)
- Maintain pre-allocated SoA buffers for rects/text.
- Each frame:
  - Copy or write positions directly into the pre-allocated arrays.
  - Update any per-item fields that changed (opacity, clip, z).
  - Increment `RenderBatch::revision`.
  - Re-run `OptimizeRenderBatch`.

## Cache/Reuse Rules
- `OptimizedBatch` can be reused only when all are unchanged:
  - `RenderBatch::revision`
  - Target width/height
  - Tile size
- Use `RenderBatch::reuseOptimized = true` and only bump `revision` on changes.

## Latency Considerations
- Lowest latency is a single-thread pipeline: build -> optimize -> render in one frame.
- If multi-threaded, use double-buffered SoA to avoid locks; accept ~1 frame of latency.

## Risks / Tradeoffs
- Full re-optimization each frame for large scenes is O(N) but predictable.
- Binning and cache building cost grows with visible primitives.
- Shared mutable ownership would reduce copies but risks stalls or tearing.

## Open Questions
- Is 1-frame latency acceptable for any multi-threaded path?
- Do we need a hard "render packet" boundary for future serialization or GPU handoff?
- Are there constraints on maximum item counts for UI vs. dynamic scenes?
- Should we formalize a minimal UI-to-RenderBatch mapping (rect/text/clip/gradient)?

## Layout Model: Two-Pass Measure + Arrange
- Bottom-up "measure" pass computes intrinsic sizes from content and children.
- Top-down "arrange" pass assigns final sizes/positions from parent constraints.

### Size Hints (Min/Max)
- Each rect can specify optional min/max per axis.
- Sizing rule per axis:
  - `measured = clamp(contentSize, min, max)`
  - `arranged = clamp(allocatedSize, min, max)`
- If constraints are impossible (sum of mins > available), keep mins and overflow; clipping is the default fallback.

### Split Layouts
- `HorizontalSplit`: divides available width equally among children.
- `VerticalSplit`: divides available height equally among children.
- Cross-axis size defaults to stretch to parent unless overridden.
- Distribution respects child min/max; if impossible, overflow/clip.

## Scroll Containers (Overlay, Both Axes, Auto)
- Scrollbars do not affect layout; they are drawn as overlay rectangles.
- Visibility is auto:
  - `showH = contentW > viewportW`
  - `showV = contentH > viewportH`
- Children are clipped to the viewport and rendered with `-scrollOffset`.

### Scrollbar Geometry
- `maxScrollX = max(0, contentW - viewportW)`
- `maxScrollY = max(0, contentH - viewportH)`
- Thumb size:
  - Horizontal: `thumbW = max(minThumb, viewportW * viewportW / contentW)`
  - Vertical: `thumbH = max(minThumb, viewportH * viewportH / contentH)`
- Thumb position:
  - Horizontal: `thumbX = viewportX + (scrollOffsetX / maxScrollX) * (viewportW - thumbW)`
  - Vertical: `thumbY = viewportY + (scrollOffsetY / maxScrollY) * (viewportH - thumbH)`

## Callback-Enabled Rectangle Hierarchy
- Each node has a rect, optional text, style, and callbacks (click/drag/hover/text input).
- Layout produces absolute rects for hit testing and flattening to `RenderBatch`.
- Rendering remains rect/text based; events are handled in the UI layer.

## Clipping Defaults
- `clipChildren` defaults to true for all nodes.
- Nodes that intentionally draw outside their bounds (e.g., focus outline, shadows) should set `clipChildren = false` or draw in an overlay layer.

### Event Propagation (Hit Test + Bubble Up)
- Find the highest-z root rect under the cursor.
- Descend into children to find the deepest rect under the cursor.
- Respect parent clipping: children outside a clipped parent are not eligible.
- Invoke the callback on the deepest rect first.
- If the callback returns `true`, the event is accepted and propagation stops.
- If the callback returns `false`, move one level up and try the parent callback.
- Continue bubbling up until a callback accepts the event or the root is reached.
- Pointer capture: if a node accepts a pointer-down/drag, it receives subsequent move/up events until release.

## Focus Model
- Single focused node at a time (`focusedNodeId`), only if it is focusable.
- Tab/Shift-Tab moves focus in a defined traversal order (stable per root).
- Keyboard/text input is routed directly to the focused node first; if it accepts, no other widgets are checked.
- Focus is rendered as a top-level overlay (drawn last) so it is always on top.
  - Focus outline may extend beyond the focused widget bounds.
  - Visual: Nintendo Switch-style pulsating outline between two colors.
  - On focus changes, the outline animates/morphs from old bounds to new bounds.
- On rebuilds: keep focus if the focused ID still exists; otherwise blur and clear focus.
- When the active root changes, focus moves to the first focusable widget in that root.
- It is valid for no widget to be focused (e.g., at startup).
- Blur callbacks are optional; focus changes can be handled entirely via focus assignment if preferred.

### Active Root Policy
- Click-to-front: any pointer down inside a root makes it active and moves it to the front of the root list.
- Active root changes drive focus handoff to the first focusable widget in that root.

### Tab Order (Best-Effort Spatial)
- Prefer ground-truth `tabIndex` when provided.
  - `tabIndex >= 0` participates; negative means skip.
- Otherwise compute a best-effort spatial order after layout:
  - Primary: active root only (first in root list).
  - Then: `y` (top to bottom), then `x` (left to right).
  - Ties: stable local order as a fallback.
- Only include visible, focusable nodes; clipped-out children are excluded.

## Z Ordering Model
- Top-level rects are stored as `std::vector<Rect>`; root stack order is the vector order.
  - Changing root z order is done by moving a root within the vector.
  - Target scale is small (<= ~100 roots), so vector moves are acceptable.
- Within each root, children use stable local order (pre-order traversal).
- Composite order for rendering/hit testing is `(rootOrder, localOrder)`.
  - Render: back-to-front roots, then local order.
  - Hit test: front-to-back roots, then descend to deepest child.

## Tree List Widget (Composite)
- Built from a scroll container + vertical list.
- Prefer virtualization (only visible rows).
- Fixed row height (decision) for simplicity and latency.
- Expand/collapse updates visible rows; rendering stays rect + text.

## Themes
- A theme struct is provided when creating a root rect hierarchy.
- Different roots may use different themes.
- Nodes store style tokens (not raw colors); themes resolve tokens to palette/typography.
- Theme switches can re-resolve tokens in place without a full rebuild; rebuild remains an acceptable fallback.
