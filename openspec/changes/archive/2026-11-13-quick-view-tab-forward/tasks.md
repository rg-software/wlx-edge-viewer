# Tasks

## 1. Quick-view mode detection and ini option

- [x] 1.1 Record per-view quick-view mode from `ParentWin` style and verify each open view keeps its own mode across navigation and drops it on close
- [x] 1.2 Add shipped `[QuickView] PassTabToTC=1` (with comment) to `Resources/edgeviewer.ini` and verify the sample parses and an absent key degrades to in-page Tab

## 2. Tab intercept and docs

- [x] 2.1 Suppress plain `Tab`/`Shift+Tab` page-side in quick-view views with the option on (per-view `__evTabForward` flag baked into the document keydown bridge + `preventDefault`, `CMD_TAB` forwarded as genuine `SendInput` Tab after moving focus out; `Ctrl`/`Alt`/`Meta`-held presses skipped), and verify F3 views and option-off views keep in-page link navigation
- [x] 2.2 Document the F3 vs quick-view `Tab` behavior and the ini key in `Readme.md` and verify the documented key name matches the shipped sample

## 3. Verify

- [x] 3.1 Build Release for Win32 and x64 and verify both `EdgeViewer.wlx` / `EdgeViewer.wlx64` outputs succeed
- [x] 3.2 Manual check in Total Commander with an `Examples/` file and verify F3 `Tab` navigates links, quick-view `Tab` switches panels with `=1` and navigates with `=0`, and `Ctrl+Tab` is unchanged in both modes
