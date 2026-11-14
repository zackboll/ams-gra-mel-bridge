"""Source-only annotation delta regressions in isolated temporary Git repos."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from check_ada_doc_annotations import AnnotationError, check


SCRIPT = Path(__file__).with_name('check_ada_doc_annotations.py').resolve()
FUNCTION = '''   function Read (Value : Natural) return Natural;
   --  Read an owned value.
   --  @param Value Input value.
   --  @return Owned result.
'''
RECORD = '''   type Counters (Enabled : Boolean := True) is record
      Received, Dropped : Natural := 0;
   end record;
   --  Owned counters.
   --  @field Enabled Whether counters are enabled.
   --  @field Received Received event count.
   --  @field Dropped Dropped event count.
'''

# Synthetic equivalent of PR #202's original public surface; deliberately prose
# only. This does not import or change that PR's branch or corrected source.
PR202 = '''package AMS.MEL.IR.Channel.Metadata is
   type Comms_Metadata_Stream is limited private;
   --  Bounded owner of common communications-test metadata.
   function Open
     (Channel : Channel_View;
      Queue_Capacity : Positive := 64) return Comms_Metadata_Stream;
   --  Register a bounded communications-test report receiver.
   function Receive
     (Stream : Comms_Metadata_Stream;
      Timeout_Milliseconds : Natural := 0) return Comms_Test_Report;
   --  Receive owned command and request correlation identifiers.
   type Metadata_Counters is record
      Events_Received : Natural;
      Events_Dropped : Natural;
   end record;
   --  Snapshot of bounded queue accounting.
   function Counters (Stream : Comms_Metadata_Stream) return Metadata_Counters;
   --  Snapshot report queue accounting without receiving an event.
   procedure Close (Stream : in out Comms_Metadata_Stream);
   --  Close the receiver explicitly.
private
   type Comms_Metadata_Stream is null record;
end AMS.MEL.IR.Channel.Metadata;
'''


class DeltaTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='ada-doc-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'ada/src/demo.ads'
        self.source.parent.mkdir(parents=True)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Annotation Tests')
        self.git('config', 'user.email', 'annotation-tests@example.invalid')
        self.write('')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.git('update-ref', 'refs/remotes/origin/main', self.base)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], text=True, stderr=subprocess.PIPE)

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')

    def write(self, body, private=''):
        self.source.write_text('package Demo is\n' + body + private + 'end Demo;\n', encoding='utf-8')

    def result(self):
        return check(self.root, self.base)

    def passes(self, inspected=1):
        count, diagnostics = self.result()
        self.assertEqual(diagnostics, [])
        self.assertEqual(count, inspected)

    def fails(self, *messages):
        count, diagnostics = self.result()
        self.assertGreater(count + len(diagnostics), 0)
        text = '\n'.join(diagnostics)
        for message in messages:
            self.assertIn(message, text)
        self.assertTrue(all(line.startswith('ada/src/') and ': GNATDOC: ' in line for line in diagnostics))
        return text

    def test_new_complete_function(self):
        self.write(FUNCTION)
        self.passes()

    def test_missing_return(self):
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.fails('ada/src/demo.ads:2: GNATDOC: Read missing @return')

    def test_missing_one_of_several_parameters(self):
        self.write(FUNCTION.replace('Value : Natural', 'Value, Other : Natural'))
        self.fails('Read missing @param Other')

    def test_wrong_parameter(self):
        self.write(FUNCTION.replace('@param Value', '@param Typo'))
        self.fails('unexpected @param Typo', 'missing @param Value')

    def test_duplicate_parameter_case_insensitive(self):
        self.write(FUNCTION + '   --  @param VALUE Another description.\n')
        self.fails('duplicate @param VALUE')

    def test_procedure_return_is_invalid(self):
        self.write(FUNCTION.replace('function Read', 'procedure Read').replace(' return Natural;', ';'))
        self.fails('Read unexpected @return')

    def test_no_parameter_function_requires_return(self):
        self.write('   function Ready return Boolean;\n   --  Query readiness.\n')
        self.fails('Ready missing @return')

    def test_no_parameter_procedure(self):
        self.write('   procedure Reset;\n   --  Reset state.\n')
        self.passes()

    def test_record_missing_field(self):
        self.write(RECORD.replace('   --  @field Received Received event count.\n', ''))
        self.fails('Counters missing @field Received')

    def test_grouped_components_and_discriminants(self):
        self.write(RECORD)
        self.passes()

    def test_grouped_multiline_components(self):
        self.write(RECORD.replace('Received, Dropped :', 'Received,\n      Dropped :'))
        self.passes()

    def test_missing_discriminant(self):
        self.write(RECORD.replace('   --  @field Enabled Whether counters are enabled.\n', ''))
        self.fails('missing @field Enabled')

    def test_duplicate_and_stale_fields(self):
        self.write(RECORD + '   --  @field Received Repeated count.\n   --  @field Old Stale count.\n')
        self.fails('duplicate @field Received', 'unexpected @field Old')

    def test_multiline_defaults_grouped_parameters(self):
        self.write('''   function Read
     (First, Second : Natural := Integer'Max (1, 2);
      Name : String := "default; -- value")
      return Natural;
   --  @param First Initial input.
   --  @param Second Other input.
   --  @param Name Display name.
   --  @return Owned result.
''')
        self.passes()

    def test_multiline_annotation_content(self):
        self.write(FUNCTION.replace('@param Value Input value.', '@param Value\n   --    Input value.'))
        self.passes()

    def test_compact_and_case_insensitive_profiles(self):
        self.write('   FUNCTION Read(Value:Natural) RETURN Natural;\n'
                   '   --  @param vAlUe Input value.\n   --  @return Owned value.\n')
        self.passes()

    def test_overriding_profile(self):
        self.write(FUNCTION.replace('function Read', 'overriding function Read')
                   .replace('   --  @return Owned result.\n', ''))
        self.fails('Read missing @return')

    def test_variant_record_fails_explicitly(self):
        self.write('''   type Variant (Flag : Boolean) is record
      case Flag is
         when True => Item : Natural;
         when False => null;
      end case;
   end record;
''')
        self.fails('Variant unsupported variant record')

    def test_multiple_components_on_line_fail_explicitly(self):
        self.write(RECORD.replace('Received, Dropped : Natural := 0;',
                                  'Received : Natural := 0; Dropped : Natural := 0;'))
        self.fails('unsupported multiple record components on one line')

    def test_overloads_are_independent(self):
        self.write(FUNCTION)
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.write(FUNCTION + FUNCTION.replace('Natural', 'Integer').replace('   --  @return Owned result.\n', ''))
        count, diagnostics = self.result()
        self.assertEqual(count, 1)
        self.assertEqual(len(diagnostics), 1)
        self.assertIn('demo.ads:6: GNATDOC: Read missing @return', diagnostics[0])

    def legacy_base(self):
        self.write('   function Legacy (Old : Natural) return Natural;\n   --  Historical prose.\n')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        return self.source.read_text()

    def test_unchanged_legacy_excluded(self):
        self.legacy_base()
        self.passes(0)

    def test_neighbor_edit_does_not_select_legacy(self):
        old = self.legacy_base()
        self.source.write_text(old.replace('end Demo;', FUNCTION + 'end Demo;'))
        self.passes()

    def test_other_declaration_and_package_comments_excluded(self):
        old = self.legacy_base()
        self.source.write_text(old.replace('package Demo is', 'package Demo is\n   --  Package prose changed.')
                               .replace('end Demo;', '   Count : Natural := 2;\nend Demo;'))
        self.passes()

    def test_documentation_only_change_selects(self):
        old = self.legacy_base()
        self.source.write_text(old.replace('Historical prose.', 'Updated prose.'))
        self.fails('Legacy missing @param Old', 'Legacy missing @return')

    def test_formatting_only_does_not_select(self):
        old = self.legacy_base()
        self.source.write_text(old.replace('function Legacy (Old : Natural) return Natural;',
                                          'FUNCTION Legacy\n     (Old:Natural)\n      return Natural;')
                               .replace('--  Historical prose.', '--    Historical\n   --    prose.'))
        self.passes(0)

    def test_documentation_reflow_does_not_select(self):
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.source.write_text(self.source.read_text().replace('@param Value Input value.',
                                                             '@param Value Input\n   --    value.'))
        self.passes(0)

    def test_annotation_spacing_does_not_select(self):
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.source.write_text(self.source.read_text().replace('@param Value Input value.',
                                                             '@param    Value   Input  value.'))
        self.passes(0)

    def test_modified_profile(self):
        self.write(FUNCTION)
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.write(FUNCTION.replace('Value : Natural', 'Value, Added : Natural'))
        self.fails('Read missing @param Added')

    def test_modified_default_literal_selects(self):
        self.write('   function Read (Name : String := "one") return Natural;\n')
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.source.write_text(self.source.read_text().replace('"one"', '"two"'))
        self.fails('Read missing @param Name', 'Read missing @return')

    def test_modified_component_type_selects_record(self):
        self.write(RECORD.replace('   --  @field Received Received event count.\n', ''))
        self.commit()
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.source.write_text(self.source.read_text().replace('Received, Dropped : Natural', 'Received, Dropped : Integer'))
        self.fails('Counters missing @field Received')

    def test_private_part_excluded_even_if_unsupported(self):
        self.write(FUNCTION, 'private\n   function "unknown" return Boolean;\n   type Hidden is null record;\n')
        self.passes()

    def test_private_unit_excluded(self):
        self.source.write_text('private package Demo is\n   function Hidden return Boolean;\nend Demo;\n')
        self.passes(0)

    def test_private_change_does_not_select_legacy(self):
        old = self.legacy_base()
        self.source.write_text(old.replace('end Demo;', 'private\n   function Hidden return Boolean;\nend Demo;'))
        self.passes(0)

    def test_new_untracked_ads(self):
        self.source = self.source.with_name('new.ads')
        self.write(FUNCTION)
        self.passes()

    def test_staged_unstaged_and_committed(self):
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.commit()
        self.write(self.source.read_text().split('package Demo is\n')[1].rsplit('end Demo;', 1)[0]
                   + '   procedure Staged (X : Natural);\n')
        self.git('add', '.')
        self.source.write_text(self.source.read_text().replace('end Demo;', '   function Unstaged return Boolean;\nend Demo;'))
        self.fails('Read missing @return', 'Staged missing @param X', 'Unstaged missing @return')
        self.assertEqual(self.result()[0], 3)

    def test_missing_base(self):
        with self.assertRaisesRegex(AnnotationError, 'base revision.*unavailable'):
            check(self.root, 'missing-base')

    def test_unsupported_operator(self):
        self.write('   function "+" (L, R : Natural) return Natural;\n   --  Add values.\n')
        self.fails('function "+"', 'unsupported public declaration')

    def test_ambiguous_duplicate(self):
        self.write(FUNCTION + FUNCTION)
        self.fails('Read ambiguous public declaration identity')

    def test_unterminated_profile(self):
        self.write('   function Broken (Value : Natural\n')
        self.fails('cannot safely inspect changed public source', 'line 2', 'did not terminate')

    def test_multiple_declarations_on_line_fail_closed(self):
        self.write('   function First return Boolean; function Second return Boolean;\n   --  @return Result value.\n')
        self.fails('unsupported multiple declarations on one line')

    def test_generic_instantiation_fails_closed(self):
        self.write('   function Read is new Generic_Read (Natural);\n')
        self.fails('Read unsupported generic subprogram instantiation')

    def test_inline_package_declaration_fails_closed(self):
        self.source.write_text('package Demo is function Inline return Boolean;\nend Demo;\n')
        self.fails('unsupported code on package header line')

    def test_nested_package_fails_closed(self):
        self.write('   package Nested is\n      function Hidden return Boolean;\n   end Nested;\n')
        self.fails('Nested unsupported nested public package')

    def test_private_record_completion_does_not_require_fields(self):
        self.write('   type Owner is private;\n',
                   'private\n   type Owner is record\n      Hidden : Natural;\n   end record;\n')
        self.passes()

    def test_new_committed_file(self):
        self.source = self.source.with_name('new.ads')
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.commit()
        self.fails('Read missing @return')

    def test_new_staged_file(self):
        self.source = self.source.with_name('new.ads')
        self.write(FUNCTION.replace('   --  @return Owned result.\n', ''))
        self.git('add', '.')
        self.fails('Read missing @return')

    def test_no_borrowed_leading_or_separated_comments(self):
        self.write('   --  @return Unrelated package description.\n   function Read return Natural;\n\n'
                   '   --  @return Separated description.\n')
        self.fails('Read missing @return')

    def test_no_borrowing_next_declaration_annotations(self):
        self.write('   function Legacy return Natural;\n' + FUNCTION)
        text = self.fails('Legacy missing @return')
        self.assertNotIn('Read missing', text)

    def test_empty_placeholder_and_duplicate_returns(self):
        for description in ('', 'TODO', 'TBD.', 'FIXME: explain', '...'):
            with self.subTest(description=description):
                self.write(FUNCTION.replace('Owned result.', description))
                self.fails('empty or placeholder @return')
        self.write(FUNCTION + '   --  @return Another result.\n')
        self.fails('duplicate @return')

    def test_empty_field_and_parameter(self):
        self.write(FUNCTION.replace('Input value.', '') + RECORD.replace('Received event count.', 'TODO'))
        self.fails('empty or placeholder @param Value', 'empty or placeholder @field Received')

    def test_optional_enum_formal_exception_not_required(self):
        self.write('   type Mode is (First, Second);\n   --  @enum First First mode.\n'
                   '   Failure : exception;\n' + FUNCTION + '   --  @exception Failure Operation failed.\n'
                   '   --  @formal Optional Existing formal annotation.\n')
        self.passes(3)

    def test_malformed_and_misplaced_tags(self):
        self.write(FUNCTION + '   --  @field Wrong Misplaced field.\n   --  @params Value Typo tag.\n')
        self.fails('unexpected @field Wrong', 'malformed or unsupported structured tag @params')

    def test_pr202_prose_does_not_satisfy_structured_contract(self):
        self.source = self.source.with_name('ams-mel-ir-channel-metadata.ads')
        self.source.write_text(PR202)
        self.fails('Open missing @param Channel', 'Open missing @param Queue_Capacity',
                   'Open missing @return', 'Receive missing @param Stream',
                   'Receive missing @param Timeout_Milliseconds', 'Receive missing @return',
                   'Metadata_Counters missing @field Events_Received', 'Metadata_Counters missing @field Events_Dropped',
                   'Counters missing @return', 'Close missing @param Stream')

    def test_deleted_file(self):
        self.source.unlink()
        self.passes(0)

    def test_cli_default_and_missing_base(self):
        result = subprocess.run([sys.executable, str(SCRIPT)], cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Public declarations inspected: 0', result.stdout)
        self.assertIn('PASS', result.stdout)
        self.write('   function Ready return Boolean;\n')
        result = subprocess.run([sys.executable, str(SCRIPT), '--base', self.base],
                                cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Ready missing @return', result.stdout)
        result = subprocess.run([sys.executable, str(SCRIPT), '--base', 'unavailable'],
                                cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('unavailable', result.stderr)


if __name__ == '__main__':
    unittest.main()
