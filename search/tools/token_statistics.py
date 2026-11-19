#!/usr/bin/env python3
"""Measure LR1 token lengths and compare candidate alphabets on the exported corpus."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re


TOKEN_ALPHABET = "A-Za-z0-9А-Яа-яЁё"
TOKEN = re.compile(rf"[{TOKEN_ALPHABET}]+")
ASCII_TOKEN = re.compile(r"[A-Za-z0-9]+")
UNICODE_TOKEN = re.compile(r"[^\W_]+")
OUTSIDE_ALPHABET = re.compile(rf"[^{TOKEN_ALPHABET}]")
CYRILLIC = re.compile(r"[А-Яа-яЁё]")
LATIN = re.compile(r"[A-Za-z]")
DIGIT = re.compile(r"[0-9]")
ESCAPE = re.compile(r"\\(.)?", re.DOTALL)
REPLACEMENTS = {"\\": "\\", "t": "\t", "n": "\n", "r": "\r"}


def decode_field(field: str) -> str:
    def replace(match: re.Match) -> str:
        escaped = match.group(1)
        if escaped not in REPLACEMENTS:
            raise ValueError("Invalid TSV escape")
        return REPLACEMENTS[escaped]

    return ESCAPE.sub(replace, field)


def decode_document(line: str) -> tuple[str, str]:
    fields = line.split("\t")
    if len(fields) != 3:
        raise ValueError("Expected exactly three TSV fields")
    title, url, body = (decode_field(field) for field in fields)
    if not title or not url:
        raise ValueError("Title and URL must not be empty")
    return title, body


def calculate_statistics(corpus: Path, index_metrics: Path | None = None) -> dict:
    documents = text_bytes = tokens = token_characters = token_bytes = 0
    with corpus.open(encoding="utf-8-sig", newline="") as source:
        for line_number, line in enumerate(source, 1):
            if line.endswith("\n"):
                line = line[:-1]
            if line.endswith("\r"):
                line = line[:-1]
            if not line:
                continue
            try:
                title, body = decode_document(line)
                documents += 1
                # Separate fields preserve the title/body token boundary.
                for field in (title, body):
                    text_bytes += len(field.encode("utf-8"))
                    for match in TOKEN.finditer(field):
                        token = match.group().lower()
                        tokens += 1
                        token_characters += len(token)
                        token_bytes += len(token.encode("utf-8"))
            except ValueError as error:
                raise ValueError(f"Corpus line {line_number}: {error}") from error
    result = {
        "documents": documents,
        "text_bytes": text_bytes,
        "tokens": tokens,
        "token_characters": token_characters,
        "token_bytes": token_bytes,
        "average_token_characters": token_characters / tokens if tokens else 0.0,
        "average_token_bytes": token_bytes / tokens if tokens else 0.0,
        "tokenization": "[A-Za-z0-9А-Яа-яЁё]+; lowercase; title and body; no stop-word removal",
    }
    if index_metrics is not None:
        cpp = json.loads(index_metrics.read_text(encoding="utf-8"))
        matches = (
            all(result[key] == cpp[key] for key in ("documents", "tokens", "text_bytes"))
            and abs(result["average_token_characters"] - cpp["average_token_characters"]) <= 0.0000005
        )
        result["cpp_index_metrics_match"] = matches
        result["unique_terms_cpp"] = cpp["terms"]
        result["average_term_characters_cpp"] = cpp["average_term_characters"]
        result["term_minus_token_characters"] = cpp["average_term_characters"] - result["average_token_characters"]
        if not matches:
            raise ValueError("Python token counts/lengths do not match C++ indexing metrics")
    return result


def profile_alphabet(corpus: Path) -> dict:
    """Compare the implemented alphabet with ASCII and Unicode alphanumeric runs."""
    characters = Counter()
    outside_alphabet = Counter()
    patterns = {"ascii": ASCII_TOKEN, "current": TOKEN, "unicode_alnum": UNICODE_TOKEN}
    variants = {name: Counter(tokens=0, code_points=0) for name in patterns}
    token_features = Counter()
    case_sensitive, lowercase, without_yo = set(), set(), set()
    documents = 0

    with corpus.open(encoding="utf-8-sig", newline="") as source:
        for line_number, line in enumerate(source, 1):
            line = line.removesuffix("\n").removesuffix("\r")
            if not line:
                continue
            try:
                title, body = decode_document(line)
            except ValueError as error:
                raise ValueError(f"Corpus line {line_number}: {error}") from error
            documents += 1
            for field in (title, body):
                characters["code_points"] += len(field)
                characters["utf8_bytes"] += len(field.encode("utf-8"))
                for name, pattern in patterns.items():
                    for match in pattern.finditer(field):
                        raw = match.group()
                        variants[name]["tokens"] += 1
                        variants[name]["code_points"] += len(raw)
                        if name != "current":
                            if name == "unicode_alnum" and (excluded := OUTSIDE_ALPHABET.findall(raw)):
                                token_features["unicode_tokens_with_excluded_characters"] += 1
                                outside_alphabet.update(excluded)
                            continue
                        term = raw.lower()
                        cyrillic = CYRILLIC.search(raw) is not None
                        latin = LATIN.search(raw) is not None
                        token_features["with_cyrillic"] += cyrillic
                        token_features["with_latin"] += latin
                        token_features["mixed_cyrillic_and_latin"] += cyrillic and latin
                        token_features["with_ascii_digits"] += DIGIT.search(raw) is not None
                        token_features["only_ascii_digits"] += raw.isascii() and raw.isdigit()
                        token_features["with_yo"] += "ё" in term
                        token_features["changed_by_lowercasing"] += raw != term
                        case_sensitive.add(raw)
                        lowercase.add(term)
                        without_yo.add(term.replace("ё", "е"))

    retained = variants["current"]["code_points"]
    ascii_retained = variants["ascii"]["code_points"]
    characters["current_ascii"] = ascii_retained
    characters["current_cyrillic"] = retained - ascii_retained
    characters["other_alphanumeric"] = variants["unicode_alnum"]["code_points"] - retained
    characters["non_alphanumeric"] = characters["code_points"] - variants["unicode_alnum"]["code_points"]
    return {
        "documents": documents,
        "characters": dict(characters),
        "variants": {name: dict(counts) for name, counts in variants.items()},
        "current_token_features": dict(token_features),
        "distinct_current_tokens": {
            "case_sensitive": len(case_sensitive),
            "lowercase": len(lowercase),
            "lowercase_with_yo_folded": len(without_yo),
        },
        "top_excluded_alphanumeric": [
            {"character": character, "code_point": f"U+{ord(character):04X}", "count": count}
            for character, count in outside_alphabet.most_common(12)
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--index-metrics", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--profile-alphabet", action="store_true", help="Compare token alphabets and normalization")
    args = parser.parse_args()
    if args.profile_alphabet and args.index_metrics is not None:
        parser.error("--index-metrics applies to the default token-length report")
    if args.profile_alphabet:
        result = profile_alphabet(args.corpus)
    else:
        result = calculate_statistics(args.corpus, args.index_metrics)
    formatted = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output is not None:
        if args.output.resolve() in (args.corpus.resolve(), args.index_metrics.resolve() if args.index_metrics else None):
            parser.error("Output must differ from the source files")
        args.output.write_text(formatted, encoding="utf-8")
    print(formatted, end="")


if __name__ == "__main__":
    main()
