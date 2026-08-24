## MODIFIED Requirements

### Requirement: ForcedHtmlExt temp-copy

On Windows, the `ForcedHtmlExt` regular expression in `edgeviewer.ini` SHALL list extensions (for example `xml|xhtml`) whose content SHALL be loaded as HTML by the web engine. When the supplied file's extension matches that regular expression, the plugin SHALL copy the file into the temp directory with an `.html` extension and SHALL render the temp copy, so the engine's content-type sniffing settles on HTML.

On Linux, the temp-copy path is not implemented (`Platform_Linux.cpp::GetPhysicalPath` does not perform the regex check); the `ev://` scheme handler's default `Content-Type: text/html` achieves the same user-visible result for HTML-sniffable content.

#### Scenario: XML file forced to HTML

- **WHEN** the supplied file has an extension that matches the `ForcedHtmlExt` regular expression (for example `.xml`)
- **THEN** the plugin copies the file into the temp directory with an `.html` extension and renders the temp copy as HTML

#### Scenario: ordinary extension is not forced

- **WHEN** the supplied file has an extension that does not match the `ForcedHtmlExt` regular expression
- **THEN** the plugin does not copy the file to a `.html` temp file; the file is rendered through its normal processor

### Requirement: Temp file cleanup

The plugin SHALL track every temp file it creates. The temp files MAY be removed on demand by walking the tracking list and deleting each file. On Windows, this same removal routine SHALL be invoked when the plugin is unloaded, gated by the `[WebView]` `CleanupOnExit` key in `edgeviewer.ini`: when that key is set to 1, the plugin SHALL remove all tracked temp files during process detach; when the key is unset or 0, the tracked temp files SHALL be left on disk for the operating system's own temp cleanup to reclaim. On Linux there is no `DLL_PROCESS_DETACH`, so `RemoveTempFiles()` is reachable but never auto-called; `gs_tempFiles` accumulates across the plugin's lifetime in Double Commander.

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

### Requirement: 32-bit and 64-bit temp and path parity

The symlink resolution rules, the UNC and ForcedHtmlExt temp-copy rules, the temp file generation procedure, the cleanup rules gated by `CleanupOnExit`, the EBWebView directory removal rules and the path-prefix stripping SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read the same `edgeviewer.ini` keys and both builds SHALL use the same Windows APIs for path handling, so a user's temp-file layout and cleanup behavior SHALL be the same regardless of which build is loaded.

#### Scenario: Win32 build UNC copy

- **WHEN** the 32-bit plugin is loaded and is asked to render a file at a `\\?\UNC\` path
- **THEN** the plugin copies the file into the temp directory and renders the temp copy, matching the 64-bit build's behavior

#### Scenario: x64 build cleanup at exit

- **WHEN** the 64-bit plugin is loaded with `[WebView]` `CleanupOnExit=1` and is being unloaded
- **THEN** the plugin removes the tracked temp files and the `EBWebView` directory, matching the 32-bit build's behavior for the same configuration
