# virustotal-link-scan Specification

## Purpose

Adds a "Send link to VirusTotal" entry to the built-in web-link context menu inside rendered views, letting users open a URL-analysis page for a right-clicked web link in their default browser.

## Requirements

### Requirement: Link context menu entry

Right-clicking a **web link** (a hyperlink whose target uses the `http:` or `https:` scheme) inside any rendered view SHALL extend the engine's built-in context menu with a "Send link to VirusTotal" entry, alongside the standard browser entries (Open link, Open link in new window, Copy link address, and so on), which SHALL remain unchanged. The entry SHALL appear regardless of which processor produced the view (HTML, MHT, EML, Markdown, RST, AsciiDoc, URL/Other pages) and on both the Windows and Linux builds. Right-clicking content that is not a web link SHALL NOT show the entry; the stock menu SHALL remain otherwise unchanged.

#### Scenario: Right-click a web link in an HTML view

- **WHEN** an HTML file is rendered and the user right-clicks a hyperlink pointing to `https://example.com/a`
- **THEN** the built-in context menu is shown containing a "Send link to VirusTotal" entry in addition to the standard link entries

#### Scenario: Right-click a web link in a loader-based view

- **WHEN** a Markdown, RST, AsciiDoc, MHT, or EML file is rendered and the user right-click a hyperlink pointing to `http://example.com/b`
- **THEN** the built-in context menu is shown containing a "Send link to VirusTotal" entry

#### Scenario: Right-click a link on a navigated page

- **WHEN** a URL file or other processor renders a page and the user right-clicks a web link on it
- **THEN** the built-in context menu is shown containing a "Send link to VirusTotal" entry

#### Scenario: Right-click non-link content

- **WHEN** the user right-clicks plain text, an image, or empty space in any rendered view
- **THEN** the built-in context menu contains no VirusTotal entry and is otherwise unchanged

#### Scenario: Local links are excluded

- **WHEN** the user right-clicks a hyperlink whose target is a local reference (`local.example`, `lister.example`, `ev://`, `evh://`, `file:`)
- **THEN** the built-in context menu contains no VirusTotal entry

### Requirement: Scope is the link under the cursor

The presence of the entry SHALL be determined solely by the hyperlink under the cursor at the moment of the right-click. It SHALL NOT depend on a fixed list of processor types, on configuration, or on the file extension being viewed. An entry is never shown on a view that cannot produce a right-clickable web link (image-only views, directory listings whose links target local files, the built-in PDF viewer).

#### Scenario: Directory listing links expose no entry

- **WHEN** a directory is rendered and the user right-clicks a thumbnail or file entry (whose link targets a local path)
- **THEN** the context menu contains no VirusTotal entry

#### Scenario: Image view exposes no entry

- **WHEN** an image file is rendered and the user right-clicks anywhere in it
- **THEN** the context menu contains no VirusTotal entry

### Requirement: Opens in the default browser

Selecting "Send link to VirusTotal" SHALL open the VirusTotal URL-analysis page for the right-clicked link in the user's **default web browser**, and SHALL NOT navigate the rendered view or open any new view inside the plugin. The launched address SHALL identify exactly the right-clicked link (the plugin SHALL encode the link as required so the full original URL survives the round trip, including query strings and fragments where the host accepts them). The plugin SHALL use the host OS's standard "open in default browser" mechanism on each platform.

#### Scenario: Selecting the entry opens VirusTotal

- **GIVEN** a rendered view showing a hyperlink to `https://example.com/a?x=1`
- **WHEN** the user right-clicks the link and selects "Send link to VirusTotal"
- **THEN** the user's default browser opens the VirusTotal URL-analysis page for `https://example.com/a?x=1`
- **AND** the plugin's rendered view remains unchanged (no navigation)

### Requirement: No configuration or persistence

The entry SHALL be a fixed menu item present whenever its precondition holds; it SHALL introduce no ini keys, no on-disk state, and no user-settable options. Its behavior SHALL be identical on the 32-bit and 64-bit Windows builds and the Linux x64 build.

#### Scenario: No configuration required

- **WHEN** a fresh plugin install renders any link-bearing view and the user right-clicks a web link
- **THEN** the "Send link to VirusTotal" entry is present and functional without any setup

#### Scenario: Behavior is equal across platforms

- **WHEN** the same link-bearing file is opened on Windows (Win32 and x64) and Linux
- **THEN** the right-click menu shows the same "Send link to VirusTotal" entry and selecting it opens the user's default browser on each platform