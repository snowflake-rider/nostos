import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('check_repository', Path(__file__).resolve().parents[1] / 'check_repository.py')
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class RepositoryChecks(unittest.TestCase):
    def test_relative_and_escaped_paths(self):
        path = Path('/tmp/repo/docs/start.md')
        self.assertEqual(CHECK.local_target(path, '../code/a%20b.c#L2'), Path('/tmp/repo/code/a b.c').resolve())
        self.assertEqual(CHECK.local_target(path, '<../code/a b.c>'), Path('/tmp/repo/code/a b.c').resolve())

    def test_external_and_fragment_links(self):
        for link in ['https://example.com/page', 'mailto:person@example.com', '#heading', '//example.com/path']:
            self.assertIsNone(CHECK.local_target(Path('/tmp/start.md'), link))

    def test_detects_missing_and_ignores_generated_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'README.md').write_text('[missing](missing.c)\n```\n[x](ignored.c)\n```\n')
            (root / 'build').mkdir()
            (root / 'build' / 'generated.md').write_text('[bad](absent.c)')
            errors, documents, links = CHECK.check(root)
            self.assertEqual((len(errors), documents, links), (1, 1, 1))

    def test_allows_project_docs_but_rejects_build_inputs_under_docs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'firmware').mkdir()
            (root / 'docs').mkdir()
            (root / 'firmware' / 'README.md').write_text('notes')
            (root / 'firmware' / 'LICENSE.md').write_text('license')
            (root / 'docs' / 'main.c').write_text('int main(void) {}')
            self.assertEqual(len(CHECK.check(root)[0]), 1)

    def test_rejects_retired_source_trees(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('code', 'esp-ble-unorganized'):
                (root / name).mkdir()
            self.assertEqual(len(CHECK.check(root)[0]), 2)


if __name__ == '__main__':
    unittest.main()
