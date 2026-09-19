"""Portable path references for exported diagnostics, never execution paths."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def public_text(text):
    """Remove this checkout/home prefix, including paths inside diagnostics."""
    for prefix, replacement in ((ROOT, ""), (Path.home(), "~/")):
        if prefix == Path("/"):
            continue
        text = text.replace(prefix.as_posix() + "/", replacement)
        # Also handle the directory itself without matching a sibling prefix.
        text = re.sub(re.escape(prefix.as_posix()) + r"(?=$|[\s\"'<>:,;)])",
                      "." if not replacement else "~", text)
    return text


def public_path(path):
    """Repository paths are relative to ROOT; other home paths use ~/ notation."""
    return public_text(Path(path).absolute().as_posix())


def public_data(data):
    """Copy diagnostic data for display without changing live execution state.

    Do not use on executable source-edit plans or raw evidence archives: their
    source text must remain exact. This helper is for generated reports only.
    """
    if isinstance(data, str):
        return public_text(data)
    if isinstance(data, dict):
        return {public_text(k) if isinstance(k, str) else k: public_data(v) for k, v in data.items()}
    if isinstance(data, (list, tuple)):
        return [public_data(item) for item in data]
    return data
