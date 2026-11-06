#!/usr/bin/env python3
"""Checks every pattern in patterns/ and the stitch counts of the keychain patterns.

Run:  python tools/test_patterns.py
"""
import glob, os, sys, unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import send_pattern as sp  # noqa: E402

PATTERNS = os.path.join(HERE, "..", "patterns")


def load(name):
    steps, errors = sp.parse(os.path.join(PATTERNS, name))
    assert not errors, errors
    return steps


def total(steps, op):
    return sum(n for o, n, _ in steps if o == op)


class AllPatterns(unittest.TestCase):
    def test_every_pattern_is_valid(self):
        files = sorted(glob.glob(os.path.join(PATTERNS, "*.crochet")))
        self.assertGreaterEqual(len(files), 7)
        for f in files:
            steps, errors = sp.parse(f)
            self.assertEqual(errors, [], f)
            self.assertTrue(0 < len(steps) <= sp.MAX_STEPS, f)

    def test_bad_input_is_rejected(self):
        import tempfile
        for text in ("CH x\n", "FOO 3\n", "REPEAT 2\nCH 1\n", "END\n"):
            with tempfile.NamedTemporaryFile("w", suffix=".crochet", delete=False) as t:
                t.write(text)
            _, errors = sp.parse(t.name)
            os.unlink(t.name)
            self.assertTrue(errors, text)


class Keychains(unittest.TestCase):
    def test_bow(self):
        s = load("bow-keychain.crochet")
        self.assertEqual(s[0][:2], ("CH", 56))
        self.assertEqual(total(s, "HDC"), 4 * (6 + 12 + 12 + 6))
        self.assertEqual(total(s, "SLST"), 4 * 18)
        self.assertEqual(sum(1 for o, _, _ in s if o == "TURN"), 4)

    def test_star(self):
        s = load("star-keychain.crochet")
        self.assertEqual(sum(1 for o, _, _ in s if o == "MR"), 2)        # two pieces
        self.assertEqual(total(s, "INC"), 2 * (5 + 5 + 5))               # R2 to R4
        self.assertEqual(total(s, "SLST"), 2 * (3 * 4 + 3))              # R5
        self.assertEqual(total(s, "DC"), 2 * (2 + 4))                    # first point + 4 points
        pauses = [a for o, _, a in s if o == "PAUSE"]
        self.assertEqual(len(pauses), 2 + 3)                             # per piece + finishing

    def test_star_round_stitch_counts(self):
        # Rounds R1 to R4 must grow 5 -> 10 -> 15 -> 20 stitches.
        s = load("star-keychain.crochet")
        piece = s[:s.index(("PAUSE", 0, "Fasten off this star piece"))]
        used = lambda seg: sum(n for o, n, _ in seg if o in ("SC", "INC"))
        made = lambda seg: sum(n for o, n, _ in seg if o == "SC") + 2 * sum(n for o, n, _ in seg if o == "INC")
        r2, r3, r4 = piece[1:6], piece[6:16], piece[16:31]
        self.assertEqual((used(r2), made(r2)), (5, 10))
        self.assertEqual((used(r3), made(r3)), (10, 15))
        self.assertEqual((used(r4), made(r4)), (15, 20))
        # R5 (as read in the pattern file): 1 first-point stitch + 4 point stitches + 15 slip stitches = 20
        self.assertEqual(total(piece, "SLST") + 1 + 4, 20)

if __name__ == "__main__":
    unittest.main(verbosity=2)
