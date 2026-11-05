"""The rules check_routes.py holds a row to: python3 -m unittest test_check_routes, from tools/."""
import os
import sys
import tempfile
import unittest
from pathlib import Path

from check_routes import CRASH_RETRIES, SIZED, compare_cores_rooms, run_harness, sized_rules, wall_rules

FAR = ["--language", "fr", "--view", "far", "--screen", "1920x1080"]


class WallRules(unittest.TestCase):
    MEASURED = {"transitions.wall_arrivals": "0"}

    def test_a_row_that_measures_it_holds_it(self):
        self.assertEqual(wall_rules(self.MEASURED, ["transitions.taken=3"], "sameboy"), ["transitions.wall_arrivals=0"])

    def test_a_row_that_does_not_holds_none(self):
        self.assertEqual(wall_rules({}, [], "sameboy"), [])

    def test_its_own_rule_replaces_it_on_the_cores_it_names(self):
        self.assertEqual(wall_rules(self.MEASURED, ["transitions.wall_arrivals<=1"], "mgba"), [])
        self.assertEqual(wall_rules(self.MEASURED, ["mgba:transitions.wall_arrivals<=1"], "mgba"), [])
        self.assertEqual(wall_rules(self.MEASURED, ["mgba:transitions.wall_arrivals<=1"], "sameboy"), ["transitions.wall_arrivals=0"])


class SizedRules(unittest.TestCase):
    def test_a_row_of_the_normal_size_holds_none(self):
        self.assertEqual(sized_rules("enhanced", ["--language", "fr"], [], "sameboy"), [])

    def test_a_sized_row_holds_them_all(self):
        self.assertEqual(sized_rules("enhanced", FAR, ["language.code=fr"], "mgba"), SIZED["enhanced"])

    def test_its_own_ceiling_replaces_the_rule_on_every_core(self):
        for core in ("sameboy", "mgba"):
            self.assertEqual(sized_rules("enhanced", FAR, ["enhanced.ghost_wrong_room<=11"], core), ["enhanced.cold_fill_frames>=0"])

    def test_a_ceiling_for_one_core_leaves_the_rule_on_the_other(self):
        expect = ["sameboy:enhanced.ghost_wrong_room<=11"]
        self.assertEqual(sized_rules("enhanced", FAR, expect, "sameboy"), ["enhanced.cold_fill_frames>=0"])
        self.assertEqual(sized_rules("enhanced", FAR, expect, "mgba"), SIZED["enhanced"])

    def test_only_a_ceiling_replaces_it(self):
        for rule in ("enhanced.ghost_wrong_room>=0", "enhanced.ghost_wrong_room=3", "mgba:enhanced.ghost_wrong_room>=0"):
            self.assertEqual(sized_rules("enhanced", FAR, [rule], "mgba"), SIZED["enhanced"])

    def test_a_threaded_row_has_no_sized_rules(self):
        self.assertEqual(sized_rules("enhanced-threaded", FAR, [], "sameboy"), [])


class CoresRooms(unittest.TestCase):
    FRAMES = "0 0 0 0 0\n10 0 1 64 80\n20 0 2 64 80\n"

    def compare(self, reference: str, positions: str, reference_exit: int = 0, row_core: str = "-"):
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d) / "positions-sameboy.tsv", Path(d) / "positions.tsv"
            a.write_text(reference)
            b.write_text(positions)
            values = {}
            return compare_cores_rooms(a, b, reference_exit, row_core, values), values

    def test_the_same_rooms_hold(self):
        self.assertEqual(self.compare(self.FRAMES, self.FRAMES), ([], {"cores.rooms_same": "2", "cores.parted": "0"}))

    def test_a_half_written_line_fails_the_row(self):
        violations, values = self.compare(self.FRAMES + "30 0", self.FRAMES)
        self.assertEqual(len(violations), 1)
        self.assertIn("line 4", violations[0])
        self.assertEqual(values, {})

    def test_a_reference_killed_by_a_signal_fails_the_row(self):
        violations, values = self.compare(self.FRAMES, self.FRAMES, reference_exit=-11)
        self.assertEqual(len(violations), 1)
        self.assertIn("signal 11", violations[0])
        self.assertEqual(values, {})

    def test_a_row_of_both_cores_must_not_part(self):
        violations, values = self.compare(self.FRAMES, self.FRAMES.replace("20 0 2", "20 0 3"))
        self.assertEqual(len(violations), 1)
        self.assertEqual(values["cores.parted"], "1")
        self.assertEqual(self.compare(self.FRAMES, self.FRAMES.replace("20 0 2", "20 0 3"), row_core="mgba")[0], [])


@unittest.skipIf(os.name == "nt", "no signal kills a process on Windows")
class Crashes(unittest.TestCase):
    # A harness that dies by SIGSEGV on its first `deaths` runs, then exits 0.
    SCRIPT = ("import os, signal, sys\n"
              "count = sys.argv[1]\n"
              "n = int(open(count).read()) if os.path.exists(count) else 0\n"
              "open(count, 'w').write(str(n + 1))\n"
              "print('run', n + 1, flush=True)\n"
              "if n < int(sys.argv[2]): os.kill(os.getpid(), signal.SIGSEGV)\n")

    def run_dying(self, deaths: int):
        with tempfile.TemporaryDirectory() as d:
            script = Path(d) / "harness.py"
            script.write_text(self.SCRIPT)
            log = Path(d) / "harness.log"
            code, note = run_harness([sys.executable, str(script), str(Path(d) / "count"), str(deaths)], log)
            return code, note, sorted(p.name for p in Path(d).glob("harness*.log")), log.read_text()

    def test_a_run_that_lives_says_nothing(self):
        self.assertEqual(self.run_dying(0), (0, "", ["harness.log"], "run 1\n"))

    def test_a_run_killed_once_is_run_again(self):
        code, note, logs, text = self.run_dying(1)
        self.assertEqual((code, logs, text), (0, ["harness.crashed.log", "harness.log"], "run 2\n"))
        self.assertIn("signal 11", note)

    def test_a_run_killed_every_time_fails(self):
        code, note, logs, text = self.run_dying(CRASH_RETRIES + 1)
        self.assertEqual((code, logs, text), (-11, ["harness.crashed-2.log", "harness.crashed.log", "harness.log"], f"run {CRASH_RETRIES + 1}\n"))
        self.assertIn("signal 11, 11, 11", note)


if __name__ == "__main__":
    unittest.main()
