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

  function attachmentList(email) {
    var atts = (email.attachments || []).filter(function (a) { return !a.related; });
    if (!atts.length) return '';
    var items = atts.map(function (att) {
      var name = att.filename || '(unnamed)';
      var size = formatSize(attachmentSize(att));
      var meta = att.mimeType || '';
      if (size) meta = meta ? meta + ', ' + size : size;
      return '<li>' + escapeHtml(name) + (meta ? ' <span class="meta">(' + escapeHtml(meta) + ')</span>' : '') + '</li>';
    }).join('');
    return '<div class="attachments"><h3>Attachments</h3><ul>' + items + '</ul></div>';
  }

  function renderEmail(email) {
    var header = headerBlock(email);
    var bodyHtml;
    if (email.html) {
      bodyHtml = inlineCidImages(email.html, email.attachments);
    } else if (email.text) {
      bodyHtml = '<div class="text-body">' + escapeHtml(email.text) + '</div>';
    } else {
      bodyHtml = '';
    }
    var atts = attachmentList(email);
    document.getElementById('content').innerHTML =
      '<div class="message">' + header + '<div class="body">' + bodyHtml + '</div>' + atts + '</div>';
  }

  function renderRawText(text) {
    document.getElementById('content').innerHTML =
      '<div class="message"><div class="body"><pre>' + escapeHtml(text) + '</pre></div></div>';
  }

  window.emlRenderer = { renderEmail: renderEmail, renderRawText: renderRawText };
})();