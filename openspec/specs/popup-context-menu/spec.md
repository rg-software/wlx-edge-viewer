# popup-context-menu Specification

## Purpose
Characterizes the existing pop-up context menu capability of the Total Commander WLX Lister plugin: how a right-click on a file entry inside the directory viewer triggers the native Windows Explorer shell context menu for that file, how the menu is built from the shell namespace, how a chosen command is invoked on the right-clicked file, and how the COM lifetime and PIDL memory are managed across one menu invocation. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin.
## Requirements
### Requirement: Shell context menu invocation from the directory viewer

The pop-up context menu SHALL only be reachable from the directory viewer. When the user right-clicks a file entry inside the rendered directory listing, the directory processor SHALL send a menu command to the lister window, and that window SHALL display the native Windows Explorer shell context menu for the right-clicked file. Right-clicking inside other processors (Markdown, AsciiDoc, RST, HTML, MHT, images, PDF) SHALL NOT trigger this menu.

#### Scenario: right-click in directory view

- **WHEN** the directory viewer is rendering a directory listing and the user right-clicks one of the file entries
- **THEN** the directory processor sends a menu command to the lister window and the lister window displays the Windows Explorer shell context menu for the right-clicked file

#### Scenario: right-click in a Markdown document

- **WHEN** a Markdown document is being rendered and the user right-clicks inside it
- **THEN** the pop-up shell context menu is not displayed, because the menu is only reachable from the directory viewer

### Requirement: Shell context menu content

The pop-up menu SHALL be the standard Windows Explorer shell context menu for the right-clicked file. It SHALL be built by walking the shell namespace: the desktop folder is obtained, the parent directory is parsed into a child shell folder, the right-clicked file is parsed into an item identifier list inside that parent shell folder, and the resulting item identifier list is queried for its context-menu interface. The menu SHALL be populated with the file's standard Explorer commands (Open, Cut, Copy, Paste, Properties, the file's shell verbs supplied by registered handlers, etc.) at the normal position and with the normal icons.

#### Scenario: standard Explorer commands

- **WHEN** the pop-up menu is displayed for a regular file
- **THEN** the menu shows the standard Windows Explorer entries for that file (for example Open, Copy, Properties), in the same order and with the same icons as in Windows Explorer

#### Scenario: registered shell verbs appear

- **WHEN** the right-clicked file has shell verbs registered by other applications (for example a version-control "commit" verb)
- **THEN** those verbs appear in the pop-up menu at their usual position, because the menu is built from the file's real shell context menu

#### Scenario: menu contents follow file type

- **WHEN** the right-clicked file is of a different type from a previously right-clicked file (for example an `.exe` versus a `.txt`)
- **THEN** the pop-up menu's contents reflect the new file's type, because the menu is built from the new file's own shell context menu

### Requirement: Shell context menu command execution

When the user selects a menu item, the plugin SHALL invoke the corresponding shell command against the right-clicked file. The selected command SHALL be invoked with the cursor's screen position as the invocation point and the normal "shown normal" show flag, so any window the command opens appears at the cursor and is activated in the usual way. Cancelling the menu by clicking outside it or pressing Escape SHALL NOT invoke any command.

#### Scenario: user selects a command

- **WHEN** the pop-up menu is displayed and the user selects one of its commands
- **THEN** the plugin invokes the corresponding shell verb on the right-clicked file, with the cursor's screen position as the invocation point

#### Scenario: user cancels the menu

- **WHEN** the pop-up menu is displayed and the user cancels it (by clicking outside it or pressing Escape)
- **THEN** no shell command is invoked, because the cancel returns a sentinel command identifier that the plugin does not act on

### Requirement: COM lifetime and PIDL memory management

Each pop-up menu invocation SHALL establish an apartment-threaded COM session for the duration of the call, build the menu, track the menu, handle the selected command (if any), and then tear the COM session down. Two apartment flags SHALL be requested together so that the OLE1 DDE layer is disabled while the shell namespace is being walked. Every item identifier list produced while building the menu SHALL be freed through the COM task allocator before the call returns, so a menu invocation SHALL produce no PIDL leaks even if the user cancels the menu.

#### Scenario: COM init and uninit bracket one menu call

- **WHEN** the plugin displays a pop-up menu and the user either selects a command or cancels
- **THEN** the COM session that was initialized for the menu call is uninitialized before the menu call returns, so COM is left in the same state as before the call

#### Scenario: PIDL memory is freed

- **WHEN** the pop-up menu call produced item identifier lists for the parent folder and the file
- **THEN** each of those item identifier lists is freed through the COM task allocator before the menu call returns, regardless of whether a command was selected

### Requirement: 32-bit and 64-bit shell menu parity

The pop-up menu invocation, the shell namespace walk, the menu's contents, the selected-command invocation and the COM and PIDL lifetime management SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL use the same COM calls, the same shell folder walk and the same menu tracking; the only difference SHALL be the bitness of the pointers in the PIDLs, which is transparent to the shell APIs used.

#### Scenario: Win32 build pop-up menu

- **WHEN** the 32-bit plugin is loaded and the user right-clicks a file in the directory viewer
- **THEN** the plugin displays the standard Windows Explorer context menu for that file, the same as the 64-bit build does for the same file

#### Scenario: x64 build pop-up menu command

- **WHEN** the 64-bit plugin is loaded and the user selects a command from the pop-up menu
- **THEN** the plugin invokes the corresponding shell verb with the cursor's screen position as the invocation point, the same as the 32-bit build does for the same selection

