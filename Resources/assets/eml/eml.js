(function () {
  'use strict';

  function escapeHtml(s) {
    return String(s == null ? '' : s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function addressText(a) {
    if (!a) return '';
    if (a.group && a.group.length) return a.group.map(addressText).join(', ');
    if (a.address) return a.name ? a.name + ' <' + a.address + '>' : a.address;
    return a.name || '';
  }

  function formatDate(date) {
    if (!date) return '';
    try { return new Date(date).toLocaleString(); } catch (e) { return String(date); }
  }

  function bytesToBase64(bytes) {
    var u8 = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    var bin = '';
    var chunk = 0x8000;
    for (var i = 0; i < u8.length; i += chunk) {
      bin += String.fromCharCode.apply(null, u8.subarray(i, i + chunk));
    }
    return btoa(bin);
  }

  // URL-safe base64 for the save message so it survives the Linux
  // ev://_cmd URL transport without percent-encoded inflation.
  function bytesToUrlSafeBase64(bytes) {
    return bytesToBase64(bytes).replace(/\+/g, '-').replace(/\//g, '_');
  }

  function normalizeCid(id) {
    return String(id || '').replace(/^<|>$/g, '').trim();
  }

  function inlineCidImages(html, attachments) {
    if (!html) return html;
    var cidMap = {};
    (attachments || []).forEach(function (att) {
      if (att.related && att.contentId && att.content) {
        var mime = att.mimeType || 'application/octet-stream';
        var dataUri = 'data:' + mime + ';base64,' + bytesToBase64(att.content);
        cidMap[normalizeCid(att.contentId)] = dataUri;
      }
    });
    return html.replace(/\bcid:([^"'\s)>]+)/gi, function (m, cid) {
      var id = normalizeCid(cid);
      return cidMap[id] || m;
    });
  }

  function attachmentSize(att) {
    if (!att.content) return 0;
    if (typeof att.content === 'string') return att.content.length;
    return att.content.byteLength || 0;
  }

  function formatSize(bytes) {
    if (!bytes) return '';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
  }

  // Strip characters that would corrupt the "CMD_SAVE|name|payload"
  // message or are unsafe as a filesystem name.
  function sanitizeFilename(name) {
    return String(name || '')
      .replace(/[|"\r\n\t]/g, '_')
      .replace(/[\\/]/g, '_')
      .replace(/^[. ]+/, '')
      .replace(/[. ]+$/, '')
      || 'attachment';
  }

  function headerRow(label, value) {
    if (!value) return '';
    return '<div class="row"><span class="label">' + escapeHtml(label) +
           '</span><span class="value">' + escapeHtml(value) + '</span></div>';
  }

  function headerBlock(email) {
    var rows = '';
    rows += headerRow('Subject', email.subject);
    rows += headerRow('From', addressText(email.from));
    if (email.to && email.to.length) rows += headerRow('To', email.to.map(addressText).join(', '));
    if (email.cc && email.cc.length) rows += headerRow('Cc', email.cc.map(addressText).join(', '));
    rows += headerRow('Date', formatDate(email.date));
    if (!rows) return '';
    return '<div class="header">' + rows + '</div>';
  }

  function setSaveStatus(status, message) {
    var el = document.getElementById('save-status');
    if (!el) return;
    var cls = status === 'ok' ? 'save-ok' : (status === 'error' ? 'save-error' : 'save-info');
    el.className = cls;
    el.textContent = message || '';
  }

  function saveAttachment(att) {
    if (!att || !att.content) return;
    var b64 = bytesToUrlSafeBase64(att.content);
    var name = sanitizeFilename(att.filename);
    var msg = 'CMD_SAVE|' + name + '|' + b64;
    try {
      if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(msg);
      } else {
        setSaveStatus('error', 'Cannot save: host bridge unavailable.');
      }
    } catch (e) {
      setSaveStatus('error', 'Cannot save from this view.');
    }
  }

  function attListItem(att, index) {
    var name = sanitizeFilename(att.filename) || '(unnamed)';
    var size = formatSize(attachmentSize(att));
    var meta = att.mimeType || '';
    if (size) meta = meta ? meta + ', ' + size : size;
    var metaHtml = meta ? ' <span class="meta">(' + escapeHtml(meta) + ')</span>' : '';
    return '<li><button type="button" class="save-link" data-index="' + index + '">' +
           escapeHtml(name) + '</button>' + metaHtml + '</li>';
  }

  function attachmentList(email) {
    var atts = (email.attachments || []).filter(function (a) { return !a.related; });
    if (!atts.length) return '';
    var items = atts.map(attListItem).join('');
    return '<div class="attachments"><h3>Attachments</h3><ul>' + items +
           '</ul><div class="save-status" id="save-status"></div></div>';
  }

  // Host -> loader-side result callback (invoked via ExecuteScript). The
  // `&&` guard makes this a no-op when the callback is absent, keeping
  // rollback backward-compatible.
  window.__emlSaveResult = function (status, message) {
    setSaveStatus(status, message);
  };

  // Body-only rebuild shared by renderEmail and the page-side encoding
  // executor (loader.html's window.__evEncodingApply): a re-decode must
  // swap only the body region, leaving the header block and attachment
  // list untouched. HTML bodies get their cid: images inlined; text
  // bodies are escaped into .text-body — mirroring renderEmail's selection.
  function applyBody(html, isHtml, attachments) {
    var bodyInner;
    if (html) {
      bodyInner = isHtml
        ? inlineCidImages(String(html), attachments)
        : '<div class="text-body">' + escapeHtml(html) + '</div>';
    } else {
      bodyInner = '';
    }
    var bodyEl = document.querySelector('#content .message > .body');
    if (bodyEl) {
      bodyEl.innerHTML = bodyInner;
      return;
    }
    document.getElementById('content').innerHTML =
      '<div class="message"><div class="body">' + bodyInner + '</div></div>';
  }

  function renderEmail(email) {
    var header = headerBlock(email);
    var atts = (email.attachments || []).filter(function (a) { return !a.related; });
    var attsHtml = attachmentList(email);
    document.getElementById('content').innerHTML =
      '<div class="message">' + header + '<div class="body"></div>' + attsHtml + '</div>';
    applyBody(email.html || email.text, !!email.html, email.attachments);
    wireAttachmentButtons(atts);
  }

  function wireAttachmentButtons(atts) {
    var saveEls = document.querySelectorAll('.save-link');
    for (var i = 0; i < saveEls.length; i++) {
      var idx = parseInt(saveEls[i].getAttribute('data-index'), 10);
      var att = atts[idx];
      if (!att) continue;
      saveEls[i].addEventListener('click', (function (a) {
        return function () { saveAttachment(a); };
      })(att));
    }
  }

  function renderRawText(text) {
    document.getElementById('content').innerHTML =
      '<div class="message"><div class="body"><pre>' + escapeHtml(text) + '</pre></div></div>';
  }

  window.emlRenderer = { renderEmail: renderEmail, renderRawText: renderRawText, applyBody: applyBody };
})();