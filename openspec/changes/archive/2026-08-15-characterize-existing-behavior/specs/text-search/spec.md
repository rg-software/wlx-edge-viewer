## Purpose

Characterizes the existing in-document text search capability of the Total Commander WLX Lister plugin: how a search request from Total Commander is dispatched into the rendered WebView2 document, how the TC search-parameter flags map to the underlying `window.find()` arguments, how the "find first" flag is given special handling, and how the legacy ANSI entry point wraps the Unicode entry point. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin.

## ADDED Requirements

### Requirement: Search dispatch chain

When Total Commander asks the lister to search for text, the plugin SHALL relay the request to its lister window and that window SHALL drive the search inside the rendered document. The Unicode search entry point SHALL format the search-parameter flags and the search string into a single message (the parameter value first, followed by a single space, followed by the search text) and SHALL send that message to the lister window. The lister window SHALL extract the leading parameter token and the remaining text, then SHALL ask the navigator to find the remaining text using the parsed parameters. The navigator SHALL run a script that calls the browser's `window.find()` API with arguments derived from the parameters. The search entry point SHALL return a success indicator to Total Commander regardless of whether a match was found.

#### Scenario: TC triggers a search

- **WHEN** Total Commander invokes the Unicode search entry point with a search string and a set of parameter flags
- **THEN** the plugin formats the parameters and the search string into one message, sends it to the lister window, which then runs `window.find()` against the rendered document with arguments derived from the parameters

#### Scenario: search returns success to TC

- **WHEN** a search request has been dispatched and the document search script has been invoked
- **THEN** the Unicode search entry point returns a success indicator to Total Commander, independent of whether `window.find()` reported a match

### Requirement: Search parameter flags

The plugin SHALL map Total Commander's search parameter flags to the corresponding parameters of the browser's `window.find()` API. The case-sensitive flag (`lcs_matchcase`, value 2) SHALL set the `caseSensitive` argument; the backwards flag (`lcs_backwards`, value 8) SHALL set the `backwards` argument so the search proceeds from the current position toward the beginning of the document; the whole-words flag (`lcs_wholewords`, value 4) SHALL set the `wholeWord` argument so only whole-word matches are reported. Unspecified flags SHALL fall back to the browser's defaults (case-insensitive, forward, partial-word).

#### Scenario: case-sensitive search

- **WHEN** Total Commander requests a search with `lcs_matchcase` (2) set in the parameters
- **THEN** the plugin runs `window.find()` with the case-sensitive argument enabled, so only matches with the same letter casing as the search string are reported

#### Scenario: backwards search

- **WHEN** Total Commander requests a search with `lcs_backwards` (8) set in the parameters
- **THEN** the plugin runs `window.find()` with the backwards argument enabled, so the search proceeds from the current position toward the beginning of the document

#### Scenario: whole-word search

- **WHEN** Total Commander requests a search with `lcs_wholewords` (4) set in the parameters
- **THEN** the plugin runs `window.find()` with the whole-word argument enabled, so only matches that are not part of a larger word are reported

#### Scenario: default search

- **WHEN** Total Commander requests a search with none of the case-sensitive, backwards or whole-word flags set
- **THEN** the plugin runs `window.find()` with the browser's defaults (case-insensitive, forward, partial-word)

### Requirement: Find-first behavior

The find-first flag (`lcs_findfirst`, value 1) SHALL be given special handling. When set, the plugin SHALL repeatedly invoke `window.find()` in the backwards direction until the browser reports that no more matches exist. This loop SHALL have the effect of moving the search cursor backwards through every match until it wraps past the beginning, after which the next forward search starts from the start of the document. The flag SHALL be treated as a reset-to-beginning operation rather than a one-shot find.

#### Scenario: find-first resets to beginning

- **WHEN** Total Commander requests a search with `lcs_findfirst` (1) set
- **THEN** the plugin repeatedly calls `window.find()` backwards until no further match is reported, so the document's search cursor is positioned at the beginning for a subsequent search

#### Scenario: find-first combined with case sensitivity

- **WHEN** Total Commander requests a search with both `lcs_findfirst` (1) and `lcs_matchcase` (2) set
- **THEN** the plugin performs the repeated backwards search with the case-sensitive argument enabled, so the reset-to-beginning cursor skips past case-mismatched text

### Requirement: ANSI wrapper parity

The plugin SHALL export an ANSI search entry point in addition to the Unicode search entry point. The ANSI entry point SHALL convert the ANSI search string to UTF-16 and SHALL delegate to the Unicode entry point so that both entry points exercise the same dispatch chain and the same `window.find()` invocation. No separate ANSI search path SHALL exist.

#### Scenario: ANSI search string

- **WHEN** Total Commander invokes the ANSI search entry point with an ANSI search string and a set of parameter flags
- **THEN** the plugin converts the search string to UTF-16 and runs the same dispatch chain as the Unicode entry point, so the document search sees the same pattern and the same parameters

#### Scenario: Unicode search string

- **WHEN** Total Commander invokes the Unicode search entry point directly
- **THEN** the plugin uses the UTF-16 search string as-is and runs the same dispatch chain that the ANSI entry point delegates to

### Requirement: 32-bit and 64-bit search parity

The search dispatch chain, the parameter-to-`window.find()` argument mapping, the find-first reset loop and the ANSI-to-Unicode delegation SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL export both the ANSI and the Unicode search entry points, and both builds SHALL return the same success indicator to Total Commander for the same inputs.

#### Scenario: Win32 build text search

- **WHEN** the 32-bit plugin is loaded and Total Commander requests a case-sensitive, whole-word search
- **THEN** the plugin runs `window.find()` with the case-sensitive and whole-word arguments enabled, matching the behavior of the 64-bit build for the same inputs

#### Scenario: x64 build text search

- **WHEN** the 64-bit plugin is loaded and Total Commander requests a find-first search
- **THEN** the plugin performs the repeated backwards search reset loop, matching the behavior of the 32-bit build for the same inputs