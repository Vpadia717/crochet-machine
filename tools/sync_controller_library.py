#!/usr/bin/env python3
"""Copy every pattern in patterns/ into the template list of the controller page.

Run this after adding or editing a pattern:  python tools/sync_controller_library.py
The template name comes from the '# Name:' line at the top of each .crochet file.
"""
import glob, json, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PAGE = os.path.join(ROOT, "controller", "crochet_controller.html")

lib = {}
for path in sorted(glob.glob(os.path.join(ROOT, "patterns", "*.crochet"))):
    text = open(path, encoding="utf-8").read().strip("\n")
    m = re.search(r"^#\s*Name:\s*(.+)$", text, re.M)
    name = m.group(1).strip() if m else os.path.basename(path)[:-len(".crochet")]
    lib[name] = text

html = open(PAGE, encoding="utf-8").read()
block = "/*LIB_START*/const LIB=" + json.dumps(lib, indent=1, ensure_ascii=False).replace("</", "<\\/") + ";/*LIB_END*/"
new, n = re.subn(r"/\*LIB_START\*/.*?/\*LIB_END\*/", lambda _: block, html, flags=re.S)
if n != 1:
    sys.exit("Markers /*LIB_START*/ and /*LIB_END*/ not found in the controller page")
open(PAGE, "w", encoding="utf-8").write(new)
print(f"Updated {len(lib)} templates: " + ", ".join(lib))
