## Purpose

Characterizes the existing accelerator-key handling of the Total Commander WLX Lister plugin: how the plugin disables the browser's own hotkeys so they do not conflict with Total Commander, how it forwards non-zoom key events from the WebView2 control to the Total Commander parent window, how the `Q` key closes the lister and how the `1`–`8` keys switch Total Commander's Quick View tabs, and how zoom hotkeys are kept exclusive to the plugin. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin.

## ADDED Requirements

### Requirement: Browser hotkeys are disabled

The plugin SHALL tell the WebView2 engine that the browser's accelerator keys are not enabled. This SHALL prevent the web engine from reacting on its own to keys like Ctrl+F (browser find), Ctrl+P (browser print), F5 (reload) and F12 (developer tools), because those same keys are meaningful to Total Commander and would otherwise be intercepted before Total Commander can see them. With the engine's accelerator keys disabled, the plugin SHALL be the only consumer of key events inside the lister area and SHALL decide which keys to handle itself, which to forward to the parent, and which to drop.

#### Scenario: Ctrl+F does not open the browser find bar

- **WHEN** the user presses Ctrl+F while a document is rendered in the lister
- **THEN** the WebView2 engine does not open its built-in find bar, because browser accelerator keys are disabled; the key is available for Total Commander's own purposes

#### Scenario: F5 does not reload the document

- **WHEN** the user presses F5 while a document is rendered in the lister
- **THEN** the WebView2 engine does not reload the document, because browser accelerator keys are disabled; the key is available for Total Commander's own purposes

#### Scenario: F12 does not open developer tools

- **WHEN** the user presses F12 while a document is rendered in the lister
- **THEN** the WebView2 engine does not open its developer tools, because browser accelerator keys are disabled

### Requirement: WebView key events are relayed to the parent

For every key-down event in the WebView2 control that is not consumed by the plugin's own zoom handling, the plugin SHALL post the key to the Total Commander parent window. This SHALL keep Total Commander's navigation and quick-search hotkeys working while the lister has focus, and it SHALL be the single channel through which non-special keys reach the host. No per-key filtering SHALL occur at this stage apart from the zoom check: every non-zoom key-down SHALL be relayed.

#### Scenario: navigation key forwarded

- **WHEN** the user presses Page Down while the lister has focus
- **THEN** the plugin posts the Page Down key event to the Total Commander parent window so that Total Commander can advance to the next file

#### Scenario: letter key forwarded

- **WHEN** the user presses a letter key (other than the special `Q`) while the lister has focus and Ctrl is not held
- **THEN** the plugin posts the letter key event to the Total Commander parent window so that Total Commander's quick-search can use it

### Requirement: Q key closes the lister

The `Q` key SHALL be the plugin's quick-close shortcut for the lister. When the user presses `Q` (without modifiers) while the rendered document has focus, the WebView2's JavaScript keydown listener SHALL post a message to the host. The host SHALL translate that message into a `Q` key-down event and SHALL post it to the Total Commander parent window, which interprets `Q` as the close-lister command. The lister window SHALL then be closed by Total Commander, not by the plugin itself.

#### Scenario: Q closes the lister

- **WHEN** the user presses `Q` while a document is rendered in the lister
- **THEN** the plugin posts a `Q` key-down event to the Total Commander parent window, which closes the lister

#### Scenario: Ctrl+Q is not the close shortcut

- **WHEN** the user presses Ctrl+Q while a document is rendered in the lister
- **THEN** the close-lister shortcut does not trigger because the close shortcut requires the unmodified `Q` key; the behavior of the Ctrl-modified press is governed by the generic key-relay rule

### Requirement: Digits 1 to 8 switch Quick View tabs

The digit keys `1` through `8` SHALL be the plugin's shortcut for switching Total Commander's Quick View tabs. When the user presses one of those digit keys (without modifiers) while the rendered document has focus, the WebView2's JavaScript keydown listener SHALL post a message to the host carrying the digit's character code. The host SHALL translate that message into the corresponding digit key-down event and SHALL post it to the Total Commander parent window, which switches the active Quick View tab. The digits `9` and `0` SHALL NOT be part of this shortcut.

#### Scenario: digit 3 switches to the third tab

- **WHEN** the user presses `3` while a document is rendered in the lister
- **THEN** the plugin posts a `3` key-down event to the Total Commander parent window, which switches the Quick View to the third tab

#### Scenario: digits 1 through 8 are all valid tab shortcuts

- **WHEN** the user presses any of `1`, `2`, `3`, `4`, `5`, `6`, `7` or `8` while a document is rendered in the lister
- **THEN** the plugin posts the corresponding digit key-down event to the Total Commander parent window, which switches the Quick View to the matching tab

#### Scenario: digit 9 is not a tab shortcut

- **WHEN** the user presses `9` while a document is rendered in the lister
- **THEN** the plugin does not treat the press as a tab switch; the key is forwarded to the parent under the generic key-relay rule

### Requirement: Zoom hotkeys are not relayed

The zoom hotkeys (Ctrl+Plus, Ctrl+Minus, Ctrl+Zero and the numeric keypad's equivalents) SHALL be handled by the plugin itself and SHALL NOT be posted to the Total Commander parent window. The no-relay rule SHALL be enforced in the same key-down handler that decides whether a key is a zoom hotkey or a relay candidate.

#### Scenario: Ctrl+Plus is not forwarded to TC

- **WHEN** the user presses Ctrl+Plus while a document is rendered in the lister
- **THEN** the plugin consumes the key for zoom and does not post it to the Total Commander parent window

#### Scenario: Ctrl+Zero is not forwarded to TC

- **WHEN** the user presses Ctrl+Zero while a document is rendered in the lister
- **THEN** the plugin consumes the key for the zoom reset and does not post it to the Total Commander parent window

### Requirement: 32-bit and 64-bit accelerator key parity

The browser-hotkeys disabling, the key-relay-to-parent rule, the `Q` close-lister shortcut, the `1`–`8` Quick View tab shortcut and the no-relay rule for zoom hotkeys SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL disable the same set of browser accelerator keys and both builds SHALL post the same key events to the Total Commander parent for the same physical key presses.

#### Scenario: Win32 build Q closes the lister

- **WHEN** the 32-bit plugin is loaded and the user presses `Q`
- **THEN** the plugin posts the `Q` key-down to the Total Commander parent and the lister closes, matching the 64-bit build's behavior

#### Scenario: x64 build digit tab switching

- **WHEN** the 64-bit plugin is loaded and the user presses `3`
- **THEN** the plugin posts the `3` key-down to the Total Commander parent and the Quick View switches to the third tab, matching the 32-bit build's behavior