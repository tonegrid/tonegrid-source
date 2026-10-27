"""Portable delivery-path budgets, independent of the development checkout path."""
from pathlib import PurePosixPath

MAX_AMP_FILENAME = 64
MAX_AMP_ARCHIVE_PATH = 150
MAX_WINDOWS_ARCHIVE_PATH = 160
EXTRACTION_PREFIX_BUDGET = 90


def windows_units(value):
    return len(str(value).encode('utf-16-le')) // 2


def validate_runtime_path(relative, version):
    parts = PurePosixPath(relative).parts
    if not parts or relative.startswith('/') or '\\' in relative or any(p in ('.', '..') for p in parts):
        raise ValueError('Unsafe runtime path: ' + relative)
    archived = f'ToneGrid-{version}/' + relative
    if windows_units(archived) > MAX_WINDOWS_ARCHIVE_PATH:
        raise ValueError('Delivery path too long; shorten its physical name: ' + archived)
    if any(len(p.encode('utf-8')) > 240 or windows_units(p) > 240 for p in parts):
        raise ValueError('Delivery filename component too long: ' + relative)
    if relative.startswith('FactoryAmps/') and relative.lower().endswith('.nam'):
        if windows_units(parts[-1]) > MAX_AMP_FILENAME or windows_units(archived) > MAX_AMP_ARCHIVE_PATH:
            raise ValueError('Factory amp capture needs a shorter physical filename: ' + relative)
    return windows_units(archived)


def validate_inventory(relative_paths, version):
    seen = set()
    longest = 0
    for name in relative_paths:
        key = name.casefold()
        if key in seen:
            raise ValueError('Case-insensitive delivery path collision: ' + name)
        seen.add(key)
        longest = max(longest, validate_runtime_path(name, version))
    # Leave space for a realistic extraction directory, the NUL and spare room.
    assert longest + EXTRACTION_PREFIX_BUDGET + 1 < 260
    return longest
