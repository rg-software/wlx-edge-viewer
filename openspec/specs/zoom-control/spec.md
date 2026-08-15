# zoom-control Specification

## Purpose
Characterizes the existing zoom-control capability of the Total Commander WLX Lister plugin: how Ctrl+Plus / Ctrl+Minus / Ctrl+0 hotkeys snap the WebView2 zoom factor through a fixed table of discrete steps, how the zoom factor is persisted per processor type when the `[Chromium]` `KeepZoom` ini key is set, and how the zoom hotkeys are kept for the plugin instead of being forwarded to the Total Commander parent window. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds.
## Requirements
### Requirement: Zoom hotkeys

The plugin SHALL recognize a small set of Ctrl-modified hotkeys for changing the zoom factor of the rendered document. The recognized keys SHALL be the main keyboard's plus and minus (`VK_OEM_PLUS`, `VK_OEM_MINUS`), the numeric keypad's plus and minus (`VK_ADD`, `VK_SUBTRACT`), and the main keyboard's zero plus the numeric keypad's zero (`'0'`, `VK_NUMPAD0`). All of these SHALL require the Ctrl modifier to be held; unmodified presses of the same keys SHALL NOT be treated as zoom hotkeys. The plus variants SHALL increase the zoom factor to the next higher step in the zoom table, the minus variants SHALL decrease it to the next lower step, and the zero variants SHALL reset the zoom factor to 1.0.

#### Scenario: Ctrl+Plus increases zoom

- **WHEN** the user presses Ctrl+`VK_OEM_PLUS` or Ctrl+`VK_ADD` while a document is rendered
- **THEN** the plugin moves the zoom factor up to the next higher value in the zoom step table

#### Scenario: Ctrl+Minus decreases zoom

- **WHEN** the user presses Ctrl+`VK_OEM_MINUS` or Ctrl+`VK_SUBTRACT` while a document is rendered
- **THEN** the plugin moves the zoom factor down to the next lower value in the zoom step table

#### Scenario: Ctrl+Zero resets zoom

- **WHEN** the user presses Ctrl+`'0'` or Ctrl+`VK_NUMPAD0` while a document is rendered
- **THEN** the plugin resets the zoom factor to 1.0

#### Scenario: unmodified keys are not zoom hotkeys

- **WHEN** the user presses `VK_OEM_PLUS`, `VK_OEM_MINUS`, `'0'`, `VK_ADD`, `VK_SUBTRACT` or `VK_NUMPAD0` without holding Ctrl
- **THEN** the plugin does not treat the press as a zoom hotkey and the key is forwarded to the Total Commander parent as a normal key event

### Requirement: Discrete zoom step table

The zoom factor SHALL be constrained to a fixed, ordered table of seventeen discrete values: 0.25, 0.33, 0.5, 0.67, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0 and 5.0. Increasing zoom SHALL move to the first table entry strictly greater than the current factor; decreasing zoom SHALL move to the first table entry strictly less than the current factor. If the current factor is already at or beyond an end of the table, the corresponding increase or decrease operation SHALL have no effect. The Ctrl+Zero reset SHALL always target 1.0, which is one of the table entries.

#### Scenario: snap up to the next step

- **WHEN** the current zoom factor is 1.0 and the user triggers an increase
- **THEN** the zoom factor becomes 1.1, the first table entry strictly greater than 1.0

#### Scenario: snap down to the previous step

- **WHEN** the current zoom factor is 1.0 and the user triggers a decrease
- **THEN** the zoom factor becomes 0.9, the first table entry strictly less than 1.0

#### Scenario: ceiling at the top of the table

- **WHEN** the current zoom factor is 5.0 and the user triggers an increase
- **THEN** the zoom factor stays at 5.0 because no table entry is strictly greater than the current value

#### Scenario: floor at the bottom of the table

- **WHEN** the current zoom factor is 0.25 and the user triggers a decrease
- **THEN** the zoom factor stays at 0.25 because no table entry is strictly less than the current value

#### Scenario: reset targets the midpoint

- **WHEN** the current zoom factor is any value and the user triggers a reset
- **THEN** the zoom factor becomes 1.0, the neutral entry of the zoom step table

### Requirement: Zoom persistence across files of the same type

The plugin SHALL persist the zoom factor per processor type. When the `[Chromium]` `KeepZoom` key in `edgeviewer.ini` is set to 1, the plugin SHALL remember the zoom factor that was in effect when a document of a given processor type was last rendered, and SHALL restore that factor when a new WebView2 control is created for a document of the same processor type. When `KeepZoom` is 0 or unset, the zoom factor SHALL NOT be carried between files and every new document SHALL start at the default zoom factor.

#### Scenario: KeepZoom enabled restores previous zoom

- **WHEN** `[Chromium]` `KeepZoom=1` is set in `edgeviewer.ini`, the user zooms a Markdown document to 1.5 and then opens another Markdown document
- **THEN** the new Markdown document is opened with the zoom factor restored to 1.5

#### Scenario: KeepZoom disabled resets zoom

- **WHEN** `[Chromium]` `KeepZoom` is 0 or absent, the user zooms a Markdown document to 1.5 and then opens another Markdown document
- **THEN** the new Markdown document is opened with the default zoom factor rather than the previously chosen 1.5

### Requirement: Zoom is tracked per processor type, not per file

The persisted zoom factor SHALL be keyed by processor type, not by individual file. Navigating from one file to another file of the same processor type SHALL keep the zoom factor in effect (when persistence is enabled), because both files share the same per-type slot. Navigating to a file handled by a different processor type SHALL switch to that type's slot: the zoom factor for the new type SHALL be its persisted value when persistence is enabled, or the default zoom factor otherwise. The previously used type's slot SHALL be left untouched and MAY be restored when the user returns to a file of that type.

#### Scenario: same type keeps zoom

- **WHEN** persistence is enabled, the user opens Markdown file A and zooms it to 2.0, then opens Markdown file B without changing the zoom
- **THEN** Markdown file B is rendered at zoom factor 2.0 because both files share the Markdown-type zoom slot

#### Scenario: switching to a different type uses that type's slot

- **WHEN** persistence is enabled, the user opens a Markdown document and zooms it to 2.0, then opens an AsciiDoc document whose own type slot is still at the default
- **THEN** the AsciiDoc document is rendered at the default zoom factor, not 2.0, because the AsciiDoc-type slot is independent of the Markdown-type slot

#### Scenario: returning to a previous type restores its slot

- **WHEN** persistence is enabled, the user opens a Markdown document and zooms it to 2.0, then opens an AsciiDoc document, then opens another Markdown document
- **THEN** the second Markdown document is rendered at the Markdown-type slot's persisted 2.0

### Requirement: Zoom hotkeys are not relayed to the parent window

When a key press is recognized as a zoom hotkey, the plugin SHALL consume the key event: it SHALL change the zoom factor and SHALL NOT forward the key to the Total Commander parent window. Other keyboard events that are not zoom hotkeys SHALL be forwarded to the parent window so that Total Commander's own hotkeys (navigation, quick-search, etc.) keep working while the lister has focus.

#### Scenario: zoom hotkey consumed

- **WHEN** the user presses a recognized zoom hotkey (e.g. Ctrl+`VK_OEM_PLUS`)
- **THEN** the plugin changes the zoom factor and does not post the key to the Total Commander parent window

#### Scenario: non-zoom key forwarded

- **WHEN** the user presses a key that is not a recognized zoom hotkey (e.g. Page Down)
- **THEN** the plugin forwards the key event to the Total Commander parent window so that Total Commander can handle it

### Requirement: 32-bit and 64-bit zoom parity

The set of recognized zoom hotkeys, the zoom step table, the `KeepZoom` persistence rule, the per-type tracking and the consume-vs-forward behavior SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read the same `[Chromium]` `KeepZoom` key from `edgeviewer.ini` and both builds SHALL use the same zoom step table, so a user switching between the 32-bit and 64-bit plugin sees the same zoom behavior.

#### Scenario: Win32 build zoom hotkeys

- **WHEN** the 32-bit plugin is loaded and the user presses Ctrl+`VK_OEM_PLUS`
- **THEN** the zoom factor snaps up to the next step of the same zoom table used by the 64-bit build

#### Scenario: x64 build zoom persistence

- **WHEN** the 64-bit plugin is loaded with `[Chromium]` `KeepZoom=1` and the user opens a second document of the same type after zooming the first
- **THEN** the second document's zoom factor is restored to match the first, the same behavior the 32-bit build exhibits

