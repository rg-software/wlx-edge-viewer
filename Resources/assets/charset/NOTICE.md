# Third-party notice

This directory contains vendored third-party assets used by the plugin.

## jschardet (charset auto-detection)

- Source: https://github.com/aadsm/jschardet
- Version: 3.1.4
- License: GNU Lesser General Public License v2.1 or later (LGPL-2.1+)
- File: `jschardet.min.js`, license text in `LICENSE-jschardet.txt`

The plugin bundles and serves `jschardet.min.js` at runtime (loaded as a
browser script by the HTML charset auto-detection glue, `autodetect.js`). In
compliance with LGPL v2.1 section 6 (and to permit application relinking), the
complete LGPL-2.1 license text is distributed alongside in
`LICENSE-jschardet.txt`, and the unminified/replaceable source is available
from the upstream project at the URL above. No modifications are made to the
library source.

`autodetect.js` is EdgeViewer's own glue and carries no third-party license.