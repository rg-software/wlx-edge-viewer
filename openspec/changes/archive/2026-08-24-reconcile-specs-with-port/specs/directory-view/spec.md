## MODIFIED Requirements

### Requirement: Directory thumbnail for subdirectories

For each subdirectory entry, the directory processor MUST produce a thumbnail that anchors the entry. On Windows, when the `[Directory]` option `GenDirThumbs=1` is active, the processor MUST request a shell thumbnail for that directory by creating a shell item from the parsing name and using the shell item image factory to obtain a thumbnail of the size configured by `DirThumbSize` (default `256`) with the bigger-size-allowed thumbnail-only flags, then MUST encode that bitmap to PNG via GDI+, base64-encode it, and embed it as a `data:` URI directly in the generated HTML. When `GenDirThumbs=0`, the processor MUST instead use the static `folder.png` icon bundled under `Resources/assets/dirviewer/`. The thumbnail generation behavior SHALL be identical on the 32-bit and 64-bit builds, even where the underlying shell apis differ between bitnesses.

On Linux, dynamic shell thumbnails are not implemented (future-work #2 in `Readme.md`); the `GenDirThumbs` key is silently ignored and the static `folder.png`/`file.png` icons are always used.

#### Scenario: GenDirThumbs=1 produces a live shell thumbnail

- **WHEN** `GenDirThumbs=1` and `DirThumbSize=256` are configured and a subdirectory `Images` is being listed
- **THEN** the directory processor MUST request a shell thumbnail of size 256x256 for `Images`, encode it as a base64 PNG embedded through a `data:` URI, and use that as the entry's thumbnail in the rendered HTML

#### Scenario: GenDirThumbs=0 uses the static folder icon

- **WHEN** `GenDirThumbs=0` is configured and a subdirectory `Images` is being listed
- **THEN** the directory processor MUST use the static `folder.png` icon under `Resources/assets/dirviewer/` as the entry's thumbnail instead of calling the shell item image factory

#### Scenario: Linux always uses static icons

- **WHEN** the Linux build lists a directory with `GenDirThumbs=1`
- **THEN** the static `folder.png`/`file.png` icons are used, and no shell thumbnail is generated
