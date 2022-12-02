/*
 * detect-charset
 * https://github.com/treyhunner/detect-charset
 *
 * Copyright (c) 2015 Trey Hunner
 * Licensed under the MIT license.
 */

/* slightly modifed for the browser */

var bom = {
  'utf-8': '\xEF\xBB\xBF',
  'utf-16be': '\xFE\xFF',
  'utf-16le': '\xFF\xFE',
  'utf-32be': '\x00\x00\xFE\xFF',
  'utf-32le': '\xFF\xFE\x00\x00',
};

function hasBOM(buffer, bom) {
	if (buffer.length < bom.length)
		return false;
		
	var bufferBeginning = buffer.slice(0, bom.length);

	for (let i = 0; i < bufferBeginning.length; i++)
		if (bufferBeginning[i] != bom.charCodeAt(i))
			return false;

	return true;
}

function detect_charset(buffer) {
	var hasUnicode = Array.prototype.some.call(buffer, function(character) {
		return (character > 127);
	});

	if (hasBOM(buffer, bom['utf-8'])) {
		return 'utf-8-bom';
	} else if (hasBOM(buffer, bom['utf-32be'])) {
		return 'utf-32be';
	} else if (hasBOM(buffer, bom['utf-32le'])) {
		return 'utf-32le';
	} else if (hasBOM(buffer, bom['utf-16be'])) {
		return 'utf-16be';
	} else if (hasBOM(buffer, bom['utf-16le'])) {
		return 'utf-16le';
	} else if (hasUnicode) {
		return 'utf-8';
	} else {
		return 'latin1';
	}
};
