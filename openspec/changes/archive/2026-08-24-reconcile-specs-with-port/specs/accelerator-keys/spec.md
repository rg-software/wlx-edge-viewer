## MODIFIED Requirements

### Requirement: WebView key events are relayed to the parent

On Windows, for every key-down event in the WebView2 control that is not consumed by the plugin's own zoom handling, the plugin SHALL post the key to the Total Commander parent window. This SHALL keep Total Commander's navigation and quick-search hotkeys working while the lister has focus, and it SHALL be the single channel through which non-special keys reach the host. No per-key filtering SHALL occur at this stage apart from the zoom check: every non-zoom key-down SHALL be relayed.

The HWND-level accelerator-key relaying (including the `Q`-close and `1`–`8` Quick View tab bridges) is Windows-only (future-work #6 in `Readme.md`). On Linux, Qt Web Engine's own focus handling and Double Commander's hotkey dispatch apply instead; there is no `WebView2` control and no HWND to relay through. The Linux ESC-close bridge and the `F`-key image toggle are the Linux equivalents (see the `wlx-contract` and `images` capabilities).

#### Scenario: navigation key forwarded

- **WHEN** the user presses Page Down while the lister has focus
- **THEN** the plugin posts the Page Down key event to the Total Commander parent window so that Total Commander can advance to the next file

#### Scenario: letter key forwarded

- **WHEN** the user presses a letter key (other than the special `Q`) while the lister has focus and Ctrl is not held
- **THEN** the plugin posts the letter key event to the Total Commander parent window so that Total Commander's quick-search can use it

#### Scenario: Linux defers to Qt Web Engine focus handling

- **WHEN** the Linux build renders a document and the user presses a key
- **THEN** Qt Web Engine and Double Commander's own focus/hotkey handling apply; the plugin does not relay keys through an HWND parent
