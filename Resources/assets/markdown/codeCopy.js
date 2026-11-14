/*
 * codeCopy.js - "Copy" button for Markdown code blocks in EdgeViewer.wlx.
 *
 * Wraps every <pre><code> in <div class="md-code-block"> and adds
 * <button class="md-copy-btn"> with an SVG icon. No styles here: the look is
 * set in the plugin's CSS theme (assets\markdown\*.css), see README.md.
 *
 * Include in loader.html inside <body>, AFTER <div id="content">
 * and BEFORE the main <script> that calls marked.parse.
 */
(() => {
  'use strict';

  const LABEL = 'Copy';
  const LABEL_DONE = 'Copied!';
  const LABEL_FAIL = 'Copy failed';
  const RESET_MS = 2000;

  // Octicons "copy" and "check" (MIT), https://github.com/primer/octicons
  const svg = (...paths) => '<svg class="md-copy-icon" aria-hidden="true" viewBox="0 0 16 16" width="16" height="16">' +
    paths.map(d => `<path fill="currentColor" d="${d}"/>`).join('') + '</svg>';
  const ICON_COPY = svg(
    'M0 6.75C0 5.784.784 5 1.75 5h1.5a.75.75 0 0 1 0 1.5h-1.5a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-1.5a.75.75 0 0 1 1.5 0v1.5A1.75 1.75 0 0 1 9.25 16h-7.5A1.75 1.75 0 0 1 0 14.25Z',
    'M5 1.75C5 .784 5.784 0 6.75 0h7.5C15.216 0 16 .784 16 1.75v7.5A1.75 1.75 0 0 1 14.25 11h-7.5A1.75 1.75 0 0 1 5 9.25Zm1.75-.25a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-7.5a.25.25 0 0 0-.25-.25Z');
  const ICON_DONE = svg(
    'M13.78 4.22a.75.75 0 0 1 0 1.06l-7.25 7.25a.75.75 0 0 1-1.06 0L2.22 9.28a.751.751 0 0 1 .018-1.042.751.751 0 0 1 1.042-.018L6 10.94l6.72-6.72a.75.75 0 0 1 1.06 0Z');

  const STATES = {
    copy: [ICON_COPY, LABEL],
    done: [ICON_DONE, LABEL_DONE],
    fail: [ICON_COPY, LABEL_FAIL]
  };

  // The page is loaded with NavigateToString, so navigator.clipboard may be
  // unavailable (not a secure context); execCommand('copy') works there.
  function copyLegacy(text) {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.cssText = 'position:fixed;top:0;opacity:0';
    document.body.append(ta);
    ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    return ok;
  }

  const copyText = text => window.isSecureContext && navigator.clipboard
    ? navigator.clipboard.writeText(text).then(() => true, () => copyLegacy(text))
    : Promise.resolve(copyLegacy(text));

  function setState(btn, state) {
    const [icon, label] = STATES[state];
    btn.classList.toggle('md-copy-done', state === 'done');
    btn.classList.toggle('md-copy-fail', state === 'fail');
    btn.innerHTML = icon;
    btn.title = label;
    btn.setAttribute('aria-label', label);
  }

  async function onClick() {
    // marked keeps a trailing newline inside <code>; drop it.
    const text = this.parentNode.querySelector('code').textContent.replace(/\n$/, '');
    setState(this, (await copyText(text)) ? 'done' : 'fail');
    clearTimeout(this.resetTimer);
    this.resetTimer = setTimeout(() => setState(this, 'copy'), RESET_MS);
  }

  const content = document.getElementById('content');

  function decorate() {
    for (const code of content.querySelectorAll('pre > code')) {
      const pre = code.parentNode;
      // Skip Mermaid diagrams and blocks that already have a button.
      if (pre.classList.contains('mermaid') || pre.parentNode.classList.contains('md-code-block')) continue;
      const wrap = document.createElement('div');
      const btn = document.createElement('button');
      wrap.className = 'md-code-block';
      btn.type = 'button';
      btn.className = 'md-copy-btn';
      btn.addEventListener('click', onClick);
      setState(btn, 'copy');
      pre.before(wrap);
      wrap.append(pre, btn);
    }
  }

  // First render and in-viewer navigation (_mdNavigate) both replace
  // #content.innerHTML. No subtree: ignore highlight.js / MathJax edits.
  new MutationObserver(decorate).observe(content, { childList: true });
})();
