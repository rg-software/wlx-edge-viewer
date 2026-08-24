## MODIFIED Requirements

### Requirement: Attachment in multipart mixed

When the message is `multipart/mixed` (or a nested combination) with an attachment part, the body part SHALL be rendered as the body and the non-inline attachment parts SHALL be listed in a footer so the user can see they were attached and can save them. Each listed attachment SHALL show its filename and, where available, its size. Clicking an attachment SHALL initiate saving that attachment to a folder chosen by the user.

#### Scenario: Attachment in multipart mixed

- **WHEN** the message is `multipart/mixed` with an attachment part
- **THEN** the body part is rendered and the attachment is listed (name and, where available, size) as a clickable entry

#### Scenario: User saves an attachment

- **WHEN** the user clicks a listed attachment and chooses a destination folder
- **THEN** the attachment is written to that folder and the view confirms the save

## ADDED Requirements

### Requirement: Save attachment to chosen folder

The EML view SHALL allow the user to save any listed non-inline attachment to a folder of their choice. The save SHALL write the attachment's raw bytes to a file named after the attachment's original filename (sanitized as needed for the target filesystem). Saving SHALL work on 32-bit and 64-bit Windows builds and on the Linux build.

#### Scenario: User cancels the folder selection

- **WHEN** the user clicks an attachment but dismisses the folder picker without choosing a folder
- **THEN** nothing is written and the view is unchanged

#### Scenario: Save failing to write

- **WHEN** an attachment cannot be written to the chosen folder (e.g. permission denied)
- **THEN** the view shows a message explaining that the save failed and no partial or corrupt file is left behind misleadingly

### Requirement: Inline images are not saved as attachments

Images that are inline (cid-referenced in the body) SHALL NOT appear as savable attachments in the footer. Only non-inline parts are eligible for the save action.

#### Scenario: Inline image excluded from save

- **WHEN** a message has an inline cid-referenced image and a regular attachment
- **THEN** the footer lists the regular attachment as savable but not the inline image