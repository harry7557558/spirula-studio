#!/usr/bin/env python3
"""Check changed catalog copy for length and terminal character support."""

import argparse
import ast
from bisect import bisect_left
from functools import lru_cache
import json
import pathlib
import re
import subprocess
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parent.parent
CATALOG = pathlib.Path('src/i18n/catalog')
ASCII_TABLE = pathlib.Path('src/i18n/AsciiPunctuation.inc')
CLI = pathlib.Path('src/app/cli')
MAIN = pathlib.Path('src/app/Main.cpp')
LICENSES = pathlib.Path('src/core/LicenseTexts.cpp')
POLICY = pathlib.Path('tools/ui_text_policy.json')
TOKEN = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|'
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"|'
    r'(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|'
    r"\b[0-9][A-Za-z0-9_'.]*|(?:u8|u|U|L)?'(?:\\.|[^'\\\n])*'|[A-Za-z_]\w*|[^\s]")


@lru_cache(maxsize=32768)
def literal(token):
    return ast.literal_eval(token)


def cpp_string(token):
    token = re.sub(r'''^(?:u8|u|U|L)(?=R?"|')''', '', token)
    if token.startswith('R"'):
        delimiter = token[2:token.index('(')]
        return token[len(delimiter) + 3:-(len(delimiter) + 2)]
    return literal(token)


def messages(source):
    tokens, comments = [], []
    for match in TOKEN.finditer(source):
        token = match.group()
        if token.startswith(('//', '/*')):
            comments.append(match.span())
        else:
            tokens.append((token, match.start()))
    newlines = [m.start() for m in re.finditer('\n', source)]
    result = {}
    i = 0
    while i < len(tokens):
        if tokens[i][0] not in ('SS_MSG', 'SS_MSG_EN'):
            i += 1
            continue
        start = i
        macro = tokens[i][0]
        if tokens[i + 1][0] != '(' or tokens[i + 3][0] != ',':
            raise ValueError('unsupported message declaration')
        name = tokens[i + 2][0]
        i += 4
        translations = {}
        while tokens[i][0] != ')':
            if tokens[i][0] == ',':
                i += 1
                continue
            lang = 'EN' if macro == 'SS_MSG_EN' else tokens[i][0]
            if macro != 'SS_MSG_EN':
                if tokens[i + 1][0] != '(':
                    raise ValueError(f'{name}: expected language tag')
                i += 2
            parts = []
            while tokens[i][0].startswith('"'):
                parts.append(literal(tokens[i][0]))
                i += 1
            if not parts:
                raise ValueError(f'{name}: expected string literal')
            if lang in translations:
                raise ValueError(f'{name}: duplicate language {lang}')
            translations[lang] = ''.join(parts)
            if macro != 'SS_MSG_EN':
                if tokens[i][0] != ')':
                    raise ValueError(f'{name}: unsupported string expression')
                i += 1
        if name in result:
            raise ValueError(f'duplicate message {name}')
        result[name] = {'line': bisect_left(newlines, tokens[start][1]) + 1,
                        'translations': translations}
        i += 1
    for declaration in re.finditer(r'^\s*SS_MSG(?:_EN)?\(\s*(\w+)\s*,', source, re.MULTILINE):
        if any(begin <= declaration.start() < end for begin, end in comments):
            continue
        if declaration[1] not in result:
            raise ValueError(f'{declaration[1]}: declaration hidden by malformed catalog syntax')
    return result


@lru_cache(maxsize=8192)
def cell_width(char):
    if unicodedata.category(char) in ('Mn', 'Me', 'Cf'):
        return 0
    return 2 if unicodedata.east_asian_width(char) in ('W', 'F') else 1


def wrapped_lines(text, columns):
    lines = 1
    used = 0
    tokens = []
    for run in re.findall(r'\n|[^\S\n]+|[^\s]+', text):
        part = ''
        for char in run:
            if cell_width(char) == 2:
                if part:
                    tokens.append(part)
                    part = ''
                tokens.append(char)
            else:
                part += char
        if part:
            tokens.append(part)
    for token in tokens:
        if token == '\n':
            lines += 1
            used = 0
        elif token.isspace():
            used = min(columns, used + sum(cell_width(c) for c in token))
        else:
            width = sum(cell_width(c) for c in token)
            if used and used + width > columns:
                lines += 1
                used = 0
            for char in token:
                width = cell_width(char)
                if used + width > columns:
                    lines += 1
                    used = 0
                used += width
    return lines


def text_budget(text, columns, max_lines, list_policy):
    lines = wrapped_lines(text, columns)
    parts = text.split('\n')
    bullet_pattern = r'(?:- |• )(\S.*)'
    first = 0 if parts[0].startswith(('- ', '• ')) else 1
    bullets = [re.fullmatch(bullet_pattern, part) for part in parts[first:]]
    if bullets and all(bullets) and (first == 0 or parts[0].strip()):
        budget = list_policy['max_total_lines']
        if first and wrapped_lines(parts[0], columns) > max_lines:
            return lines, budget, 'overview exceeds prose line budget'
        if len(bullets) > list_policy['max_items']:
            return lines, budget, 'too many bullet items'
        if any(wrapped_lines(item[0], columns) > list_policy['max_item_lines'] or
               wrapped_lines(item[1], max(2, columns - 2)) > list_policy['max_item_lines']
               for item in bullets):
            return lines, budget, 'bullet item exceeds line budget'
        if lines > budget:
            return lines, budget, 'list exceeds total line budget'
        return lines, budget, None
    if (len(parts) == 2 and all(part.strip() for part in parts)
            and not any(part.startswith(('- ', '• ')) for part in parts)
            and wrapped_lines(parts[0], columns) <= max_lines
            and wrapped_lines(parts[1], columns) <= list_policy['max_item_lines']):
        budget = min(max_lines + list_policy['max_item_lines'], list_policy['max_total_lines'])
        return lines, budget, 'prose exceeds total line budget' if lines > budget else None
    return lines, max_lines, 'prose exceeds line budget' if lines > max_lines else None


def role_for(file, name, policy):
    key = f'{file}:{name}'
    if key in policy['messages']:
        return policy['messages'][key]['role'], 'explicit'
    if file in policy['catalogs']:
        return policy['catalogs'][file]['role'], 'catalog'
    if name.startswith('usage_') or name.endswith('_usage'):
        return 'cli', 'inferred'
    if '_help' in name or name.startswith(('hint_', 'tp_')):
        return 'tooltip', 'inferred'
    return 'interface', 'inferred'


def git(*args):
    try:
        result = subprocess.run(['git', *args], cwd=ROOT, capture_output=True, encoding='utf-8')
    except FileNotFoundError:
        return None
    return result.stdout if result.returncode == 0 else None


def changed_paths(base):
    rev = git('rev-parse', '--verify', f'{base or "HEAD"}^{{commit}}')
    if rev is None:
        if base:
            raise ValueError(f'invalid base revision: {base}')
        if git('rev-parse', '--show-toplevel') is None:
            return None, set()
        rev = '4b825dc642cb6eb9a060e54bf8d69288fbee4904'
    rev = rev.strip()
    paths = list(map(str, (CATALOG, CLI, MAIN, LICENSES, ASCII_TABLE, POLICY)))
    changed = git('diff', '--name-only', '-z', '--no-renames', '--diff-filter=d', rev, '--', *paths)
    untracked = git('ls-files', '--others', '--exclude-standard', '-z', '--', *paths)
    if changed is None or untracked is None:
        raise ValueError(f'cannot compare sources with {rev}')
    return rev, {pathlib.Path(p) for p in (changed + untracked).split('\0') if p}


def ascii_mappings():
    source = (ROOT / ASCII_TABLE).read_text(encoding='utf-8')
    mappings = {}
    for line in source.splitlines():
        match = re.fullmatch(r'SS_ASCII_PUNCTUATION\((0x[0-9A-Fa-f]+), (".*")\)', line)
        if not match:
            raise ValueError(f'invalid ASCII mapping: {line}')
        char = chr(int(match[1], 16))
        replacement = literal(match[2])
        if char.isascii() or char in mappings or not replacement.isascii():
            raise ValueError(f'invalid ASCII replacement for U+{ord(char):04X}')
        mappings[char] = replacement
    return mappings


def unsupported_chars(text, mappings):
    if text.isascii():
        return []
    return sorted({f'U+{ord(c):04X} ({c})' for c in text if not c.isascii() and c not in mappings})


def survey(args):
    policy = json.loads((ROOT / POLICY).read_text(encoding='utf-8'))
    for section in ('catalogs', 'messages'):
        for key, entry in policy[section].items():
            if entry['role'] not in policy['roles'] or not entry.get('reason', '').strip():
                raise ValueError(f'{key}: assignment needs a known role and a reason')
    exclusions = policy.get('exclusions', {})
    for key, exclusion in exclusions.items():
        if not exclusion.get('reason', '').strip() or not exclusion.get('usage', '').strip():
            raise ValueError(f'{key}: exclusion needs a reason and a usage location')
    list_policy = policy.get('lists', {'max_items': 4, 'max_item_lines': 2, 'max_total_lines': 8})
    base, paths = (None, set()) if args.all else changed_paths(args.base)
    full = args.all or POLICY in paths or ASCII_TABLE in paths
    catalog_paths = (ROOT / CATALOG).glob('*.h') if full else (
        ROOT / p for p in paths if p.parent == CATALOG and p.suffix == '.h')
    mappings = ascii_mappings() if (ROOT / ASCII_TABLE).exists() else {}
    ascii_translation = str.maketrans(mappings)
    findings, ascii_findings = [], []
    examined = 0
    for path in sorted(catalog_paths):
        current = messages(path.read_text(encoding='utf-8'))
        previous = {}
        if not full:
            old = git('show', f'{base}:{path.relative_to(ROOT).as_posix()}')
            if old is not None:
                previous = messages(old)
        for name, message in current.items():
            if not full and previous.get(name, {}).get('translations') == message['translations']:
                continue
            examined += 1
            unsupported = unsupported_chars(message['translations'].get('EN', ''), mappings)
            if unsupported:
                ascii_findings.append({'file': str(path.relative_to(ROOT)), 'line': message['line'],
                                       'message': name, 'characters': unsupported})
            role, classification = role_for(path.name, name, policy)
            budget = policy['roles'][role]['max_lines']
            if budget is None:
                continue
            for lang, text in message['translations'].items():
                renderings = [('catalog', text)]
                if lang == 'EN' and not text.isascii():
                    terminal_text = text.translate(ascii_translation)
                    if terminal_text != text:
                        renderings.append(('terminal', terminal_text))
                for rendering, content in renderings:
                    lines, effective_budget, issue = text_budget(content, args.columns, budget, list_policy)
                    if issue:
                        findings.append({'file': str(path.relative_to(ROOT)), 'line': message['line'],
                                         'message': name, 'locale': lang, 'role': role,
                                         'classification': classification, 'estimated_lines': lines,
                                         'max_lines': effective_budget, 'issue': issue, 'text': text,
                                         'rendering': rendering,
                                         'exclusion': exclusions.get(f'{path.name}:{name}')})
                        break
    cli_paths = ({MAIN, LICENSES} | {p.relative_to(ROOT) for p in (ROOT / CLI).rglob('*')}) if full else paths
    for path in sorted(cli_paths):
        if path not in (MAIN, LICENSES) and (CLI not in path.parents or path.suffix not in ('.cpp', '.h')):
            continue
        if not (ROOT / path).is_file():
            continue
        source = (ROOT / path).read_text(encoding='utf-8')
        for match in TOKEN.finditer(source):
            token = match.group()
            if not re.match(r'''^(?:u8|u|U|L)?(?:R?"|')''', token):
                continue
            if token.isascii() and '\\' not in token:
                continue
            unsupported = unsupported_chars(cpp_string(token), mappings if path == LICENSES else {})
            if unsupported:
                ascii_findings.append({'file': str(path), 'line': source.count('\n', 0, match.start()) + 1,
                                       'message': 'CLI string literal', 'characters': unsupported})
    return {'columns': args.columns, 'messages_examined': examined,
            'translations_over_budget': sum(not f['exclusion'] for f in findings),
            'translations_excluded': sum(bool(f['exclusion']) for f in findings),
            'messages_excluded': len({(f['file'], f['message']) for f in findings if f['exclusion']}),
            'findings': findings, 'ascii_findings': ascii_findings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument('--all', action='store_true', help='survey existing debt too')
    scope.add_argument('--base', help='compare whole messages with this Git revision')
    parser.add_argument('--columns', type=int, default=60, help='approximate display columns per line (default: 60)')
    parser.add_argument('--json', action='store_true', help='emit all findings as JSON')
    parser.add_argument('--quiet', action='store_true', help='omit output when checks pass')
    parser.add_argument('--limit', type=int, default=40, help='maximum human-readable findings')
    args = parser.parse_args()
    if args.columns < 2 or args.limit < 0:
        parser.error('columns must be >= 2 and limit must be >= 0')
    try:
        report = survey(args)
    except (ValueError, IndexError, SyntaxError) as error:
        parser.exit(2, f'check_ui_text: {error}\n')
    failed = bool(report['translations_over_budget'] or report['ascii_findings'])
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return int(failed)
    if args.quiet and not failed:
        return 0
    for finding in report['ascii_findings']:
        print('{file}:{line}: {message} cannot print as ASCII: {characters}'.format(
            **{**finding, 'characters': ', '.join(finding['characters'])}))
    findings = sorted(report['findings'], key=lambda f: (not f['exclusion'], f['estimated_lines']), reverse=True)
    for finding in findings[:args.limit]:
        print('{file}:{line}: {message} [{locale}] {role} ({classification}, {rendering}): '
              '~{estimated_lines} lines, budget {max_lines}; {issue}'.format(**finding))
        if finding['exclusion']:
            print(f"  EXCLUDED: {finding['exclusion']['reason']}")
            print(f"  Usage: {finding['exclusion']['usage']}")
    print(f"check_ui_text: {report['messages_examined']} messages examined; "
          f"{report['translations_over_budget']} translations over budget at {args.columns} columns; "
          f"{report['translations_excluded']} translations in "
          f"{report['messages_excluded']} messages excluded by policy; "
          f"{len(report['ascii_findings'])} ASCII violations")
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
