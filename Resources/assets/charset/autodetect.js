/*
 * autodetect.js - provisional HTML charset auto-detection glue.
 *
 * Runs as a DocumentCreation script injected for HTML views (only HTML sets
 * window.__evRawFileBytesB64). It never mutates the live DOM; it reads the
 * pristine bytes already in the page and:
 *   - DECLARED charset (BOM / <meta charset> / http-equiv, ForceDetect off):
 *     detection is skipped (design D5), but the charset the engine actually
 *     decoded with (document.characterSet) is REPORTED so the Encoding submenu
 *     can show "Auto: <codepage>".
 *   - Otherwise it statistically detects an encoding (jschardet) and:
 *     - DISAGREES with what the engine chose (document.characterSet): posts a
 *       single CMD_AUTO_ENCODING|<tag> JS->host message, which the host turns
 *       into a provisional host-side re-decode (ApplyAutoDetectedEncoding).
 *     - AGREES with the engine, or yields no recognizable high-confidence code
 *       page (e.g. pure ASCII): posts a single CMD_AUTO_ENCODING_REPORT|<tag> so
 *       the host can surface "Auto: <tag>" in the Encoding submenu without
 *       re-decoding anything (zero flicker).
 *   Both report paths feed the same display-only host handler; only the
 *   disagreement path ever re-renders.
 *
 * __EV_ASSET_BASE__ is replaced by the backend with the asset host prefix
 * (http://assets.example on Windows, ev://assets.example on Linux).
 */
(function () {
	'use strict';

	// The backend injects a tiny bootstrap that sets window.__evAssetBase to
	// the asset host prefix (http://assets.example on Windows, ev://assets.example
	// on Linux) and then loads this file via a <script> tag.
	var ASSET_BASE = window.__evAssetBase || '';

	// jschardet's returned names -> EncodingList tag (lowercase). Anything not
	// offered by the plugin's Encoding menu is ignored so we never request a
	// code page the host cannot apply.
	var TAG_NORMALIZE = {
		'utf-8': 'utf-8',
		'utf8': 'utf-8',
		'windows-1252': 'windows-1252',
		'iso-8859-1': 'windows-1252',
		'iso-8859-15': 'iso-8859-15',
		'windows-1250': 'windows-1250',
		'iso-8859-2': 'iso-8859-2',
		'windows-1251': 'windows-1251',
		'koi8-r': 'koi8-r',
		'x-mac-cyrillic': 'windows-1251',
		'iso-8859-5': 'iso-8859-5',
		'windows-1253': 'windows-1253',
		'windows-1254': 'windows-1254',
		'windows-1255': 'windows-1255',
		'windows-1256': 'windows-1256',
		'windows-1257': 'windows-1257',
		'windows-1258': 'windows-1258',
		'windows-874': 'windows-874',
		'shift_jis': 'shift_jis',
		'shift-jis': 'shift_jis',
		'euc-jp': 'euc-jp',
		'gbk': 'gbk',
		'gb2312': 'gb2312',
		'big5': 'big5',
		'euc-kr': 'euc-kr'
	};

	function normalizeName(name) {
		var n = String(name).toLowerCase();
		return TAG_NORMALIZE[n] || null;
	}

	// The charset the engine actually decoded the page with (document.characterSet
	// contains the name it used). Null if it is not one we can represent in the
	// Encoding submenu. Any 'utf'-prefixed value maps to utf-8.
	function engineTag() {
		var cs = String(document.characterSet || '').toLowerCase();
		return normalizeName(cs) || (cs.indexOf('utf') === 0 ? 'utf-8' : null);
	}

	// True if the first bytes declare an encoding the engine is authoritative
	// about (BOM or an explicit <meta charset>/http-equiv). We then never
	// second-guess them (design D5).
	function hasEncodingDeclaration(bytes) {
		var i, n = bytes.length, head = [];
		for (i = 0; i < n && i < 1024; i++) head.push(String.fromCharCode(bytes[i]));
		var s = head.join('');
		// UTF-8 / UTF-16 / UTF-32 BOM
		if (n >= 3 && bytes[0] === 0xEF && bytes[1] === 0xBB && bytes[2] === 0xBF) return true;
		if (n >= 2 && ((bytes[0] === 0xFF && bytes[1] === 0xFE) ||
		               (bytes[0] === 0xFE && bytes[1] === 0xFF))) return true;
		// <meta charset=...> or <meta http-equiv="Content-Type" content="...charset=...">
		var re = /<meta[^>]+charset\s*=\s*['"]?[a-z0-9-]+/i;
		if (re.test(s)) return true;
		var re2 = /<meta[^>]+http-equiv\s*=\s*['"]?content-type/i;
		return re2.test(s);
	}

	// Run immediately (not on DOMContentLoaded): this file is loaded
	// asynchronously from inside a DOMContentLoaded handler in the bootstrap,
	// so that event has already fired by the time this executes. Reading the
	// pristine bytes and document.characterSet, and appending a <script>, are
	// all safe at any point; we never touch rendered content.
	// Only HTML views set the pristine bytes; MHT/loaders do not.
	var b64 = window.__evRawFileBytesB64;
	if (!b64 || window.__evAutoDetectDone) return;
	window.__evAutoDetectDone = true;

	// Decode to a Latin-1 binary string. jschardet.detect expects a string
	// (it slices/.split()/charCodeAt over it), NOT a Uint8Array.
	var bin, bytes;
	try {
		bin = atob(b64);
		bytes = new Uint8Array(bin.length);
		for (var i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
	} catch (e) { return; }
	if (!bytes.length) return;
	// With ForceDetectEncoding=1 (window.__evForceDetect), skip the "declared
	// encoding is authoritative" gate so a WRONG declared charset is corrected;
	// a genuine declared file is still untouched because the engine agrees below.
	var force = !!window.__evForceDetect;
	if (!force && hasEncodingDeclaration(bytes)) {
		// Declared encoding is authoritative (design D5): no statistical
		// detection. Still surface the charset the engine actually decoded
		// with (document.characterSet) so the Encoding submenu shows
		// "Auto: <codepage>" instead of a bare "Auto-detect".
		var declaredTag = engineTag();
		if (declaredTag) post('CMD_AUTO_ENCODING_REPORT|' + declaredTag);
		return;
	}

	// Lazily load the detector asset; wait for it before deciding.
	var s = document.createElement('script');
	s.src = ASSET_BASE + '/charset/jschardet.min.js';
	s.onload = function () {
		if (!window.jschardet) return;
		var currentTag = engineTag();
		var res;
		try { res = window.jschardet.detect(bin); } catch (e) { return; }
		if (!res || !res.encoding) return;
		if (typeof res.confidence === 'number' && res.confidence < 0.90) return; // high-confidence gate
		var tag = normalizeName(res.encoding);
		// No recognized high-confidence code page (e.g. pure ASCII reports
		// "ascii", which the menu cannot represent) or it matches what the
		// engine already used: nothing to re-decode. Surface the charset the
		// engine actually used so the menu shows "Auto: <codepage>" (zero
		// flicker, no re-render).
		if (!tag || tag === currentTag) {
			if (currentTag) post('CMD_AUTO_ENCODING_REPORT|' + currentTag);
			return;
		}
		// Disagreement: post one provisional correction request to the host.
		post('CMD_AUTO_ENCODING|' + tag);
	};
	// Route a message to whichever JS<->host bridge this platform exposes.
	function post(msg) {
		try {
			if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage)
				window.chrome.webview.postMessage(msg);
		} catch (e) {}
	}
	(document.head || document.documentElement).appendChild(s);
})();