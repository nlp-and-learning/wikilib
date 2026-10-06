# Template expansion support

`TemplateExpander` implements a defined subset of template preprocessing. It
returns `Result<std::string>` or `Result<void>`; check the result before using its
value. Parsing wikitext does not automatically expand templates: call `expand()`
before parsing, or call `expand_ast()` on an existing document.

## Templates and parameters

Definitions come from `TemplateProvider`; `MemoryTemplateProvider` is available
for local definitions. Names are trimmed and case-sensitive. An exact English
`Template:` prefix is removed before provider lookup. Localized prefixes,
redirect resolution, namespace aliases, and MediaWiki title normalization are
not implemented by the expander.

Supported syntax includes `{{Name|value|key=value}}`, nested invocations, dynamic
names, and `{{{parameter|default}}}` references. Pipes/equality signs inside
nested braces, wiki links, comments, and lowercase nowiki sections do not split
arguments. Defaults may contain templates or parameter references; an explicitly
empty parameter overrides its default. Repeated parameter keys use the last
value. Explicit numeric keys and unnamed positional arguments share the same
parameter map; only unnamed arguments advance the positional counter.

Argument names and values are evaluated before transclusion. Named and positional
values are trimmed by invocation parsing; this differs from MediaWiki's treatment
of positional whitespace. Undefined parameters without a default remain literal.
Malformed, unclosed brace sequences remain literal. Comments and lowercase
`<nowiki>` sections, including self-closing forms, are opaque and preserved.

Missing definitions return an error when `fail_on_missing` is enabled. Otherwise
`preserve_unknown` selects reconstruction of the complete invocation or an empty
string. Reconstruction preserves arguments but may normalize their surrounding
whitespace. Unsupported functions/magic words follow `preserve_unknown`; they
are not treated as missing template definitions.

## Parser functions

| Function | Implemented behavior |
| --- | --- |
| `#if` | Tests trimmed condition for nonempty text; expands only the selected branch. |
| `#ifeq` | Compares trimmed strings; expands only the selected branch. Numeric coercion is not implemented. |
| `#ifexpr` | Evaluates a numeric expression; zero selects the false branch, every other finite value the true branch. |
| `#switch` | String cases, fallthrough to a following `key=value`, and `#default`; only the selected result is expanded. |
| `#expr` | Numeric grammar described below. |
| `#ifexist` | Queries `provider->template_exists()`; this checks the provider's template collection, not an arbitrary wiki page database. |
| `#time` | Current UTC only; format codes `Y y m n d j H i s`, with other characters copied literally. Date/language arguments are rejected. |
| `#tag` | Tag name and content, without attributes. Additional arguments are rejected. |

The first argument after the colon is retained. Function names are
case-insensitive. Disabling `expand_parser_functions` leaves calls reconstructed
or drops them according to `preserve_unknown`.

`#invoke`, `#titleparts`, `#language`, and other functions are unimplemented and
preserved/dropped. `evaluate_lua` is reserved and does not enable a Lua runtime.

## Expressions

Expressions use finite `double` arithmetic and locale-independent ASCII numbers:
integers, decimals, and scientific literals such as `1e-3`. Supported operations,
from highest to lowest precedence, are:

1. Parentheses and unary `+`, `-`, `not`.
2. `^`.
3. `*`, `/`, `div`, `mod`.
4. Binary `+`, `-`.
5. `=`, `!=`, `<>`, `<`, `>`, `<=`, `>=`.
6. `and`.
7. `or`.

Equal-precedence operators associate left to right, including powers; e.g.
`2^3^2` evaluates to 64. This matches the documented association in the
[MediaWiki expression syntax reference](https://www.mediawiki.org/wiki/Manual:Expr_parser_function_syntax).
Unary signs bind before powers, so `-2^2` evaluates to 4.

`div` truncates the quotient toward zero; `mod` currently uses floating-point
remainder (`fmod`), unlike MediaWiki's integer conversion. Boolean/comparison
results are 0 or 1. Both operands of boolean operators are evaluated. Output uses
15 significant decimal digits and normalizes negative zero to `0`.

Invalid syntax, division by zero, non-finite values, unsupported identifiers,
missing operands, and trailing tokens produce errors rather than a zero result.
Input is limited to 65,536 bytes and recursive parentheses/unary nesting to 128
levels. General scientific-notation `e` operators, mathematical functions,
constants, `round`, arbitrary precision, and MediaWiki's detailed numeric error
formatting are not implemented.

## Page context, cache, and limits

Supported magic words are `PAGENAME`, `FULLPAGENAME`, `BASEPAGENAME`, `SUBPAGENAME`,
`ROOTPAGENAME`, `NAMESPACE`, `CURRENTYEAR`, `CURRENTMONTH`/`CURRENTMONTH2`,
`CURRENTDAY`/`CURRENTDAY2`, `CURRENTTIME`, and `CURRENTTIMESTAMP`. Month/day values
are zero-padded. `PageInfo::title` is the full dump title; for nonzero namespace
IDs, the prefix before its first colon supplies `NAMESPACE` and is removed for
`PAGENAME` and its base/sub/root forms. Provide a full title including the
localized namespace prefix. Site statistics, server/site identity, language,
URL-encoded variants, and talk/subject namespace mappings are unsupported.

Each public expansion operation gets a fresh budget and a cache of raw template
definitions. Expanded results are never cached: the current page, parameters,
and recursion/count limits are applied every time. Changes to the provider are
visible on the next operation. `stats().cache_hits` counts definition reuse,
not skipped expansion work. Statistics accumulate until `reset_stats()`.

Defaults are 40 recursive expansion levels, 10,000 expansion constructs, and
16 MiB per expanded string (including intermediate arguments). Constructs include
templates, parameters, parser functions, and magic words; cached and preserved
calls still consume the budget. Recursive body/argument/default evaluation counts
toward depth. Caller context depth limits and configured limits both apply.
Limit violations always return errors. Instances require external synchronization
for concurrent use; provider callbacks must not reenter the same expander.

## AST expansion

`expand_ast()` serializes the existing document, expands it, and reparses the
result with the markup parser. Generated headings, lists, links, and tables can
therefore become real AST nodes. This is a document replacement, not an in-place
edit of selected nodes: serialization can normalize whitespace, source locations
refer to regenerated wikitext, and existing child pointers become invalid on
success. Parent pointers, category pointers, and redirect pointers are rebuilt,
including preserved template argument children.

Expansion or reported parse errors leave the original document unchanged.
Statistics reflect attempted expansion even when AST reparsing fails. Reparsing
uses the existing lenient markup parser, which can normalize incomplete markup;
it is not a full MediaWiki grammar validator. Comments already discarded by the
original parser cannot be restored; use `TokenizerConfig::preserve_comments`
when the original AST must retain them.

## Follow-up scope

Lua execution and `#invoke` require a separate runtime integration stage. Full
MediaWiki preprocessing is also outside this subset: ambiguous runs of four or
more braces, `subst`/`safesubst`, include/noinclude/onlyinclude processing, extension
tag semantics, and delimiter creation across expansion boundaries are unsupported.
The expander does not resolve definitions from a wiki automatically; that is the
provider's responsibility.
