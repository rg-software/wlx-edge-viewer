# Spec Delta

## ADDED Requirements

### Requirement: Quick-view Tab is forwarded to Total Commander when enabled

In quick-view mode with `[QuickView] PassTabToTC=1`, the plugin SHALL suppress in-page focus navigation for `Tab` and `Shift+Tab` and post the key to the TC parent window so panel switching works. Suppression happens page-side (the document keydown bridge calls `preventDefault` and reports the press to the host, which relays it), because WebView2 does not surface focus-navigation keys through the host key channel. In F3 lister mode, or with the option disabled/absent, `Tab` SHALL keep native in-page link navigation.

#### Scenario: Tab switches panels in quick view when enabled

- **WHEN** a document is rendered in quick view with `[QuickView] PassTabToTC=1` and the user presses `Tab`
- **THEN** the page does not advance link focus and TC moves focus to the other panel

#### Scenario: Shift+Tab behaves like Tab in quick view when enabled

- **WHEN** a document is rendered in quick view with `[QuickView] PassTabToTC=1` and the user presses `Shift+Tab`
- **THEN** the page does not move link focus and the key reaches the TC parent window

#### Scenario: Tab navigates links in F3 lister

- **WHEN** a document is rendered in the standalone F3 lister and the user presses `Tab`
- **THEN** the WebView advances focus to the next focusable element in the page

#### Scenario: Tab stays in page when the option is off

- **WHEN** a document is rendered in quick view with `[QuickView] PassTabToTC=0` (or the key absent) and the user presses `Tab`
- **THEN** the WebView advances link focus in the page and nothing is posted to TC for panel switching

#### Scenario: Ctrl+Tab is unchanged

- **WHEN** the user presses `Ctrl+Tab` in either mode
- **THEN** the plugin applies no new handling (page default, matching peer viewers)

#### Scenario: 32-bit and 64-bit Tab parity

- **WHEN** the same quick-view file is opened in the Win32 and x64 builds with the option enabled and `Tab` is pressed
- **THEN** both builds forward the key to TC and neither advances page link focus
