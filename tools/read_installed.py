"""Read-only inspection of unencrypted Enfusion PAC1 archives; never changes game data.

Only ordinary compression is supported. Encrypted/protected entries are refused.
This is inspection, not a replacement for Workbench resource validation.
"""
from __future__ import annotations
import argparse
import json
import struct
import zlib
from pathlib import Path


class Pak:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}
        with self.path.open('rb') as stream:
            head = stream.read(12)
            if head[:4] != b'FORM' or head[8:] != b'PAC1':
                raise ValueError(f'Unsupported archive: {self.path}')
            offset = 12
            while offset + 8 <= self.path.stat().st_size:
                stream.seek(offset)
                kind, size = struct.unpack('>4sI', stream.read(8))
                if kind == b'FILE':
                    data = stream.read(size)
                    break
                offset += 8 + size
            else:
                raise ValueError('No FILE index')
        cursor = 0

        def node(parent):
            nonlocal cursor
            kind, length = data[cursor:cursor + 2]
            cursor += 2
            name = data[cursor:cursor + length].decode('utf-8')
            cursor += length
            path = f'{parent}/{name}'.strip('/')
            if kind == 0:
                count = struct.unpack_from('<I', data, cursor)[0]
                cursor += 4
                for _ in range(count):
                    node(path)
            elif kind == 1:
                start, stored, original = struct.unpack_from('<III', data, cursor)
                flags = data[cursor + 12:cursor + 20]
                cursor += 24
                if start + stored > self.path.stat().st_size:
                    raise ValueError('Entry outside archive')
                self.entries[path] = (start, stored, original, flags)
            else:
                raise ValueError(f'Unsupported index type {kind}')
        node('')
        if cursor != len(data):
            raise ValueError(f'Index length mismatch {cursor}/{len(data)}')

    def read(self, name):
        start, stored, original, flags = self.entries[name]
        if any(flags[:6]):
            raise ValueError('Unsupported/protected entry flags')
        with self.path.open('rb') as stream:
            stream.seek(start)
            data = stream.read(stored)
        if stored != original:
            data = zlib.decompress(data)
        if len(data) != original:
            raise ValueError('Entry size mismatch')
        return data


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pak', type=Path)
    parser.add_argument('--match', default='')
    parser.add_argument('--read')
    args = parser.parse_args()
    archive = Pak(args.pak)
    if args.read:
        print(archive.read(args.read).decode('utf-8-sig'))
    else:
        print(json.dumps([name for name in archive.entries if args.match.lower() in name.lower()], indent=2))
