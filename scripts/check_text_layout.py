#!/usr/bin/env python3
"""Pure UTF-8 line-break regression checks for the client text layout."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / "src/engine/client/text.cpp").read_text(encoding="utf-8")

OPENING = set("([{<‘“〈《「『【〔〖〘〚（［｛")
CLOSING = set(")]}>,'’”〉》」』】〕〗〙〛），。！？：；、．？！］｝")
INTERNAL = set("'’-_.,:;/\\@#%&=+?~")


def is_cjk(char: str) -> bool:
    value = ord(char)
    return (
        0x2E80 <= value <= 0x9FFF
        or 0xF900 <= value <= 0xFAFF
        or 0x3040 <= value <= 0x30FF
        or 0xAC00 <= value <= 0xD7AF
        or 0x20000 <= value <= 0x3134F
    )


def is_latin_word(char: str) -> bool:
    value = ord(char)
    return char.isascii() and char.isalnum() or 0x00C0 <= value <= 0x02AF


def is_token(char: str) -> bool:
    return is_latin_word(char) or char in INTERNAL


def can_break(left: str, right: str) -> bool:
    if left == "\n" or left in " \t\u3000":
        return True
    if left in OPENING or right in CLOSING:
        return False
    if is_token(left) and is_token(right):
        return False
    return is_cjk(left) or is_cjk(right) or left in CLOSING or right in OPENING


def runs(text: str) -> list[str]:
    if not text:
        return []
    result: list[str] = []
    start = 0
    for index in range(1, len(text)):
        if can_break(text[index - 1], text[index]):
            result.append(text[start:index])
            start = index
    result.append(text[start:])
    return result


def layout(text: str, width: int, *, stop_at_end: bool = False) -> list[str]:
    if width < 0:
        return text.split("\n")
    if stop_at_end:
        return [text.split("\n", 1)[0][:width]]
    lines = [""]
    for run in runs(text):
        while run:
            newline = run.find("\n")
            if newline == 0:
                lines.append("")
                run = run[1:]
                continue
            segment = run if newline < 0 else run[:newline]
            remaining = width - len(lines[-1])
            if len(segment) <= remaining:
                lines[-1] += segment
                run = run[len(segment):]
            elif len(segment) <= width:
                lines.append("")
            else:
                if remaining == 0:
                    lines.append("")
                    continue
                lines[-1] += segment[:remaining]
                run = run[remaining:]
                if run:
                    lines.append("")
    return lines


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    for token in (
        "IsCjk",
        "IsOpeningPunctuation",
        "IsClosingPunctuation",
        "IsLatinWordToken",
        "WidthTolerance",
        "pCursor->m_LineWidth > 0",
    ):
        require(token in SOURCE, f"text layout implementation missing {token}")
    require("Cutter.m_CharCount <= 3" not in SOURCE, "legacy three-character wrap heuristic returned")

    require(runs("alpha beta") == ["alpha ", "beta"], "Latin words must stay intact")
    require(runs("1,234.50 v10.2") == ["1,234.50 ", "v10.2"], "numbers must retain internal punctuation")
    require(runs("https://ninslash.example/a-b?q=1") == ["https://ninslash.example/a-b?q=1"], "URL split early")
    require(runs("中文测试") == ["中", "文", "测", "试"], "CJK character breaks missing")
    require(runs("中文，测试") == ["中", "文，", "测", "试"], "closing punctuation may start a line")
    require(runs("中文（测试）") == ["中", "文", "（测", "试）"], "paired punctuation rule broken")
    require(layout("ab 1234567", 6) == ["ab 123", "4567"], "short remaining width was discarded")
    require(layout("超长EnglishToken", 5) == ["超长Eng", "lishT", "oken"], "mixed over-wide run did not progress")
    require(layout("first\n\nthird", 20) == ["first", "", "third"], "explicit empty line lost")
    require(layout("abcdef", 3, stop_at_end=True) == ["abc"], "STOP_AT_END wrapped instead of truncating")
    require(layout("abcdef", -1, stop_at_end=True) == ["abcdef"], "negative width is not infinite")
    print("OK: UTF-8 break opportunities, punctuation, URLs, truncation and short-fragment layout")


if __name__ == "__main__":
    main()
