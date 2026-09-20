# temp-file-management Specification

## Purpose
Characterizes the existing temporary-file and path-handling capability of the Total Commander WLX Lister plugin: how the plugin resolves symlinks and junctions to their real target, how it serves files the WebView2 engine cannot load directly (UNC paths) in place through the host-side custom scheme, how the oversized-document temp files it still creates are tracked, and how they are cleaned up either on demand or at process detach. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin.
## Requirements
### Requirement: Symlink and junction resolution

Before opening a file for rendering, the plugin SHALL resolve the supplied path through the Windows file namespace. A directory symlink or a file symlink (or a junction) SHALL be resolved to the real target path by opening the path with backup semantics and asking Windows for the final path name by handle, first trying the normalized form and falling back to the opened form if the normalized form is not available. When resolution fails for any reason, the plugin SHALL fall back to the original path so rendering MAY still be attempted.

#### Scenario: file symlink resolved

- **WHEN** the supplied path points to a file that is a symlink to another file
- **THEN** the plugin resolves the symlink to its real target by querying the final path name by handle and uses the real target path for rendering

#### Scenario: directory junction resolved

- **WHEN** the supplied path points through a directory junction
- **THEN** the plugin resolves the junction to its real target directory and uses the real target path for subsequent operations

#### Scenario: symlink resolution failure

- **WHEN** the plugin cannot obtain a final path name by handle (for example because the file is not openable with backup semantics)
- **THEN** the plugin falls back to the originally supplied path so rendering can still be attempted

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

Temp files SHALL be produced by combining the system's temp path with a generated temp file name, appending the original file's extension, copying the original file contents over, and recording the resulting temp file path in the plugin's temp-file tracking list. The generated temp file name SHALL use an `UNC` prefix in its internal naming so the temp files the plugin produces are recognizable on disk.

#### Scenario: temp file creation

- **WHEN** the plugin needs a temp copy of a file (currently only the oversized-loader path past `NavigateToString`'s 2 MB cap; UNC and ForcedHtmlExt files are served in place and are never copied)
- **THEN** the plugin creates a temp file under the system's temp directory, appends the original file's extension, copies the original file's bytes into the temp file and records the temp file's path for later cleanup

#### Scenario: temp file copy failure

- **WHEN** the file-copy step fails (for example because the source is unreadable)
- **THEN** the plugin does not produce a usable temp copy and the rendering of that file fails, but the recorded temp file path (if any) SHALL still be eligible for later cleanup

### Requirement: Temp file cleanup

The plugin SHALL track every temp file it creates. The temp files MAY be removed on demand by walking the tracking list and deleting each file. On Windows, this same removal routine SHALL be invoked when the plugin is unloaded, gated by the `[WebView]` `CleanupOnExit` key in `edgeviewer.ini`: when that key is set to 1, the plugin SHALL remove all tracked temp files during process detach; when the key is unset or 0, the tracked temp files SHALL be left on disk for the operating system's own temp cleanup to reclaim. On Linux there is no `DLL_PROCESS_DETACH`, so `RemoveTempFiles()` is never auto-called; however, `GenTempFile()` currently has no call sites on the Linux path (`Platform_Linux.cpp::GetPhysicalPath` resolves symlinks only and never creates temp copies), so `gs_tempFiles` stays empty in practice and the missing auto-cleanup has no observable effect. Should a future Linux feature start creating temp files, deciding where they are cleaned up (for example per-lister clearing in `ListCloseWindow`) becomes part of that feature's design.

#### Scenario: explicit cleanup

- **WHEN** the plugin is asked to remove its tracked temp files
- **THEN** the plugin walks its tracking list and deletes each recorded temp file, leaving no plugin-produced temp file behind

#### Scenario: cleanup at exit when enabled

- **WHEN** the `[WebView]` `CleanupOnExit` key is set to 1 and the plugin is being unloaded
- **THEN** the plugin removes every tracked temp file during process detach

#### Scenario: no cleanup at exit when disabled

- **WHEN** the `[WebView]` `CleanupOnExit` key is unset or 0 and the plugin is being unloaded
- **THEN** the plugin leaves the tracked temp files on disk and lets the operating system's own temp cleanup reclaim them later

### Requirement: EBWebView cache cleanup

On Windows, the WebView2 engine stores its user data under an `EBWebView` directory inside the plugin's configured user directory. When the `[WebView]` `CleanupOnExit` key is set to 1 and the plugin is being unloaded, the plugin SHALL remove that `EBWebView` directory in addition to removing the tracked temp files. When the key is unset or 0, the `EBWebView` directory SHALL be left in place so the engine MAY reuse its cache and cookies on the next run. This requirement is Windows-only (`EBWebView` is a WebView2 artifact; the Linux `QtWebEngineBackend` has no equivalent cache directory managed by the plugin).

#### Scenario: EBWebView removed at exit when enabled

- **WHEN** the `[WebView]` `CleanupOnExit` key is set to 1 and the plugin is being unloaded
- **THEN** the plugin removes the `EBWebView` directory from its configured user directory, in addition to removing the tracked temp files

#### Scenario: EBWebView kept at exit when disabled

- **WHEN** the `[WebView]` `CleanupOnExit` key is unset or 0 and the plugin is being unloaded
- **THEN** the plugin leaves the `EBWebView` directory in place so the WebView2 engine can reuse its cache and cookies on the next run

### Requirement: Path prefix stripping

The plugin SHALL normalize the paths it returns to its own callers. When the resolved path carries a `\\?\` or `\\?\UNC\` long-path prefix, the plugin SHALL strip that prefix before returning the path, so downstream code that does not understand the long-path prefix receives a plain path. The stripping SHALL happen at the path-handling boundary, after symlink resolution but before the decision to copy a file to temp.

#### Scenario: dos-device prefix stripped

- **WHEN** the resolved path is prefixed with `\\?\`
- **THEN** the plugin strips the `\\?\` prefix before returning the path to its own callers, so the returned path looks like a plain absolute path

#### Scenario: UNC prefix stripped

- **WHEN** the resolved path is prefixed with `\\?\UNC\`
- **THEN** the plugin strips the `\\?\UNC\` prefix before returning the path to its own callers, so the returned path looks like a plain UNC path of the form `\\server\share\...`

### Requirement: 32-bit and 64-bit temp and path parity

The symlink resolution rules, the UNC and ForcedHtmlExt in-place serving rules, the temp file generation procedure, the cleanup rules gated by `CleanupOnExit`, the EBWebView directory removal rules and the path-prefix stripping SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read the same `edgeviewer.ini` keys and both builds SHALL use the same Windows APIs for path handling, so a user's temp-file layout and cleanup behavior SHALL be the same regardless of which build is loaded.

#### Scenario: Win32 build UNC in-place serving

- **WHEN** the 32-bit plugin is loaded and is asked to render a file at a `\\?\UNC\` path
- **THEN** the plugin renders the file from the share through the host-side `evh://` scheme without copying it, matching the 64-bit build's behavior

#### Scenario: x64 build cleanup at exit

- **WHEN** the 64-bit plugin is loaded with `[WebView]` `CleanupOnExit=1` and is being unloaded
- **THEN** the plugin removes the tracked temp files and the `EBWebView` directory, matching the 32-bit build's behavior for the same configuration

