## MODIFIED Requirements

### Requirement: Shell context menu invocation from the directory viewer

The pop-up context menu SHALL only be reachable from the directory viewer, and only on Windows. When the user right-clicks a file entry inside the rendered directory listing, the directory processor SHALL send a menu command to the lister window, and that window SHALL display the native Windows Explorer shell context menu for the right-clicked file. Right-clicking inside other processors (Markdown, AsciiDoc, RST, HTML, MHT, images, PDF) SHALL NOT trigger this menu.

This capability is Windows-only. The Linux build has no equivalent; right-clicking inside the rendered view on Linux produces no plugin-defined popup (future-work #3 in `Readme.md`).

#### Scenario: right-click in directory view

- **WHEN** the directory viewer is rendering a directory listing and the user right-clicks one of the file entries
- **THEN** the directory processor sends a menu command to the lister window and the lister window displays the Windows Explorer shell context menu for the right-clicked file

#### Scenario: right-click in a Markdown document

- **WHEN** a Markdown document is being rendered and the user right-clicks inside it
- **THEN** the pop-up shell context menu is not displayed, because the menu is only reachable from the directory viewer

#### Scenario: right-click on Linux produces no menu

- **WHEN** the Linux build is rendering any file and the user right-clicks inside it
- **THEN** no plugin-defined popup menu appears
