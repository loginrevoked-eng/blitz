"""A small, dependency-free JavaScript network-call analyzer."""

from .analyzer import analyze_url
from .fetcher import download_mentions, download_text
from .html_extract import extract_js_mentions
from .js_calls import extract_xhr_fetch_calls
from .js_imports import extract_module_urls
from .models import AnalysisResult, JSMention, XHRFetch
from .report import pretty_print

__all__ = [
    "AnalysisResult",
    "JSMention",
    "XHRFetch",
    "analyze_url",
    "download_mentions",
    "download_text",
    "extract_js_mentions",
    "extract_xhr_fetch_calls",
    "extract_module_urls",
    "pretty_print",
]
