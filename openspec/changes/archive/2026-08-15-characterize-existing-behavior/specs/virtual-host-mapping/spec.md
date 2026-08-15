## Purpose

Defines how the plugin exposes local content to the WebView2 renderer through two fixed virtual hosts — `assets.example` pointing at the plugin's bundled `Resources/assets/` libraries and `local.example` pointing at the root of the drive the viewed file lives on — so renderer HTML/JS/CSS can load file-system resources by URL without broad filesystem access, identically on 32-bit and 64-bit builds.

## ADDED Requirements

### Requirement: Assets virtual host mapping

The plugin MUST map the virtual host name `assets.example` to the `assets` folder located alongside the plugin DLL (the deployed copy of the `Resources/assets/` source tree, located via `EdgeViewer/Processors/ProcessorInterface.cpp`) using WebView2's `SetVirtualHostNameToFolderMapping` with the `ALLOW` access kind. This SHALL let every processor's loader HTML reference the shared rendering libraries (such as `marked.js`, `highlight.js`, `asciidoctor.js`, `mermaid`, `mathjax`, and `mhtml2html` under `Resources/assets/<type>/`) as `http://assets.example/<type>/<file>`. The mapping SHALL be established for every web view before any content is loaded into it, and SHALL be identical on 32-bit and 64-bit builds — the assets folder path is derived from the loaded module's location, not from a hardcoded absolute path.

#### Scenario: Renderer fetches a shared library

- **WHEN** a loader template references `http://assets.example/highlight/highlight.min.js`
- **THEN** WebView2 serves the file from `<plugin dir>/assets/highlight/highlight.min.js` (the deployed `Resources/assets/highlight/`) without any plugin-side request handler

#### Scenario: Plugin relocated to a different folder

- **WHEN** the plugin DLL and its `assets` folder are moved to a different directory and loaded from there
- **THEN** `assets.example` still resolves to whatever `assets` folder now sits next to the DLL, because the path is derived from the module location

### Requirement: Local file virtual host mapping

The plugin MUST map the virtual host name `local.example` to the root directory of the drive the currently viewed file lives on (the file's root path, e.g. `C:\` for a file at `C:\Users\test\readme.md`), using WebView2's `SetVirtualHostNameToFolderMapping` with the `ALLOW` access kind. This SHALL let a loader template reference the viewed file and its sibling/relative resources as `http://local.example/<relative path from the drive root>`. The mapping SHALL be established for every web view before content is loaded, and SHALL resolve the same root on 32-bit and 64-bit builds.

#### Scenario: Viewing a file on the C: drive

- **WHEN** the user opens `C:\Users\test\readme.md`
- **THEN** `local.example` maps to `C:\` and the loader can fetch the file at `http://local.example/Users/test/readme.md`

#### Scenario: Viewing a file on a different drive

- **WHEN** the user opens `D:\docs\notes.md`
- **THEN** `local.example` maps to `D:\` for that view, independent of any previously opened file on another drive

### Requirement: Every processor maps both domains before loading content

Every file-type processor under `EdgeViewer/Processors/` — Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, Images, Directory, and the PDF/Other fallback — MUST establish both the `assets.example` and `local.example` virtual-host-to-folder mappings inside its rendering path (in `OpenIn`) before it navigates the web view to the loader template or the file URL. No processor SHALL rely on a mapping it did not establish itself, and no processor SHALL load content before the mappings are in place. This SHALL hold for both 32-bit and 64-bit builds.

#### Scenario: Markdown processor loads a file

- **WHEN** the Markdown processor opens a `.md` file
- **THEN** both `assets.example` (→ `<plugin dir>/assets`) and `local.example` (→ the file's drive root) are mapped before the loader template is navigated

#### Scenario: Directory processor loads a folder

- **WHEN** the Directory processor opens a directory for thumbnail rendering
- **THEN** both virtual host mappings are established before the directory viewer content is loaded

#### Scenario: PDF/Other fallback processor loads a file

- **WHEN** the fallback processor opens a `.pdf` file
- **THEN** both virtual host mappings are established before the PDF is rendered

### Requirement: Mapped folder resources bypass request interception

Resources served from a folder mapped via `SetVirtualHostNameToFolderMapping` (i.e., any URL under `http://assets.example/` or `http://local.example/`) MUST be served directly by WebView2 and SHALL NOT trigger the plugin's `WebResourceRequested` event handler. The plugin's request interceptor (installed in `EdgeViewer/WebView2.cpp` with `AddWebResourceRequestedFilter` for the `ALL` resource context) SHALL only observe and act on requests to hosts that are NOT mapped — e.g. `http://html.example/` (used for the HTML encoding-override passthrough) and any non-local external URL (gated by the `[Chromium] OfflineMode` ini key). This SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Request to a mapped host is not intercepted

- **WHEN** the renderer requests `http://assets.example/markdown/marked.js` or `http://local.example/Users/test/readme.md`
- **THEN** WebView2 serves the file from the mapped folder and the plugin's `WebResourceRequested` handler is not invoked for that request

#### Scenario: Request to a non-mapped host is intercepted

- **WHEN** the renderer requests `http://html.example/index.html` (a host with no folder mapping)
- **THEN** the plugin's `WebResourceRequested` handler receives the request and applies the encoding-override passthrough

#### Scenario: External request is blocked when OfflineMode is set

- **WHEN** `[Chromium] OfflineMode=1` is set and the renderer requests an external (non-mapped) URL
- **THEN** the plugin's `WebResourceRequested` handler returns a `403 Blocked` response instead of letting WebView2 fetch it