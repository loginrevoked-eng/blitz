#!/usr/bin/env python3
"""Python implementation of the core local SearchSploit workflow."""

import argparse
import ast
import calendar
import csv
import json
import re
import shutil
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from datetime import date, timedelta
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_DATABASE = SCRIPT_DIR / "expdb" / "files_exploits.csv"
CVE_PATTERN = re.compile(r"\bCVE-(\d{4})-(\d+)\b", re.IGNORECASE)
VERSION_TOKEN = re.compile(r"^\d+(?:\.\d+)*(?:[-.]?[a-z][\w.-]*)?$", re.IGNORECASE)
VERSION_PART = re.compile(r"\d+|[a-z]+", re.IGNORECASE)
VERSION_RANGE = re.compile(
    r"(?:(?P<left>\d+(?:\.\d+)*(?:[-.]?[a-z][\w.-]*)?)\s*)?"
    r"(?P<operator><=|<|>=|>)\s*"
    r"(?P<right>\d+(?:\.\d+)*(?:[-.]?[a-z][\w.-]*)?)",
    re.IGNORECASE,
)


@dataclass(frozen=True, order=True)
class CVE:
    year: int
    id: int

    def __str__(self):
        return f"CVE-{self.year}-{self.id:04d}"


@dataclass(frozen=True)
class AgeLimit:
    value: int
    unit: str


@dataclass(frozen=True)
class ColumnInclude:
    column_name: str
    candidates: tuple[str, ...]


@dataclass(frozen=True)
class Entry:
    row: dict
    kind: str
    database: Path

    @property
    def id(self):
        return str(self.row.get("id", ""))

    @property
    def title(self):
        return str(self.row.get("description", "") or "")

    @property
    def relative_path(self):
        return str(self.row.get("file", "") or "")

    @property
    def repo_dir(self):
        return self.database.parent

    @property
    def full_path(self):
        return self.repo_dir / Path(self.relative_path) if self.relative_path else None


def parse_cves(value):
    """Parse CVE references into structured year/id values."""
    return [CVE(year=int(year), id=int(identifier)) for year, identifier in CVE_PATTERN.findall(value or "")]


def parse_age_limit(value):
    match = re.fullmatch(r"(\d+)([ymd]?)", value.strip(), re.IGNORECASE)
    if not match:
        raise argparse.ArgumentTypeError(
            "age must be a non-negative number followed by y, m, or d (for example 5y, 5m, 5d)"
        )
    amount, unit = match.groups()
    return AgeLimit(value=int(amount), unit=(unit or "y").lower())


def parse_column_include(value):
    match = re.fullmatch(
        r"\[\s*coll?umn_name\s*=\s*(['\"])(.*?)\1\s*,\s*"
        r"candidates\s*=\s*(\[.*\])\s*\]",
        value.strip(),
        re.IGNORECASE | re.DOTALL,
    )
    if not match:
        raise argparse.ArgumentTypeError(
            "expected [column_name='name', candidates=['value1', 'value2']]"
        )
    column_name = match.group(2).strip()
    try:
        candidates = ast.literal_eval(match.group(3))
    except (SyntaxError, ValueError) as error:
        raise argparse.ArgumentTypeError(f"invalid candidates list: {error}") from error
    if not column_name:
        raise argparse.ArgumentTypeError("column_name cannot be empty")
    if not isinstance(candidates, list) or not candidates or not all(
        isinstance(candidate, str) for candidate in candidates
    ):
        raise argparse.ArgumentTypeError("candidates must be a non-empty list of strings")
    return ColumnInclude(column_name, tuple(candidates))


def load_entries(exploit_database):
    sources = (("Exploit", exploit_database), ("Shellcode", exploit_database.parent / "files_shellcodes.csv"))
    entries = []
    columns = {}
    for kind, database in sources:
        if not database.is_file():
            if kind == "Exploit":
                raise FileNotFoundError(database)
            continue
        with database.open("r", encoding="utf-8-sig", newline="") as csv_file:
            reader = csv.DictReader(csv_file)
            columns[kind] = reader.fieldnames or []
            entries.extend(Entry(row, kind, database) for row in reader)
    return entries, columns


def tokenize_terms(values):
    return re.findall(r"\S+", " ".join(values))


def nmap_queries(path):
    """Build SearchSploit-style name/product/version queries from Nmap XML."""
    root = ET.parse(path).getroot()
    queries = []
    for service in root.iter():
        if service.tag.rsplit("}", 1)[-1] != "service":
            continue
        name = (service.get("name") or "").strip()
        product = (service.get("product") or "").strip()
        version = (service.get("version") or "").strip()
        candidates = (name, product, f"{product} {version}" if product and version else "")
        for candidate in candidates:
            terms = tokenize_terms([candidate]) if candidate else []
            if terms and terms not in queries:
                queries.append(terms)
    return queries


def version_parts(value):
    parts = VERSION_PART.findall(value.casefold())
    return [(1, int(part)) if part.isdigit() else (0, part) for part in parts]


def compare_versions(left, right):
    left_parts = version_parts(left)
    right_parts = version_parts(right)
    size = max(len(left_parts), len(right_parts))
    left_parts.extend([(1, 0)] * (size - len(left_parts)))
    right_parts.extend([(1, 0)] * (size - len(right_parts)))
    return (left_parts > right_parts) - (left_parts < right_parts)


def version_matches_range(title, requested_version):
    for match in VERSION_RANGE.finditer(title):
        left = match.group("left")
        right = match.group("right")
        operator = match.group("operator")
        if operator.startswith("<"):
            minimum, maximum = left or "0", right
        else:
            minimum, maximum = right, left
        if compare_versions(requested_version, minimum) < 0:
            continue
        if maximum is not None and compare_versions(requested_version, maximum) > 0:
            continue
        return True
    return False


def search_text(entry, title_only=False):
    if title_only:
        return entry.title
    # SearchSploit's default grep input includes the EDB-ID, path, and title.
    return f"{entry.id} {entry.relative_path} {entry.title}"


def terms_match(entry, terms, exact=False, title_only=False, case_sensitive=False, strict=False):
    title = entry.title
    haystack = search_text(entry, title_only=title_only or exact)
    if not case_sensitive:
        title, haystack = title.casefold(), haystack.casefold()

    if exact:
        phrase = " ".join(terms)
        if not case_sensitive:
            phrase = phrase.casefold()
        return phrase in title

    version_terms = [term for term in terms if VERSION_TOKEN.fullmatch(term)]
    fuzzy_version = version_terms[-1] if len(terms) > 1 and version_terms and not strict else None
    compare_haystack = haystack if case_sensitive else haystack.casefold()
    for term in terms:
        needle = term if case_sensitive else term.casefold()
        if needle in compare_haystack:
            continue
        if term == fuzzy_version and version_matches_range(title, term):
            continue
        return False
    return True


def category_matches(entry, pattern):
    row = entry.row
    text = " ".join(
        str(row.get(field, "") or "")
        for field in ("description", "file", "type", "platform", "tags")
    )
    return pattern.search(text) is not None


def column_includes_match(entry, filters):
    for column_filter in filters:
        value = str(entry.row.get(column_filter.column_name, "") or "").casefold()
        candidates = {candidate.casefold() for candidate in column_filter.candidates}
        if value not in candidates:
            return False
    return True


def published_date(entry):
    value = (entry.row.get("date_published") or "").strip()
    if not value:
        return None
    try:
        return date.fromisoformat(value[:10])
    except ValueError:
        return None


def age_cutoff(limit, today):
    if limit is None:
        return None
    if limit.unit == "d":
        return today - timedelta(days=limit.value)
    months = limit.value * (12 if limit.unit == "y" else 1)
    month_index = today.year * 12 + today.month - 1 - months
    year, month_index = divmod(month_index, 12)
    month = month_index + 1
    day = min(today.day, calendar.monthrange(year, month)[1])
    return date(year, month, day)


def details(entry, show_source=True):
    row = entry.row
    print(f"EDB-{entry.id} | {entry.kind.lower()} | {row.get('platform', '')}")
    print(f"Title: {entry.title}")
    for label, field in (
        ("Published", "date_published"),
        ("Author", "author"),
        ("Type", "type"),
        ("Platform", "platform"),
        ("Port", "port"),
        ("Verified", "verified"),
        ("Codes", "codes"),
        ("Tags", "tags"),
        ("Aliases", "aliases"),
        ("Screenshot", "screenshot_url"),
        ("Application", "application_url"),
        ("Source", "source_url"),
    ):
        value = (row.get(field) or "").strip()
        if value:
            print(f"{label}: {value}")
    if not entry.full_path:
        return 0
    print(f"Path: {entry.full_path}")
    if not show_source:
        return 0
    if not entry.full_path.is_file():
        print("Exploit file is missing from the local checkout.", file=sys.stderr)
        return 2
    try:
        contents = entry.full_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        print(f"Could not read exploit file: {error}", file=sys.stderr)
        return 2
    print("\n" + "-" * 72 + "\n")
    print(contents, end="" if contents.endswith("\n") else "\n")
    return 0


def result_url(entry):
    section = "shellcodes" if entry.kind == "Shellcode" else "exploits"
    return f"https://www.exploit-db.com/{section}/{entry.id}"


def visible_path(entry):
    prefix = "exploits/" if entry.kind == "Exploit" else "shellcodes/"
    path = entry.relative_path
    if path.startswith(prefix):
        path = path[len(prefix):]
    return path.replace("/", "\\")


def highlight(text, terms, enabled, case_sensitive=False):
    if not enabled or not terms:
        return text
    flags = 0 if case_sensitive else re.IGNORECASE
    for term in sorted(set(terms), key=len, reverse=True):
        if term:
            text = re.sub(
                re.escape(term),
                lambda match: f"\033[31m{match.group(0)}\033[0m",
                text,
                flags=flags,
            )
    return text


def render_json(entries, search_label, args):
    output = {"SEARCH": search_label}
    for kind, key in (("Exploit", "EXPLOIT"), ("Shellcode", "SHELLCODE")):
        subset = [entry for entry in entries if entry.kind == kind]
        root = str(subset[0].repo_dir) if subset else ""
        output[f"DB_PATH_{key}"] = root
        if args.www:
            output[f"RESULTS_{key}"] = [
                {"Title": entry.title, "URL": result_url(entry)} for entry in subset
            ]
        elif args.id_only:
            output[f"RESULTS_{key}"] = [
                {
                    "Title": entry.title,
                    "EDB-ID": entry.id,
                    "Path": str(entry.full_path) if entry.full_path else "",
                }
                for entry in subset
            ]
        else:
            output[f"RESULTS_{key}"] = [
                {
                    "Title": entry.title,
                    "EDB-ID": entry.id,
                    "Date_Published": entry.row.get("date_published", ""),
                    "Date_Added": entry.row.get("date_added", ""),
                    "Date_Updated": entry.row.get("date_updated", ""),
                    "Author": entry.row.get("author", ""),
                    "Type": entry.row.get("type", ""),
                    "Platform": entry.row.get("platform", ""),
                    "Port": entry.row.get("port", ""),
                    "Verified": entry.row.get("verified", ""),
                    "Codes": entry.row.get("codes", ""),
                    "Tags": entry.row.get("tags", ""),
                    "Aliases": entry.row.get("aliases", ""),
                    "Screenshot": entry.row.get("screenshot_url", ""),
                    "Application": entry.row.get("application_url", ""),
                    "Source": entry.row.get("source_url", ""),
                    "Path": str(entry.full_path) if entry.full_path else "",
                }
                for entry in subset
            ]
    print(json.dumps(output, indent=2))


def print_results(entries, search_label, args):
    if args.json:
        render_json(entries, search_label, args)
        return

    search_terms = tokenize_terms(args.search_terms if args.search_terms is not None else args.terms)
    highlight_terms = search_terms + ([args.cve] if args.cve else [])
    use_colour = sys.stdout.isatty() and not args.disable_colour

    for kind in ("Exploit", "Shellcode"):
        subset = sorted(
            (entry for entry in entries if entry.kind == kind),
            key=lambda entry: entry.title.casefold(),
        )
        if not subset:
            print(f"{kind}s: No Results")
            continue

        left_label = f"{kind} Title"
        right_label = "URL" if args.www else "EDB-ID" if args.id_only else "Path"
        width = max(50, shutil.get_terminal_size((100, 24)).columns - 36)
        right_width = 45 if args.www else 34
        left_width = max(20, width - right_width - 3)
        print(f"{left_label:<{left_width}} | {right_label}")
        print("-" * left_width + "-+-" + "-" * right_width)
        for entry in subset:
            if args.www:
                right = result_url(entry)
            elif args.id_only:
                right = entry.id
            else:
                right = visible_path(entry)
            title = entry.title
            if not args.overflow and len(title) > left_width:
                title = title[:max(1, left_width - 1)] + "…"
            title = highlight(title, highlight_terms, use_colour, args.case)
            right = highlight(right, highlight_terms, use_colour, args.case)
            if args.verbose:
                print(f"{title:<{left_width}} | {right}")
                print(f"  EDB-ID: {entry.id}")
                for label, field in (
                    ("Published", "date_published"),
                    ("Author", "author"),
                    ("Type", "type"),
                    ("Platform", "platform"),
                    ("Port", "port"),
                    ("Verified", "verified"),
                    ("Tags", "tags"),
                    ("Aliases", "aliases"),
                    ("Application", "application_url"),
                    ("Source", "source_url"),
                ):
                    value = (entry.row.get(field) or "").strip()
                    if value:
                        print(f"  {label}: {value}")
                codes = entry.row.get("codes", "") or ""
                cves = parse_cves(codes)
                if cves:
                    print(f"  CVEs: {', '.join(map(str, cves))}")
                other_codes = [
                    code.strip()
                    for code in codes.split(";")
                    if code.strip() and not CVE_PATTERN.fullmatch(code.strip())
                ]
                if other_codes:
                    print(f"  Other codes: {'; '.join(other_codes)}")
            else:
                print(f"{title:<{left_width}} | {right}")
        print()


def build_parser():
    parser = argparse.ArgumentParser(
        description="Search a local Exploit-DB checkout using SearchSploit-style matching."
    )
    parser.add_argument("terms", nargs="*", metavar="TERM", help="search terms (SearchSploit style)")
    parser.add_argument("--search", nargs="+", dest="search_terms", metavar="TERM",
                        help="search terms (also supports the earlier --search interface)")
    action = parser.add_mutually_exclusive_group()
    action.add_argument("-x", "--examine", "--details", dest="details", metavar="EDB_ID",
                        help="display metadata and the local exploit text")
    action.add_argument("-p", "--path", dest="path_id", metavar="EDB_ID",
                        help="display entry metadata and its local file path")
    action.add_argument("-m", "--mirror", dest="mirror_id", metavar="EDB_ID",
                        help="copy an entry's file into the current directory")
    action.add_argument("--cve", metavar="CVE", help="search CVE identifiers in the codes column")
    parser.add_argument("-n", "--nmap", type=Path, metavar="FILE.XML",
                        help="search service names/products/versions from Nmap XML output")
    parser.add_argument("-c", "--case", action="store_true", help="case-sensitive search")
    parser.add_argument("-e", "--exact", action="store_true", help="ordered exact phrase in title (implies --title)")
    parser.add_argument("-s", "--strict", action="store_true", help="disable fuzzy version-range matching")
    parser.add_argument("-t", "--title", action="store_true", help="search title only; default also searches file path")
    parser.add_argument("--exclude", help="exclude matches to this regular expression")
    parser.add_argument("--category", metavar="REGEX", help="additional regex filter over title/path/type/platform/tags")
    parser.add_argument("--column-include", type=parse_column_include, action="append", metavar="FILTER",
                        help="exact, case-insensitive CSV filter; repeatable")
    parser.add_argument("--min-age", type=parse_age_limit, metavar="AGE",
                        help="minimum published age (e.g. 5y, 5m, 5d; bare number means years)")
    parser.add_argument("--max-age", type=parse_age_limit, metavar="AGE",
                        help="maximum published age (e.g. 5y, 5m, 5d; bare number means years)")
    parser.add_argument("-j", "--json", action="store_true", help="print JSON results")
    parser.add_argument("-v", "--verbose", action="store_true", help="include EDB-ID, publication date, and codes")
    parser.add_argument("-w", "--www", action="store_true", help="show Exploit-DB URLs")
    parser.add_argument("--id", dest="id_only", action="store_true", help="show EDB IDs instead of paths")
    parser.add_argument("-o", "--overflow", action="store_true", help="do not truncate result titles")
    parser.add_argument("--disable-colour", "--disable-color", action="store_true", help="disable ANSI highlighting")
    parser.add_argument("--db", type=Path, default=DEFAULT_DATABASE,
                        help=f"Exploit-DB CSV path (default: {DEFAULT_DATABASE})")
    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()

    terms = args.search_terms if args.search_terms is not None else args.terms
    if args.search_terms is not None and args.terms:
        parser.error("use either positional terms or --search, not both")
    if args.nmap and (terms or args.cve or args.details or args.path_id or args.mirror_id):
        parser.error("--nmap cannot be combined with terms, --cve, or entry actions")
    if args.details or args.path_id or args.mirror_id:
        if terms or args.cve or args.category or args.column_include or args.min_age or args.max_age:
            parser.error("entry actions cannot be combined with search filters")
    elif not terms and not args.cve and not args.nmap and not args.category and not args.column_include and not args.min_age and not args.max_age:
        parser.error("provide search terms or at least one filter")
    if args.exact and not terms:
        parser.error("--exact requires search terms")

    try:
        exclude_pattern = re.compile(args.exclude, 0 if args.case else re.IGNORECASE) if args.exclude else None
        category_pattern = re.compile(args.category, re.IGNORECASE) if args.category else None
    except re.error as error:
        parser.error(f"invalid regular expression: {error}")

    database = args.db.expanduser()
    try:
        entries, columns_by_kind = load_entries(database)
    except (OSError, csv.Error) as error:
        print(f"Could not read Exploit-DB index: {error}", file=sys.stderr)
        return 2

    if args.column_include:
        all_columns = {column.casefold(): column for columns in columns_by_kind.values() for column in columns}
        normalized = []
        for column_filter in args.column_include:
            column = all_columns.get(column_filter.column_name.casefold())
            if column is None:
                parser.error(f"unknown CSV column {column_filter.column_name!r}; available: {', '.join(sorted(all_columns.values()))}")
            normalized.append(ColumnInclude(column, column_filter.candidates))
        args.column_include = normalized

    def find_id(value):
        requested = value.upper().removeprefix("EDB-")
        return next((entry for entry in entries if entry.id == requested), None)

    if args.details or args.path_id or args.mirror_id:
        requested_id = args.details or args.path_id or args.mirror_id
        entry = find_id(requested_id)
        if entry is None:
            print(f"Exploit-DB entry {requested_id} not found.", file=sys.stderr)
            return 1
        if args.mirror_id:
            if not entry.full_path or not entry.full_path.is_file():
                print("The indexed exploit file is missing from this checkout.", file=sys.stderr)
                return 2
            destination = Path.cwd() / entry.full_path.name
            shutil.copy2(entry.full_path, destination)
            print(f"Copied to: {destination}")
            return 0
        return details(entry, show_source=bool(args.details))

    if args.cve:
        cve_query = args.cve.upper()
        if not cve_query.startswith("CVE-"):
            cve_query = f"CVE-{cve_query}"
        cve_pattern = re.compile(rf"(?<![A-Z0-9]){re.escape(cve_query)}(?![A-Z0-9])", re.IGNORECASE)
    else:
        cve_pattern = None

    today = date.today()
    min_cutoff = age_cutoff(args.min_age, today)
    max_cutoff = age_cutoff(args.max_age, today)
    if min_cutoff is not None and max_cutoff is not None and min_cutoff < max_cutoff:
        parser.error("--min-age cannot be greater than --max-age")

    search_queries = [tokenize_terms(terms)] if terms else []
    if args.nmap:
        try:
            search_queries = nmap_queries(args.nmap)
        except (OSError, ET.ParseError) as error:
            parser.error(f"could not read Nmap XML: {error}")
        if not search_queries:
            parser.error("Nmap XML contains no service names or product/version data")
        if args.verbose:
            query_labels = "; ".join(" ".join(query) for query in search_queries)
            print(f"[i] Nmap service queries: {query_labels}", file=sys.stderr)

    results = []
    for entry in entries:
        if search_queries and not any(
            terms_match(
                entry, query, exact=args.exact, title_only=args.title,
                case_sensitive=args.case, strict=args.strict,
            )
            for query in search_queries
        ):
            continue
        if cve_pattern and not cve_pattern.search(str(entry.row.get("codes", ""))):
            continue
        if category_pattern and not category_matches(entry, category_pattern):
            continue
        if args.column_include and not column_includes_match(entry, args.column_include):
            continue
        if exclude_pattern and exclude_pattern.search(" ".join(str(value or "") for value in entry.row.values())):
            continue
        if args.min_age or args.max_age:
            published = published_date(entry)
            if published is None:
                continue
            if min_cutoff is not None and published > min_cutoff:
                continue
            if max_cutoff is not None and published < max_cutoff:
                continue
        results.append(entry)

    search_label = (
        " ".join(term for query in search_queries for term in query)
        if search_queries
        else (args.cve or args.category or "")
    )
    print_results(results, search_label, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
