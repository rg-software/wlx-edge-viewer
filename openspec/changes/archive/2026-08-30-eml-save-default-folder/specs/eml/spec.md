## MODIFIED Requirements

### Requirement: Save attachment to chosen folder

The EML view SHALL allow the user to save any listed non-inline attachment to a folder of their choice. The save SHALL write the attachment's raw bytes to a file named after the attachment's original filename (sanitized as needed for the target filesystem). Saving SHALL work on 32-bit and 64-bit Windows builds and on the Linux build. The folder picker SHALL open pre-selected on the directory that contains the `.eml` file currently being viewed (its "current folder") rather than the OS/session default, so the user does not have to re-navigate to where the email lives; the user remains free to navigate anywhere before confirming.

#### Scenario: User saves an attachment

- **WHEN** the user clicks a listed attachment and chooses a destination folder
- **THEN** the attachment is written to that folder and the view confirms the save

#### Scenario: Folder picker defaults to the message's current folder

- **WHEN** the user clicks a listed attachment and the folder picker opens
- **THEN** the picker is initially positioned on the directory containing the `.eml` file being viewed

#### Scenario: User cancels the folder selection

- **WHEN** the user clicks an attachment but dismisses the folder picker without choosing a folder
- **THEN** nothing is written and the view is unchanged

#### Scenario: Save failing to write

- **WHEN** an attachment cannot be written to the chosen folder (e.g. permission denied)
- **THEN** the view shows a message explaining that the save failed and no partial or corrupt file is left behind misleadingly
