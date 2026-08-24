## MODIFIED Requirements

### Requirement: Zoom persistence across files of the same type

The plugin SHALL persist the zoom factor per processor type. When the `[WebView]` `KeepZoom` key in `edgeviewer.ini` is set to 1, the plugin SHALL remember the zoom factor that was in effect when a document of a given processor type was last rendered, and SHALL restore that factor when a new control is created for a document of the same processor type. When `KeepZoom` is 0 or unset, the zoom factor SHALL NOT be carried between files and every new document SHALL start at the default zoom factor.

Per-processor isolation is Windows-only. On Linux, `QtWebEngineBackend` has no `ZoomFactorChanged` hook, so `gs_ZoomFactor` is never written; zoom persistence relies on Qt Web Engine's per-origin memory, which gives a single shared zoom value across all plugin file types (future-work #4 in `Readme.md`).

#### Scenario: KeepZoom enabled restores previous zoom

- **WHEN** `[WebView]` `KeepZoom=1` is set in `edgeviewer.ini`, the user zooms a Markdown document to 1.5 and then opens another Markdown document
- **THEN** the new Markdown document is opened with the zoom factor restored to 1.5

#### Scenario: KeepZoom disabled resets zoom

- **WHEN** `[WebView]` `KeepZoom` is 0 or absent, the user zooms a Markdown document to 1.5 and then opens another Markdown document
- **THEN** the new Markdown document is opened with the default zoom factor rather than the previously chosen 1.5

### Requirement: Zoom is tracked per processor type, not per file

On Windows, the persisted zoom factor SHALL be keyed by processor type, not by individual file. Navigating from one file to another file of the same processor type SHALL keep the zoom factor in effect (when persistence is enabled), because both files share the same per-type slot. Navigating to a file handled by a different processor type SHALL switch to that type's slot: the zoom factor for the new type SHALL be its persisted value when persistence is enabled, or the default zoom factor otherwise. The previously used type's slot SHALL be left untouched and MAY be restored when the user returns to a file of that type.

On Linux, the zoom is shared across all file types (single per-origin zoom value), so per-processor isolation does not apply.

#### Scenario: same type keeps zoom

- **WHEN** persistence is enabled, the user opens Markdown file A and zooms it to 2.0, then opens Markdown file B without changing the zoom
- **THEN** Markdown file B is rendered at zoom factor 2.0 because both files share the Markdown-type zoom slot

#### Scenario: switching to a different type uses that type's slot

- **WHEN** persistence is enabled, the user opens a Markdown document and zooms it to 2.0, then opens an AsciiDoc document whose own type slot is still at the default
- **THEN** the AsciiDoc document is rendered at the default zoom factor, not 2.0, because the AsciiDoc-type slot is independent of the Markdown-type slot

#### Scenario: returning to a previous type restores its slot

- **WHEN** persistence is enabled, the user opens a Markdown document and zooms it to 2.0, then opens an AsciiDoc document, then opens another Markdown document
- **THEN** the second Markdown document is rendered at the Markdown-type slot's persisted 2.0

### Requirement: 32-bit and 64-bit zoom parity

The set of recognized zoom hotkeys, the zoom step table, the `KeepZoom` persistence rule, the per-type tracking and the consume-vs-forward behavior SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read the same `[WebView]` `KeepZoom` key from `edgeviewer.ini` and both builds SHALL use the same zoom step table, so a user switching between the 32-bit and 64-bit plugin sees the same zoom behavior.

#### Scenario: Win32 build zoom hotkeys

- **WHEN** the 32-bit plugin is loaded and the user presses Ctrl+`VK_OEM_PLUS`
- **THEN** the zoom factor snaps up to the next step of the same zoom table used by the 64-bit build

#### Scenario: x64 build zoom persistence

- **WHEN** the 64-bit plugin is loaded with `[WebView]` `KeepZoom=1` and the user opens a second document of the same type after zooming the first
- **THEN** the second document's zoom factor is restored to match the first, the same behavior the 32-bit build exhibits
