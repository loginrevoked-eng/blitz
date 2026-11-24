"""High-level page analysis orchestration."""

from dataclasses import replace

from .fetcher import download_text
from .html_extract import extract_js_mentions
from .js_calls import extract_xhr_fetch_calls
from .js_imports import extract_module_urls
from .models import AnalysisResult, JSMention


def analyze_url(
    url: str, *, timeout: float = 15.0, depth: int = 2
) -> AnalysisResult:
    """Analyze a page and recursively follow URL-like JavaScript imports.

    Depth 1 downloads scripts referenced by the HTML. Each additional level
    follows imports discovered in the preceding level of JavaScript.
    """
    if depth < 0:
        raise ValueError("depth must be zero or greater")

    html = download_text(url, timeout=timeout)
    root_mentions = extract_js_mentions(html, url)
    mentions = list(root_mentions)
    calls = []
    queue: list[tuple[JSMention, int]] = []

    for mention in root_mentions:
        if mention.type == "inline" and mention.code:
            calls.extend(extract_xhr_fetch_calls(mention.code, mention.source_url))
            if depth >= 1:
                for child_url in extract_module_urls(mention.code, mention.source_url):
                    child = JSMention(
                        type="url",
                        source_url=mention.source_url,
                        script_index=len(mentions) + 1,
                        url=child_url,
                        parent=mention,
                    )
                    mentions.append(child)
                    queue.append((child, 1))
        elif mention.type == "url" and depth >= 1:
            queue.append((mention, 1))

    downloaded: dict[str, str] = {}
    expanded: set[str] = set()
    while queue:
        mention, current_depth = queue.pop(0)
        script_url = mention.url
        if not script_url:
            continue

        code = downloaded.get(script_url)
        if code is None:
            code = download_text(script_url, timeout=timeout)
            downloaded[script_url] = code

        loaded_mention = replace(mention, code=code)
        for index, existing in enumerate(mentions):
            if existing is mention:
                mentions[index] = loaded_mention
                break

        if script_url in expanded:
            continue
        expanded.add(script_url)
        calls.extend(extract_xhr_fetch_calls(code, script_url))

        if current_depth >= depth:
            continue
        for child_url in extract_module_urls(code, script_url):
            child = JSMention(
                type="url",
                source_url=script_url,
                script_index=len(mentions) + 1,
                url=child_url,
                parent=loaded_mention,
            )
            mentions.append(child)
            queue.append((child, current_depth + 1))

    return AnalysisResult(page_url=url, mentions=tuple(mentions), calls=tuple(calls))
