#!/usr/bin/env python3
"""Drop comment and blank lines from the embedded IANA tz sources in this directory.

date's parser (valhalla/src/baldr/tz_alt.cpp, init_tzdb_strings) ignores every line that is
empty or starts with '#', so removing them is byte-for-byte behaviour-neutral and cuts ~80%
of the .rodata these blobs occupy. Re-run after any tzdata bump.
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# date_time_windows_zones.h is XML, parsed by a different reader, and only linked on Windows.
FILES = [
    "africa", "antarctica", "asia", "australasia", "backward",
    "etcetera", "europe", "leapseconds", "northamerica", "southamerica",
]

ARRAY_RE = re.compile(r"const unsigned char (\w+)\[\] = \{(.*?)\};", re.S)
LEN_RE = re.compile(r"const size_t (\w+_len) = \d+;")


def decode(text):
    m = ARRAY_RE.search(text)
    if not m:
        raise SystemExit("no byte array found")
    return m.group(1), bytes(int(b, 16) for b in m.group(2).replace("\n", "").split(",") if b.strip())


def encode(name, blob):
    rows = ["#include <cstddef>", "", "const unsigned char %s[] = {" % name]
    body = ["0x%02x" % b for b in blob]
    rows += [", ".join(body[i:i + 16]) + ("," if i + 16 < len(body) else "};")
             for i in range(0, len(body), 16)]
    rows += ["", "const size_t %s_len = %d;" % (name, len(blob)), ""]
    return "\n".join(rows)


def main():
    total_before = total_after = 0
    for stem in FILES:
        path = HERE / ("date_time_%s.h" % stem)
        name, blob = decode(path.read_text())
        parsed = lambda b: [l for l in b.split(b"\n") if l and not l.startswith(b"#")]
        stripped = b"\n".join(parsed(blob)) + b"\n"

        path.write_text(encode(name, stripped))

        # round-trip: the parser's own filter must see exactly the same lines as before
        reread_name, reread = decode(path.read_text())
        if (reread_name, parsed(reread)) != (name, parsed(blob)):
            raise SystemExit("%s: strip changed what the parser sees" % path.name)

        total_before += len(blob)
        total_after += len(stripped)
        print("%-14s %7d -> %7d" % (stem, len(blob), len(stripped)))
    print("total %d -> %d (-%.0f%%)" %
          (total_before, total_after, 100 - 100.0 * total_after / total_before))


if __name__ == "__main__":
    sys.exit(main())
