

const markdownAlertTypes = {
  NOTE: {
    name:"note",
    text:"NOTE",
    icon:'<svg class="octicon octicon-info mr-2" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path d="M0 8a8 8 0 1 1 16 0A8 8 0 0 1 0 8Zm8-6.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13ZM6.5 7.75A.75.75 0 0 1 7.25 7h1a.75.75 0 0 1 .75.75v2.75h.25a.75.75 0 0 1 0 1.5h-2a.75.75 0 0 1 0-1.5h.25v-2h-.25a.75.75 0 0 1-.75-.75ZM8 6a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z"></path></svg>'
  },
  TIP: {
    name:"tip",
    text:"TIP",
    icon:'<svg class="octicon octicon-light-bulb mr-2" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path d="M8 1.5c-2.363 0-4 1.69-4 3.75 0 .984.424 1.625.984 2.304l.214.253c.223.264.47.556.673.848.284.411.537.896.621 1.49a.75.75 0 0 1-1.484.211c-.04-.282-.163-.547-.37-.847a8.456 8.456 0 0 0-.542-.68c-.084-.1-.173-.205-.268-.32C3.201 7.75 2.5 6.766 2.5 5.25 2.5 2.31 4.863 0 8 0s5.5 2.31 5.5 5.25c0 1.516-.701 2.5-1.328 3.259-.095.115-.184.22-.268.319-.207.245-.383.453-.541.681-.208.3-.33.565-.37.847a.751.751 0 0 1-1.485-.212c.084-.593.337-1.078.621-1.489.203-.292.45-.584.673-.848.075-.088.147-.173.213-.253.561-.679.985-1.32.985-2.304 0-2.06-1.637-3.75-4-3.75ZM5.75 12h4.5a.75.75 0 0 1 0 1.5h-4.5a.75.75 0 0 1 0-1.5ZM6 15.25a.75.75 0 0 1 .75-.75h2.5a.75.75 0 0 1 0 1.5h-2.5a.75.75 0 0 1-.75-.75Z"></path></svg>'
  },
  IMPORTANT: {
    name:"important",
    text:"IMPORTANT",
    icon:'<svg class="octicon octicon-report mr-2" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path d="M0 1.75C0 .784.784 0 1.75 0h12.5C15.216 0 16 .784 16 1.75v9.5A1.75 1.75 0 0 1 14.25 13H8.06l-2.573 2.573A1.458 1.458 0 0 1 3 14.543V13H1.75A1.75 1.75 0 0 1 0 11.25Zm1.75-.25a.25.25 0 0 0-.25.25v9.5c0 .138.112.25.25.25h2a.75.75 0 0 1 .75.75v2.19l2.72-2.72a.749.749 0 0 1 .53-.22h6.5a.25.25 0 0 0 .25-.25v-9.5a.25.25 0 0 0-.25-.25Zm7 2.25v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 9a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z"></path></svg>'
  },
  WARNING: {
    name:"warning",
    text:"WARNING",
    icon:'<svg class="octicon octicon-alert mr-2" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path d="M6.457 1.047c.659-1.234 2.427-1.234 3.086 0l6.082 11.378A1.75 1.75 0 0 1 14.082 15H1.918a1.75 1.75 0 0 1-1.543-2.575Zm1.763.707a.25.25 0 0 0-.44 0L1.698 13.132a.25.25 0 0 0 .22.368h12.164a.25.25 0 0 0 .22-.368Zm.53 3.996v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 11a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z"></path></svg>'
  },
  CAUTION: {
    name:"caution",
    text:"CAUTION",
    icon:'<svg class="octicon octicon-stop mr-2" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path d="M4.47.22A.749.749 0 0 1 5 0h6c.199 0 .389.079.53.22l4.25 4.25c.141.14.22.331.22.53v6a.749.749 0 0 1-.22.53l-4.25 4.25A.749.749 0 0 1 11 16H5a.749.749 0 0 1-.53-.22L.22 11.53A.749.749 0 0 1 0 11V5c0-.199.079-.389.22-.53Zm.84 1.28L1.5 5.31v5.38l3.81 3.81h5.38l3.81-3.81V5.31L10.69 1.5ZM8 4a.75.75 0 0 1 .75.75v3.5a.75.75 0 0 1-1.5 0v-3.5A.75.75 0 0 1 8 4Zm0 8a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z"></path></svg>'
  }
};
const markdownAlertExtension = {
  name: "markdown-alert",
  level: "block",
  start(src) {
    return src.match(/^> \[!.+\]/)?.index;
  },
  tokenizer(src) {
    const regexp = /^> \[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*\n((?:>.*\n?)*)/;
    const match = regexp.exec(src);
    if (match) {
      const [raw, type, _content] = match;
      const rawContent = _content
        .split("\n")
        .map(line => line.replace(/^> ?/, ""))
        .join("\n");

      return {
        type: "markdown-alert",
        raw: raw,
        markdownAlertType: markdownAlertTypes[type],
        text: rawContent,
        tokens: this.lexer.blockTokens(rawContent)
      };
    }
  },
  renderer(token) {
    const {name: typeName, text: title, icon} = token.markdownAlertType;
    return `
<div class="markdown-alert markdown-alert-${typeName}">
  <p class="markdown-alert-title">${icon}${title}</p>
  ${this.parser.parse(token.tokens)}
</div>
`;
  }
};

function escapeHtml(str){
  const entityList = [
    {char:"&",entity:"&amp;"},
    {char:"<",entity:"&lt;"},
    {char:">",entity:"&gt;"},
    {char:"\"",entity:"&quot;"},
    {char:"\x20",entity:"&nbsp;"},
    {char:"–",entity:"&ndash;"},
    {char:"—",entity:"&mdash;"},
    {char:"©",entity:"&copy;"},
    {char:"®",entity:"&reg;"},
    {char:"™",entity:"&trade;"},
    {char:"≈",entity:"&asymp;"},
    {char:"≠",entity:"&ne;"},
    {char:"£",entity:"&pound;"},
    {char:"€",entity:"&euro;"},
    {char:"°",entity:"&deg;"}
  ];
  return entityList.reduce((s,{char, entity}) => s.replaceAll(char, entity),str);
}
function slugify(text) {
  return text
    .toLowerCase()
    .trim()
    .replace(/[^\w\s-]/g, '')
    .replace(/[\s_-]+/g, '-')
    .replace(/^-+|-+$/g, '');
}

const customRenderer = {
  heading({ tokens, depth, raw }) {
    const id = slugify(raw);
    return `<h${depth} id="${id}">${this.parser.parseInline(tokens)}</h${depth}>\n`;
  },
  code({ text, lang }) {
    const _lang = (lang || '').toLowerCase();
    if (_lang === 'mermaid') {
      return `<pre class="mermaid">${escapeHtml(text)}</pre>\n`;
    }
    return false;
  }
};

const _mdNav = { stack: [], pos: -1 };

// XHR-based fetch: Qt Web Engine's fetch() cannot reach custom schemes (the
// fetch allowlist ignores registerScheme) but XMLHttpRequest works for ev://
// on Linux and http://local.example on Windows (WebView2). Used for
// in-viewer cross-file navigation.
function _mdEvFetch(url) {
  return new Promise(function (resolve, reject) {
    const x = new XMLHttpRequest();
    x.open('GET', url);
    x.responseType = 'arraybuffer';
    x.onload = function () {
      if (x.status >= 200 && x.status < 300)
        resolve(new Uint8Array(x.response ? x.response : []));
      else reject(new Error('HTTP ' + x.status));
    };
    x.onerror = function () { reject(new Error('net error')); };
    x.send();
  });
}

function _mdNavigate(href) {
  const url = new URL(href, document.baseURI);
  _mdEvFetch(url.href).then(async t => {
    const decoder = new TextDecoder(detect_charset(t));
    const html = marked.parse(decoder.decode(t).replace(/^---\r?\n[\s\S]*?\n---\r?\n?/, ''));
    document.getElementById('content').innerHTML = html;
    window.scrollTo(0, 0);

    try { MathJax.typeset(); } catch(err) {}
    try { mermaid.initialize({ startOnLoad: false }); await mermaid.run(); } catch(err) {}
    try { hljs.highlightAll(); } catch(err) {}

    const hash = url.hash;
    if (hash) {
      const el = document.getElementById(hash.slice(1));
      if (el) el.scrollIntoView();
    }

    _mdUpdateButtons();
  }).catch(() => {});
}

function _mdPushNav(href) {
  _mdNav.stack = _mdNav.stack.slice(0, _mdNav.pos + 1);
  _mdNav.stack.push(href);
  _mdNav.pos = _mdNav.stack.length - 1;
  _mdUpdateButtons();
}

function _mdGoBack() {
  if (_mdNav.pos <= 0) return;
  _mdNav.pos--;
  _mdNavigate(_mdNav.stack[_mdNav.pos]);
}

function _mdGoForward() {
  if (_mdNav.pos >= _mdNav.stack.length - 1) return;
  _mdNav.pos++;
  _mdNavigate(_mdNav.stack[_mdNav.pos]);
}

function _mdUpdateButtons() {
  const bar = document.getElementById('md-nav-bar');
  if (!bar) return;
  const backBtn = document.getElementById('md-nav-back');
  const fwdBtn = document.getElementById('md-nav-fwd');
  const info = document.getElementById('md-nav-info');
  if (backBtn) backBtn.style.opacity = _mdNav.pos > 0 ? '1' : '0.3';
  if (fwdBtn) fwdBtn.style.opacity = _mdNav.pos < _mdNav.stack.length - 1 ? '1' : '0.3';
  if (info) info.textContent = _mdNav.stack.length > 1
    ? (_mdNav.pos + 1) + '/' + _mdNav.stack.length
    : '';
}

document.addEventListener('DOMContentLoaded', function() {
  if (!window.__MD_NAVIGATION__) return;
  const bar = document.createElement('div');
  bar.id = 'md-nav-bar';
  bar.style.cssText = 'position:fixed;top:0;right:0;z-index:9999;display:flex;gap:4px;padding:6px 8px;font-family:sans-serif;user-select:none;';
  bar.innerHTML = '<button id="md-nav-back" onclick="_mdGoBack()" style="width:28px;height:28px;border:none;border-radius:4px;background:rgba(128,128,128,0.15);cursor:pointer;font-size:16px;opacity:0.3" title="Back">\u2190</button>' +
    '<button id="md-nav-fwd" onclick="_mdGoForward()" style="width:28px;height:28px;border:none;border-radius:4px;background:rgba(128,128,128,0.15);cursor:pointer;font-size:16px;opacity:0.3" title="Forward">\u2192</button>' +
    '<span id="md-nav-info" style="line-height:28px;font-size:11px;color:#888;padding:0 4px"></span>';
  document.body.appendChild(bar);
});

document.addEventListener('click', (e) => {
  const link = e.target.closest('a');
  if (!link) return;

  const href = link.getAttribute('href');
  if (!href) return;

  if (href.startsWith('#')) {
    e.preventDefault();
    const id = href.slice(1);
    const el = document.getElementById(id);
    if (el) el.scrollIntoView();
    return;
  }

  if (/\.md(#.*)?$/i.test(href)) {
    const url = new URL(href, document.baseURI);
    const curDir = new URL(document.baseURI).pathname.replace(/\/[^/]*$/, '/');
    const linkDir = url.pathname.replace(/\/[^/]*$/, '/');
    if (curDir === linkDir) {
      e.preventDefault();
      if (window.__MD_NAVIGATION__) _mdPushNav(url.href);
      _mdNavigate(url.href);
    }
  }
});
