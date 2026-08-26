/*
 * autodetect.js - provisional HTML charset auto-detection glue.
 *
 * Runs as a DocumentCreation script injected for HTML views (only HTML sets
 * window.__evRawFileBytesB64). It never mutates the live DOM; it reads the
 * pristine bytes already in the page, statistically detects an encoding, and
 * when the detector DISAGREES with what the engine chose (document.characterSet)
 * posts a single CMD_AUTO_ENCODING|<tag> JS->host message. The host then
 * performs the provisional host-side re-decode via ApplyAutoDetectedEncoding.
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
	if (!force && hasEncodingDeclaration(bytes)) return;

	// Lazily load the detector asset; wait for it before deciding.
	var s = document.createElement('script');
	s.src = ASSET_BASE + '/charset/jschardet.min.js';
	s.onload = function () {
		if (!window.jschardet) return;
		var res;
		try { res = window.jschardet.detect(bin); } catch (e) { return; }
		if (!res || !res.encoding) return;
		if (typeof res.confidence === 'number' && res.confidence < 0.90) return; // high-confidence gate
		var tag = normalizeName(res.encoding);
		if (!tag) return;
		// If the engine already chose it, nothing to do (zero flicker).
		var current = String(document.characterSet || '').toLowerCase();
		var currentTag = normalizeName(current) || (current.indexOf('utf') === 0 ? 'utf-8' : null);
		if (currentTag === tag) return;
		// Post one provisional correction request to the host.
		try {
			if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
				window.chrome.webview.postMessage('CMD_AUTO_ENCODING|' + tag);
			}
		} catch (e) {}
	};
	(document.head || document.documentElement).appendChild(s);
})();