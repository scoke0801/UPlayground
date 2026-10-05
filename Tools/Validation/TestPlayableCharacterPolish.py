"""Failure/restore tests with real temporary files; no Unreal or project asset writes."""
import json
from pathlib import Path
import tempfile
import unittest
from PlayableCharacterTransaction import Transaction, restore, sha256, write_json


class TransactionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.content = self.root/'Content/Test'
        self.content.mkdir(parents=True)
        self.source = self.root/'polish.json'
        self.source.write_text('{"schema_version":1}')
        self.original = self.content/'Existing.uasset'
        self.original.write_bytes(b'original package')
        (self.content/'Existing.ubulk').write_bytes(b'original bulk')
        self.run = self.root/'Saved/Transaction'
        self.tx = Transaction(self.root, self.run)
        self.tx.prepare(['/Game/Test/Existing', '/Game/Test/New'], self.source)

    def tearDown(self):
        self.temp.cleanup()

    def test_partial_save_restores_existing_and_removes_only_new_files(self):
        self.original.write_bytes(b'partial save')
        (self.content/'Existing.ubulk').unlink()
        (self.content/'New.uasset').write_bytes(b'created')
        (self.content/'New.uexp').write_bytes(b'created sidecar')
        unrelated = self.content/'Unrelated.uasset'
        unrelated.write_bytes(b'do not touch')
        restore(self.root, self.run)
        self.assertEqual(self.original.read_bytes(), b'original package')
        self.assertEqual((self.content/'Existing.ubulk').read_bytes(), b'original bulk')
        self.assertFalse((self.content/'New.uasset').exists())
        self.assertFalse((self.content/'New.uexp').exists())
        self.assertEqual(unrelated.read_bytes(), b'do not touch')
        self.assertEqual(restore(self.root, self.run)['status'], 'RESTORED')

    def test_corrupt_backup_rejects_before_any_restore(self):
        self.original.write_bytes(b'current')
        (self.run/'backup/Content/Test/Existing.ubulk').write_bytes(b'corrupt')
        with self.assertRaises(ValueError): restore(self.root, self.run)
        self.assertEqual(self.original.read_bytes(), b'current')

    def test_missing_manifest_row_cannot_partially_restore(self):
        self.tx.data['files'].pop()
        self.tx.flush()
        self.original.write_bytes(b'current')
        with self.assertRaises(ValueError): restore(self.root, self.run)
        self.assertEqual(self.original.read_bytes(), b'current')

    def test_path_traversal_rejected(self):
        self.tx.data['files'][0]['relative'] = '../outside.uasset'
        self.tx.flush()
        with self.assertRaises(ValueError): restore(self.root, self.run)

    def test_unplanned_write_rejected(self):
        with self.assertRaises(ValueError): self.tx.mark_written('/Game/Test/Unrelated')

    def test_duplicate_transaction_rejected(self):
        with self.assertRaises(ValueError): self.tx.prepare(['/Game/Test/New'], self.source)

    def test_wrong_project_rejected(self):
        with self.assertRaises(ValueError): restore(self.root/'Other', self.run)

    def test_source_is_backed_up_before_mutation(self):
        self.source.write_bytes(b'changed')
        self.assertEqual(sha256(self.run/'polish-source.json'), self.tx.data['source_sha256'])


if __name__ == '__main__':
    unittest.main()
