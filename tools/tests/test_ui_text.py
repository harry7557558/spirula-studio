import argparse
import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('ui_text', pathlib.Path(__file__).parents[1] / 'check_ui_text.py')
ui = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ui)


class CopyMeasurementTest(unittest.TestCase):
    def test_unicode_width_instead_of_utf8_bytes(self):
        for text in ('abcd', 'абвг', '中文', 'ｅｆ', 'e\u0301abc'):
            self.assertEqual(ui.wrapped_lines(text, 4), 1, text)
            self.assertEqual(ui.wrapped_lines(text + 'x', 4), 2, text)

    def test_word_wrapping_and_explicit_lines(self):
        self.assertEqual(ui.wrapped_lines('one two three', 7), 2)
        self.assertEqual(ui.wrapped_lines('abc\n\ndef', 7), 3)
        self.assertEqual(ui.wrapped_lines('abcdefghijkl', 4), 3)
        self.assertEqual(ui.wrapped_lines('ab 中文中文', 6), 2)

    def test_literals_comments_and_escapes(self):
        source = r'''// SS_MSG(fake, EN("ignore"));
        SS_MSG(real, EN("Say \"yes\". " /* gap */ "Next\nline"), RU("Да"));
        SS_MSG_EN(other, "(text)");'''
        parsed = ui.messages(source)
        self.assertEqual(set(parsed), {'real', 'other'})
        self.assertEqual(parsed['real']['translations']['EN'], 'Say "yes". Next\nline')
        self.assertEqual(parsed['real']['translations']['RU'], 'Да')
        with self.assertRaises(ValueError):
            ui.messages('SS_MSG(bad, EN(make_text()));')

    def test_malformed_text_cannot_hide_following_messages(self):
        with self.assertRaisesRegex(ValueError, 'hidden'):
            ui.messages('"trailing old text\nSS_MSG(hidden, EN("more"));')
        source = '/* Example:\nSS_MSG(example, EN("text"));\n*/\nSS_MSG(real, EN("text"));'
        self.assertEqual(set(ui.messages(source)), {'real'})

    def test_bullet_lists_have_item_and_count_limits(self):
        policy = {'max_items': 4, 'max_item_lines': 2, 'max_total_lines': 8}
        check = lambda text: ui.text_budget(text, 12, 3, policy)[2]
        self.assertIsNone(check('- First item.\n- Second item.'))
        self.assertIsNone(check('• First item.\n• Second item.'))
        self.assertIsNone(check('- A\n- B\n- C\n- D'))
        self.assertEqual(check('- A\n- B\n- C\n- D\n- E'), 'too many bullet items')
        self.assertEqual(check('- ' + 'word ' * 8 + '\n- B'), 'bullet item exceeds line budget')
        self.assertIsNone(check('Intro\n- A\n- B\n- C\n- D'))
        self.assertIsNone(check('Intro\n- A'))
        self.assertEqual(check('word ' * 10 + '\n- A'), 'overview exceeds prose line budget')
        self.assertEqual(check('Intro\n' + '\n'.join(['- First item.'] * 4)),
                         'list exceeds total line budget')
        self.assertEqual(check('Intro\n\n- A\n- B'), 'prose exceeds line budget')
        self.assertIsNone(check('- 中文中文中\n- абвгдежзий'))
        long_words = '\n'.join('- ' + 'x' * 20 for _ in range(3))
        self.assertEqual(check(long_words), 'bullet item exceeds line budget')
        self.assertEqual(check('- ' + 'x' * 20 + '\n- B'), 'bullet item exceeds line budget')

    def test_short_followup_can_use_a_line_break(self):
        policy = {'max_items': 4, 'max_item_lines': 2, 'max_total_lines': 8}
        def issue(text):
            return ui.text_budget(text, 12, 3, policy)[2]
        overview = 'one two three four five six'
        self.assertIsNone(issue(overview + '\nShort note.'))
        self.assertIsNone(issue(overview + '\nMore useful context.'))
        self.assertEqual(issue(overview + '\n' + overview), 'prose exceeds line budget')
        self.assertEqual(issue(overview + '\n\nShort note.'), 'prose exceeds line budget')

    def test_cli_copy_uses_the_same_budget(self):
        policy = json.loads((ui.ROOT / 'tools/ui_text_policy.json').read_text(encoding='utf-8'))
        self.assertEqual(policy['roles']['cli']['max_lines'],
                         policy['roles']['tooltip']['max_lines'])

    def test_policy_priority(self):
        policy = json.loads((ui.ROOT / 'tools/ui_text_policy.json').read_text(encoding='utf-8'))
        self.assertEqual(ui.role_for('Dense.h', 'help', policy), ('cli', 'explicit'))
        self.assertEqual(ui.role_for('Dataset.h', 'mask_shrink_help', policy), ('tooltip', 'explicit'))
        self.assertEqual(ui.role_for('TrainFields.h', 'resume_help', policy), ('tooltip', 'inferred'))

    def test_exclusions_are_reported_separately(self):
        args = argparse.Namespace(all=True, base='HEAD', columns=60)
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / ui.CATALOG).mkdir(parents=True)
            (root / 'tools').mkdir()
            policy = {
                'roles': {'tooltip': {'max_lines': 3}}, 'catalogs': {}, 'messages': {},
                'exclusions': {'Example.h:item_help': {
                    'reason': 'A complete shortcut reference needs a layout decision.',
                    'usage': 'src/app/gui/ViewportPanel.cpp'}}}
            (root / 'tools/ui_text_policy.json').write_text(json.dumps(policy), encoding='utf-8')
            (root / ui.CATALOG / 'Example.h').write_text(
                'SS_MSG_EN(item_help, "' + 'word ' * 60 + '");', encoding='utf-8')
            with patch.object(ui, 'ROOT', root):
                report = ui.survey(args)
            self.assertEqual(report['translations_over_budget'], 0)
            self.assertEqual(report['translations_excluded'], 1)
            self.assertEqual(report['messages_excluded'], 1)
            self.assertEqual(report['findings'][0]['exclusion'],
                             policy['exclusions']['Example.h:item_help'])

    def test_changed_message_scope_and_new_catalog(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / ui.CATALOG).mkdir(parents=True)
            (root / 'tools').mkdir()
            (root / 'tools/ui_text_policy.json').write_text((ui.ROOT / 'tools/ui_text_policy.json').read_text(encoding='utf-8'), encoding='utf-8')
            catalog = root / ui.CATALOG / 'Example.h'
            catalog.write_text('SS_MSG(item_help, EN("short"), RU("кратко"));', encoding='utf-8')
            def git(*args):
                subprocess.run(['git', *args], cwd=root, check=True, capture_output=True)
            git('init')
            git('add', '.')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-m', 'base')
            args = argparse.Namespace(all=False, base='HEAD', columns=4)
            with patch.object(ui, 'ROOT', root):
                self.assertEqual(ui.survey(args)['messages_examined'], 0)
                catalog.write_text('SS_MSG(item_help, EN("short"), RU("' + 'длинно ' * 10 + '"));', encoding='utf-8')
                report = ui.survey(args)
                self.assertEqual(report['messages_examined'], 1)
                self.assertEqual([f['locale'] for f in report['findings']], ['RU'])
                (root / ui.CATALOG / 'New.h').write_text('SS_MSG_EN(new_help, "new");', encoding='utf-8')
                git('add', str(ui.CATALOG / 'New.h'))
                self.assertEqual(ui.survey(args)['messages_examined'], 2)
                args.base = 'missing-revision'
                with self.assertRaises(ValueError):
                    ui.survey(args)

    def test_ascii_catalog_check_cannot_be_length_excluded(self):
        mappings = ui.ascii_mappings()
        self.assertEqual(ui.unsupported_chars('“Quoted” — 90° → next…', mappings), [])
        self.assertEqual(ui.unsupported_chars('Café', mappings), ['U+00E9 (é)'])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / ui.CATALOG).mkdir(parents=True)
            (root / 'tools').mkdir()
            policy = {
                'roles': {'tooltip': {'max_lines': 3}}, 'catalogs': {}, 'messages': {},
                'exclusions': {'Example.h:item_help': {'reason': 'Reviewed.', 'usage': 'Example.cpp'}}}
            (root / ui.POLICY).write_text(json.dumps(policy), encoding='utf-8')
            (root / ui.CATALOG / 'Example.h').write_text(
                'SS_MSG(item_help, EN("Café"), FR("Café"));', encoding='utf-8')
            with patch.object(ui, 'ROOT', root):
                report = ui.survey(argparse.Namespace(all=True, base=None, columns=60))
            self.assertEqual(report['translations_over_budget'], 0)
            self.assertEqual(len(report['ascii_findings']), 1)
            (root / ui.ASCII_TABLE).write_text('SS_ASCII_PUNCTUATION(0x2026, "...")\n')
            (root / ui.CATALOG / 'Example.h').write_text(
                'SS_MSG_EN(expansion_help, "' + 'a… ' * 40 + '");', encoding='utf-8')
            with patch.object(ui, 'ROOT', root):
                report = ui.survey(argparse.Namespace(all=True, base=None, columns=60))
            self.assertEqual(report['translations_over_budget'], 1)
            self.assertEqual(report['findings'][0]['rendering'], 'terminal')

    def test_cli_literals_include_raw_prefixed_and_escaped_strings(self):
        source = r'''// "ignored é"
            const char quote = '"';
            int count = 1'000; "ASCII"; u8"é"; '\u00E9';
            "\u2014"; R"tag(raw é)tag"; u8R"(raw é)";'''
        strings = [m.group() for m in ui.TOKEN.finditer(source)
                   if ui.re.match(r'^(?:u8|u|U|L)?R?"', m.group())]
        self.assertEqual([ui.cpp_string(s) for s in strings],
                         ['ASCII', 'é', '—', 'raw é', 'raw é'])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / ui.CLI).mkdir(parents=True)
            (root / ui.CATALOG).mkdir(parents=True)
            (root / 'tools').mkdir()
            (root / ui.POLICY).write_text(json.dumps({
                'roles': {}, 'catalogs': {}, 'messages': {}}), encoding='utf-8')
            (root / ui.CLI / 'example.cpp').write_text(source, encoding='utf-8')
            with patch.object(ui, 'ROOT', root):
                report = ui.survey(argparse.Namespace(all=True, base=None, columns=60))
            self.assertEqual(len(report['ascii_findings']), 5)

    def test_failures_set_exit_status_even_with_zero_listing_limit(self):
        report = {'ascii_findings': [], 'findings': [], 'messages_examined': 1,
                  'translations_over_budget': 1, 'translations_excluded': 0, 'messages_excluded': 0}
        with patch.object(ui, 'survey', return_value=report), patch('builtins.print'):
            with patch('sys.argv', ['check_ui_text.py', '--limit', '0']):
                self.assertEqual(ui.main(), 1)
            report['translations_over_budget'] = 0
            with patch('sys.argv', ['check_ui_text.py', '--quiet']):
                self.assertEqual(ui.main(), 0)


if __name__ == '__main__':
    unittest.main()
