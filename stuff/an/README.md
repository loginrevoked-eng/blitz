# JavaScript URL analyzer

Small, dependency-free Python package that extracts script tags from a page and
finds `fetch(...)` calls plus `XMLHttpRequest.open(...)` calls on variables
constructed with `new XMLHttpRequest()`.

```bash
python main.py --url https://example.com --depth 2
```

The URL can also be passed positionally: `python main.py https://example.com`.
Depth 1 downloads scripts referenced by the HTML. Depth 2 (the default) also
follows URL-like `import` / `export ... from` references in those scripts.
Each discovered script keeps a `parent` JSMention link, and the report shows
the import chain. `js_syntax_highlight_cli.highlight_js()` provides the small
ANSI highlighter used for JavaScript call contexts.

Highlight a file directly with `python -m js_syntax_highlight_cli app.js`.

Render a boxed snippet from Python:

```python
from js_syntax_highlight_cli import JSSyntaxLayout, JSSyntaxWindow

wind = JSSyntaxWindow(
    "fetch('/api/items', { method: 'GET' });",
    layout=JSSyntaxLayout(width=72, color=True, line_numbers=True),
)
print(wind.render())
```

```python
from js_analyzer import analyze_url, pretty_print

result = analyze_url("https://example.com")
print(pretty_print(result))
```

The extraction stages can also be used independently:

```python
from js_analyzer import (
    download_mentions,
    download_text,
    extract_js_mentions,
    extract_xhr_fetch_calls,
)

html = download_text("https://example.com")
mentions = extract_js_mentions(html, "https://example.com")
scripts = download_mentions(mentions)  # URL mentions only; code is populated
calls = [
    call
    for script in scripts
    for call in extract_xhr_fetch_calls(script.code or "", script.url or "")
]
```

`url_expression` is the original JavaScript expression, so dynamic values are
preserved rather than evaluated. The scanner is intentionally lightweight and
does not execute JavaScript or implement the full language grammar.
