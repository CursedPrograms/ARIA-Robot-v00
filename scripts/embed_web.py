"""Build a sketch's web/ folder into web_files.h, so the pages stay normal HTML, CSS and JS files.

    python embed_web.py aria_flight
"""
import pathlib
import sys

HEADER = "// Made by scripts/embed_web.py from web/: edit those, not this.\n#pragma once\n\n"


def main():
    sketch = pathlib.Path(__file__).parent / sys.argv[1]
    out = HEADER
    for f in sorted((sketch / "web").iterdir()):
        name = "WEB_" + f.name.replace(".", "_").upper()
        out += f'static const char {name}[] = R"WEBFILE({f.read_text(encoding="utf-8")})WEBFILE";\n\n'
    (sketch / "web_files.h").write_text(out, encoding="utf-8", newline="\n")
    print("made", sketch / "web_files.h")


main()
