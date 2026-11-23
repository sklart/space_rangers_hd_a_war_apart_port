"""Inspect local copies of binary script definitions without changing resources."""
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit('expected path to isolated script copy')
data = Path(sys.argv[1]).read_bytes()
needle = 'quest_item'.encode('utf-16le')
offset = 0
while True:
    position = data.find(needle, offset)
    if position < 0:
        break
    start = max(0, position - 400)
    start += (position - start) % 2
    end = min(len(data), position + 400)
    fragment = data[start:end].decode('utf-16le', errors='replace').replace('\0', '|')
    print('\n---', position, '---\n', fragment)
    offset = position + len(needle)
