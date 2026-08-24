## Purpose

Establishes a bidirectional command channel between the rendered loader JavaScript and the plugin host, so the host can perform native actions (such as saving an EML attachment to a user-chosen folder) and report the outcome back into the rendered view on both supported platforms.

## Requirements

### Requirement: JS to host save request

The plugin SHALL provide a JavaScript-to-host message path that carries a save request from a rendered loader to the host. The message SHALL include the attachment bytes (base64) and the suggested filename. This path SHALL work on 32-bit and 64-bit Windows builds and on the Linux build.

#### Scenario: Loader posts a save request

- **WHEN** a rendered loader posts a save request message carrying attachment bytes and a filename
- **THEN** the host receives the complete message without truncation, decodes the base64 bytes, and proceeds with the save flow

#### Scenario: Large attachment is not truncated

- **WHEN** the attachment size approaches the transport limits of the per-OS bridge (WM_COPYDATA on Windows, the `ev://_cmd` URL scheme on Linux)
- **THEN** the full attachment is still delivered intact or the save is declined with an explicit cannot-save message, never silently truncated

### Requirement: Host to JS result delivery

The plugin SHALL provide a host-to-JavaScript path that reports the outcome of a host-initiated action back to the rendered view, without reloading or re-navigating the document. The outcome SHALL distinguish success, user-cancel, and failure.

#### Scenario: Host reports a successful save

- **WHEN** the host has written the attachment to the chosen folder
- **THEN** the rendered view shows a confirmation that the attachment was saved

#### Scenario: Host reports the user cancelled

- **WHEN** the user dismisses the folder picker without choosing a folder
- **THEN** the rendered view shows no error and its content is unchanged

#### Scenario: Host reports a save failure

- **WHEN** the host cannot write the attachment (e.g. permission denied or invalid path)
- **THEN** the rendered view shows a message explaining that the save failed

### Requirement: Bidirectional channel coexistence

The new save-request and result-delivery directions SHALL coexist with the existing one-way JS-to-host commands (CMD_KEY, CMD_MENU, CMD_ZOOM) without changing their behavior.

#### Scenario: zoom and menu flows still work

- **WHEN** a loader uses the existing zoom or context-menu commands after this change is installed
- **THEN** those commands behave exactly as before