## MODIFIED Requirements

### Requirement: UNC path served in place

When the plugin is asked to render a file at a UNC path (under the `\\?\UNC\` namespace or as a plain `\\server\share\...` path) that is not a directory, the plugin SHALL NOT copy the file to a temporary location. The plugin SHALL use the UNC path itself for rendering: the bytes the engine reads SHALL come from the share, read by the plugin's own process (which carries the user's credentials for the share).

Because the WebView2 renderer cannot access a UNC share directly — `SetVirtualHostNameToFolderMapping` only maps local folders and the renderer process has no credentials for the share — the plugin SHALL route UNC-rooted content through a custom URI scheme (`evh://` on Windows) whose `WebResourceRequested` handler resolves the relative path against the registered folder (the share root) and reads the file in the plugin process. Relative references inside the document (images, CSS, sibling links) SHALL therefore resolve against the real share directory, not a temporary location. On Linux, all shares are mounted at local paths and the `ev://` scheme handler already reads every file host-side, so no equivalent routing or temp copy is needed.

#### Scenario: UNC file rendered from the share

- **WHEN** the supplied path is a UNC path prefixed with `\\?\UNC\` that points to a regular file
- **THEN** the plugin resolves it to the plain `\\server\share\...` form, serves the document through the host-side `evh://` scheme, and relative subresources resolve against the share's directory

#### Scenario: UNC directory is not copied

- **WHEN** the supplied path is a UNC path prefixed with `\\?\UNC\` that points to a directory
- **THEN** the plugin uses the UNC directory path as-is because directory paths are handled by the directory viewer, which does not need the file bytes

### Requirement: ForcedHtmlExt served in place

On Windows, the `ForcedHtmlExt` regular expression in `edgeviewer.ini` SHALL list extensions (for example `xml|xhtml`) whose content SHALL be loaded as HTML by the web engine. A file whose extension matches that regular expression SHALL NOT be copied to a temporary location: it SHALL be served from its real filesystem location through the host-side `evh://` scheme, whose handler answers with `Content-Type: text/html` (the `local.example` virtual host would serve `.xml`/`.xhtml` with their raw MIME). Relative subresources inside the forced document SHALL resolve against the source directory.

On Linux, the `ev://` scheme handler's default `Content-Type: text/html` achieves the same user-visible result for HTML-sniffable content.

#### Scenario: XML file forced to HTML

- **WHEN** the supplied file has an extension that matches the `ForcedHtmlExt` regular expression (for example `.xml`)
- **THEN** the plugin serves the file from its real location with `Content-Type: text/html` and it renders as HTML

#### Scenario: ordinary extension is not forced

- **WHEN** the supplied file has an extension that does not match the `ForcedHtmlExt` regular expression
- **THEN** the plugin renders the file through its normal processor without any special content-type handling

### Requirement: Temp file generation

Temp files remain the mechanism for the oversized-loader path only: the Windows backend writes loader HTML that exceeds `NavigateToString`'s 2 MB wchar cap to the temp directory and serves it via the `lister.example` virtual host. Temp files SHALL be produced by combining the system's temp path with a generated temp file name, appending the original file's extension, copying the original file contents over, and recording the resulting temp file path in the plugin's temp-file tracking list. UNC and `ForcedHtmlExt` files are served in place and are never copied, so they SHALL NOT add entries to the tracking list.

#### Scenario: temp file creation

- **WHEN** the plugin needs a temp copy of a file (currently only the oversized-loader path past `NavigateToString`'s 2 MB cap; UNC and ForcedHtmlExt files are served in place and are never copied)
- **THEN** the plugin creates a temp file under the system's temp directory, appends the original file's extension, copies the original file's bytes into the temp file and records the temp file's path for later cleanup

#### Scenario: temp file copy failure

- **WHEN** the file-copy step fails (for example because the source is unreadable)
- **THEN** the plugin does not produce a usable temp copy and the rendering of that file fails, but the recorded temp file path (if any) SHALL still be eligible for later cleanup

### Requirement: 32-bit and 64-bit temp and path parity

The symlink resolution rules, the UNC and ForcedHtmlExt in-place serving rules, the temp file generation procedure, the cleanup rules gated by `CleanupOnExit`, the EBWebView directory removal rules and the path-prefix stripping SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read the same `edgeviewer.ini` keys and both builds SHALL use the same Windows APIs for path handling, so a user's temp-file layout and cleanup behavior SHALL be the same regardless of which build is loaded.

#### Scenario: Win32 build UNC in-place serving

- **WHEN** the 32-bit plugin is loaded and is asked to render a file at a `\\?\UNC\` path
- **THEN** the plugin renders the file from the share through the host-side `evh://` scheme without copying it, matching the 64-bit build's behavior

#### Scenario: x64 build cleanup at exit

- **WHEN** the 64-bit plugin is loaded with `[WebView]` `CleanupOnExit=1` and is being unloaded
- **THEN** the plugin removes the tracked temp files and the `EBWebView` directory, matching the 32-bit build's behavior for the same configuration