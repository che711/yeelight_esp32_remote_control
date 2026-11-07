"""Checks that need neither the board nor the lamp. They run in GitHub Actions and by hand:
    python3 tests/check_static.py
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors = []


def fail(text):
    errors.append(text)
    print("ERROR:", text)


main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
page_h = (ROOT / "src" / "index_html.h").read_text(encoding="utf-8")

# 1. The list of effects in the firmware and on the page is the same
firmware = set(re.findall(r'^\s*\{"(\w+)",\s+FLOW_', main_cpp, re.M)) | {"on", "wave", "fire"}
page = set(re.findall(r"board:\s*'(\w+)'", page_h))
if firmware != page:
    fail("the effect lists differ: only in the firmware %s, only on the page %s"
         % (sorted(firmware - page), sorted(page - firmware)))

# 2. The page is one R"HTML(...)HTML" string: the string terminator appears exactly once
if page_h.count('R"HTML(') != 1:
    fail('index_html.h must have exactly one R"HTML( start')
if page_h.count(')HTML"') != 1:
    fail(')HTML" appeared in the page text: it ends the string too early')

# 3. The page script parses without errors
m = re.search(r"<script>(.*?)</script>", page_h, re.S)
if not m:
    fail("index_html.h has no <script> tag")
elif not shutil.which("node"):
    print("node was not found, the script syntax was not checked")
else:
    with tempfile.TemporaryDirectory() as tmp:
        js = Path(tmp) / "page.js"
        js.write_text(m.group(1), encoding="utf-8")
        r = subprocess.run(["node", "--check", str(js)], capture_output=True, text=True)
        if r.returncode:
            fail("the page script has an error:\n" + r.stderr)

# 4. Every effect in the firmware table has a steps array
for name in re.findall(r'^\s*\{"(\w+)",\s+(FLOW_\w+),', main_cpp, re.M):
    if not re.search(r"const FlowStep %s\[\]" % name[1], main_cpp):
        fail("the steps array %s was not found for the effect %s" % (name[1], name[0]))

if errors:
    sys.exit(1)
print("ok: %d effects, the page and its script are fine" % len(page))
