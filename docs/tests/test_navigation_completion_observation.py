"""IR-CI-003 controls compile the real C log observer, not a Python imitation."""

import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NavigationObservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix="navigation-observer-")
        cls.addClassCleanup(cls.workspace.cleanup)
        directory = Path(cls.workspace.name)
        source = ROOT / "native/tests/test_ir_navigation.c"
        text = source.read_text()
        function = text.split("static int test_request_close_while_pending(void)\n", 1)[1]
        assertions = function.split('    CHECK(strstr(log, "navigation_sent") != NULL);', 1)[1]
        assertions = ('    CHECK(strstr(log, "navigation_sent") != NULL);' +
                      assertions.split('    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG")', 1)[0])
        # Keep the actual four presence and three order assertions in this harness.
        if assertions.count("CHECK(") != 7:
            raise RuntimeError("pending-close lifecycle assertions changed")
        harness = directory / "observer.c"
        harness.write_text(
            '#define main retained_navigation_contract_main\n'
            f'#include "{source}"\n'
            '#undef main\n'
            'int main(int argc, char **argv) {\n'
            '    char log[4096];\n'
            '    CHECK(argc == 3);\n'
            '    puts("OBSERVER_READY"); fflush(stdout);\n'
            '    if (strcmp(argv[1], "read") == 0)\n'
            '        return read_log(argv[2], log, sizeof log);\n'
            '    CHECK(wait_for_log_marker(argv[2], "library_unloaded", log, sizeof log) == EXIT_SUCCESS);\n'
            + assertions +
            '    puts("LIFECYCLE_QUALIFIED");\n'
            '    return EXIT_SUCCESS;\n}\n')
        cls.executable = directory / "observer"
        compiler = os.environ.get("CC", "cc")
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-pthread",
                        '-DAMS_MEL_TEST_MOCK_PROVIDER="unused-helper-only"',
                        "-I" + str(ROOT / "native/include"), str(harness), "-o", str(cls.executable)],
                       check=True, capture_output=True, text=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="navigation-log-")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "lifetime.log"
        self.path.write_bytes(b"")
        self.complete = b"navigation_sent\nnavigation_completed\nchannel_destroyed\nlibrary_unloaded\n"

    def start(self, mode="wait"):
        process = subprocess.Popen([str(self.executable), mode, str(self.path)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        self.assertEqual(process.stdout.readline(), "OBSERVER_READY\n")
        return process

    def finish(self, process, expected, diagnostic=None):
        stdout, stderr = process.communicate(timeout=8)
        self.assertEqual(process.returncode, expected, stdout + stderr)
        if diagnostic:
            self.assertIn(diagnostic, stderr)
        if expected:
            self.assertNotIn("LIFECYCLE_QUALIFIED", stdout)
            self.assertTrue(self.path.exists(), "failure diagnostics log was removed")
        else:
            self.assertIn("LIFECYCLE_QUALIFIED", stdout)
        return stderr

    def test_normal_delayed_observation(self):
        process = self.start()
        self.path.write_bytes(self.complete)
        self.finish(process, 0)

    def test_completion_after_old_window(self):
        self.path.write_bytes(b"navigation_sent\n")
        process = self.start()
        time.sleep(.35)
        self.assertIsNone(process.poll(), "qualified before deferred cleanup completed")
        with self.path.open("ab") as log:
            log.write(self.complete.split(b"\n", 1)[1])
        self.finish(process, 0)

    def test_missing_completion_fails_with_final_marker(self):
        self.path.write_bytes(self.complete.replace(b"navigation_completed\n", b""))
        self.finish(self.start(), 1, 'strstr(log, "navigation_completed") != NULL')

    def test_missing_channel_destruction_fails(self):
        self.path.write_bytes(self.complete.replace(b"channel_destroyed\n", b""))
        self.finish(self.start(), 1, 'strstr(log, "channel_destroyed") != NULL')

    def test_missing_unload_times_out_and_retains_contents(self):
        data = self.complete.replace(b"library_unloaded\n", b"")
        self.path.write_bytes(data)
        started = time.monotonic()
        stderr = self.finish(self.start(), 1, "Timed out waiting for library_unloaded; retained log")
        self.assertGreaterEqual(time.monotonic() - started, 5)
        self.assertIn(str(self.path), stderr)
        self.assertIn(data.decode(), stderr)
        self.assertEqual(self.path.read_bytes(), data)

    def test_permanently_absent_completion_times_out(self):
        self.path.write_bytes(b"navigation_sent\n")
        stderr = self.finish(self.start(), 1, "Timed out waiting for library_unloaded")
        self.assertIn("navigation_sent", stderr)

    def test_each_incorrect_order_fails(self):
        lines = self.complete.splitlines(keepends=True)
        for index in range(3):
            with self.subTest(pair=index):
                wrong = list(lines)
                wrong[index], wrong[index + 1] = wrong[index + 1], wrong[index]
                self.path.write_bytes(b"".join(wrong))
                self.finish(self.start(), 1, "check_order(log,")

    def test_incomplete_final_record_does_not_qualify(self):
        self.path.write_bytes(self.complete[:-1])
        process = self.start()
        time.sleep(.25)
        self.assertIsNone(process.poll())
        with self.path.open("ab") as log:
            log.write(b"\n")
        self.finish(process, 0)

    def test_substring_cannot_manufacture_final_marker(self):
        self.path.write_bytes(self.complete.replace(b"library_unloaded", b"not_library_unloaded"))
        self.finish(self.start(), 1, "Timed out waiting for library_unloaded")

    def test_embedded_nul_rejected(self):
        self.path.write_bytes(self.complete + b"\0hidden\n")
        self.finish(self.start(), 1, "Malformed or truncated lifetime log")

    def test_truncated_input_rejected_even_with_all_markers(self):
        self.path.write_bytes(self.complete + b"x" * 4096)
        self.finish(self.start(), 1, "Malformed or truncated lifetime log")

    def test_torn_marker_after_snapshot_eof_is_reobserved(self):
        # Deterministically append after fread's EOF snapshot but before any
        # overflow probe. A short snapshot must not consume one appended byte
        # and incorrectly declare that valid live log oversized.
        source = Path(self.workspace.name) / "torn.c"
        source.write_text(
            '#define main retained_navigation_contract_main\n'
            '#define fread controlled_fread\n'
            f'#include "{ROOT / "native/tests/test_ir_navigation.c"}"\n'
            '#undef fread\n#undef main\n'
            'extern size_t fread(void *, size_t, size_t, FILE *);\n'
            'static unsigned reads;\n'
            'size_t controlled_fread(void *p, size_t size, size_t count, FILE *file) {\n'
            '    char link[64], path[4096];\n'
            '    size_t result = fread(p, size, count, file);\n'
            '    if (++reads == 1U) {\n'
            '        snprintf(link, sizeof link, "/proc/self/fd/%d", fileno(file));\n'
            '        ssize_t n = readlink(link, path, sizeof path - 1U);\n'
            '        if (n <= 0) abort();\n        path[n] = 0;\n'
            '        FILE *writer = fopen(path, "ab");\n'
            '        if (!writer) abort();\n        fputs("loaded\\n", writer); fclose(writer);\n'
            '    }\n'
            '    return result;\n}\n'
            'int main(int argc, char **argv) {\n'
            '    char log[4096]; CHECK(argc == 2);\n'
            '    return wait_for_log_marker(argv[1], "library_unloaded", log, sizeof log);\n}\n')
        # This wrapper appends after the first short read, completing a torn event.
        executable = source.with_suffix("")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-ffunction-sections", "-fdata-sections",
                        "-Wl,--gc-sections", "-pthread", '-DAMS_MEL_TEST_MOCK_PROVIDER="unused"',
                        "-I" + str(ROOT / "native/include"), str(source), "-o", str(executable)],
                       check=True, capture_output=True)
        self.path.write_bytes(self.complete.replace(b"library_unloaded\n", b"library_un"))
        subprocess.run([str(executable), str(self.path)], check=True, capture_output=True, timeout=8)


if __name__ == "__main__":
    unittest.main()
