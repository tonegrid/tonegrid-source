"""Validate editable ToneGrid language packs; no external service or build needed."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def checked_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=checked_object)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', nargs='?', type=Path, default=ROOT / 'Resources/Translations')
    args = parser.parse_args()
    reference = load(ROOT / 'Resources/Translations/en.json')['strings']
    files = sorted(args.folder.glob('*.json'))
    if not files:
        raise SystemExit('No language packs found')
    for path in files:
        pack = load(path)
        assert type(pack['schemaVersion']) is int and pack['schemaVersion'] == 1, path
        assert pack['locale'] == path.stem and re.fullmatch(r'[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*', path.stem), path
        assert isinstance(pack['nativeName'], str) and pack['nativeName'].strip(), path
        strings = pack['strings']
        assert isinstance(strings, dict), path
        for key, value in strings.items():
            assert isinstance(value, str) and value.strip(), (path, key, 'Empty or non-string translation')
            if key in reference:
                assert set(re.findall(r'\{\w+\}', value)) == set(re.findall(r'\{\w+\}', reference[key])), (path, key, 'Placeholders differ')
        missing, extra = set(reference) - strings.keys(), strings.keys() - reference.keys()
        print(f'{path.name}: {len(strings)} strings; {len(missing)} fallback keys; {len(extra)} future keys')
    print(f'Validated {len(files)} language packs.')


if __name__ == '__main__':
    main()
