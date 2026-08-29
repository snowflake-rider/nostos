#!/usr/bin/env python3
"""Check the product layout and local Markdown links without opening devices."""
from pathlib import Path
from urllib.parse import unquote, urlsplit
import os
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
IGNORED = {'.git', '.build', 'DerivedData', 'managed_components', 'node_modules',
           '__pycache__', '.cache', '.venv', 'xcuserdata', '.pytest_cache', 'dist'}
LINK = re.compile(r'!?\[[^\]\n]*\]\((<[^>]+>|[^)\n]+)\)')


def files(root):
    for directory, dirs, names in os.walk(root):
        dirs[:] = [d for d in dirs if d not in IGNORED and
                   not d.lower().startswith('build') and d not in {'Debug', 'Release'}]
        for name in names:
            yield Path(directory) / name


def local_target(document, raw):
    raw = raw.strip()
    if raw.startswith('<'):
        raw = raw[1:raw.index('>')]
    else:
        raw = re.sub(r'\s+["\x27].*["\x27]$', '', raw)
    url = urlsplit(raw)
    if url.scheme or url.netloc or not url.path:
        return None
    return (document.parent / unquote(url.path)).resolve()


def check(root):
    errors, links, documents = [], 0, 0
    for retired in ('code', 'esp-ble-unorganized'):
        if (root / retired).exists():
            errors.append(f'Retired source tree: {retired}/')
    for path in files(root):
        rel = path.relative_to(root)
        if path.suffix == '.md':
            documents += 1
            text = re.sub(r'```.*?```', '', path.read_text(), flags=re.S)
            for match in LINK.finditer(text):
                target = local_target(path, match.group(1))
                if target is not None:
                    links += 1
                    if not target.is_relative_to(root.resolve()) or not target.exists():
                        errors.append(f'{rel}: missing/outside target {match.group(1)}')
        if rel.parts[0] == 'docs' and path.suffix in {'.c', '.h', '.swift', '.ioc', '.elf', '.bin'}:
            errors.append(f'Build input/output under docs/: {rel}')
    return errors, documents, links


def main():
    errors, documents, links = check(ROOT)
    for error in errors:
        print(error)
    print(f'REPOSITORY_CHECK={"FAIL" if errors else "PASS"} documents={documents} local_links={links} errors={len(errors)}')
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
