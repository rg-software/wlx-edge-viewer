## Purpose

Lets Total Commander users view `.eml` email messages inline in the lister, rendering the message as a document the way an email client would - headers, body, inline images and attachment list.

## Requirements

### Requirement: EML file detection

The lister SHALL route files whose extension matches the `EML` extension set from the `[Extensions]` section of `edgeviewer.ini` (e.g. `EML`) to the EML processor (`Resources/assets/eml/`) instead of any fallback processor. Detection SHALL be case-insensitive and SHALL behave identically on 32-bit and 64-bit builds.

#### Scenario: Opening a .eml file

- **WHEN** the user opens a file named `message.eml` in Total Commander
- **THEN** the EML processor renders the message instead of the generic fallback

#### Scenario: Non-matching extension

- **WHEN** the user opens a file that does not match the `EML` extension set (e.g. `.txt`)
- **THEN** the EML processor is not selected and other processors handle the file as before

### Requirement: Email headers displayed

The rendered view SHALL display the message headers that identify the mail - Subject, From, To, Cc and Date - in a distinct header block above the body. Header values SHALL be decoded per RFC 2047 (encoded-words such as `=?UTF-8?B?...?=`) and rendered as text. Headers that are absent in the message MAY be omitted.

#### Scenario: Message with standard headers

- **WHEN** an `.eml` contains Subject, From, To and Date headers
- **THEN** those four values are visible in a header block at the top of the rendered view

#### Scenario: Encoded header value

- **WHEN** a header value uses RFC 2047 encoded-words with UTF-8 base64 or quoted-printable
- **THEN** the decoded text is displayed instead of the raw `=?`-encoded token

### Requirement: Body rendering

The EML view SHALL render the HTML body part of the message as the main document content. When the message has no HTML part, the plain-text body SHALL be rendered instead, preserving line breaks. Transfer encodings base64 and quoted-printable SHALL be decoded. The body charset declared in the part's `Content-Type` SHALL be honored when decoding the body text.

#### Scenario: HTML body

- **WHEN** the `.eml` contains a `text/html` body
- **THEN** that HTML is rendered as a formatted document

#### Scenario: Plain-text body

- **WHEN** the `.eml` contains only a `text/plain` body
- **THEN** the plain text is rendered with its line breaks preserved

#### Scenario: Base64/quoted-printable encoded body

- **WHEN** the HTML or text body is encoded with base64 or quoted-printable
- **THEN** the decoded content is rendered, not the raw encoded bytes

### Requirement: Multipart message handling

For `multipart/alternative` messages, the HTML alternative SHALL be preferred over the plain-text alternative. For `multipart/related` messages, parts referenced by `cid:` from the HTML body SHALL be resolved and displayed as inline images. For `multipart/mixed` (or nested combinations), the primary HTML/text part SHALL be rendered as the body and remaining parts treated as attachments.

#### Scenario: Multipart alternative preferring HTML

- **WHEN** the message is `multipart/alternative` with both plain-text and HTML parts
- **THEN** the HTML part is rendered as the body

#### Scenario: Inline image referenced by cid

- **WHEN** the HTML body references a `multipart/related` part via `cid:` and that part is an image
- **THEN** the image is displayed inline at the reference location

#### Scenario: Attachment in multipart mixed

- **WHEN** the message is `multipart/mixed` with an attachment part
- **THEN** the body part is rendered and the attachment is listed (name and, where available, size) so the user can see it was attached

### Requirement: Malformed message handling

The EML view SHALL not crash or hang on malformed input. If the file cannot be parsed as a MIME message, the raw file content SHALL be displayed as text. This MUST hold for both 32-bit and 64-bit builds.

#### Scenario: File that is not a MIME message

- **WHEN** a `.eml` file cannot be parsed as a MIME message
- **THEN** the view shows the raw file content as text without crashing

#### Scenario: Corrupt or empty message

- **WHEN** an `.eml` has no parsable headers or body
- **THEN** the view still renders without error, showing whatever content is available