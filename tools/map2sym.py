#!/usr/bin/env python3
"""map2sym.py - turn a gspasm .map into a symbol file for Lua mods (roadmap 1.2).

    map2sym.py <file.map> <game_id> <program.bin | prog.lo prog.hi> <mods/sym folder>

Writes <game_id>_<crc>.lua, where <crc> is the CRC-32 of the program ROM as the program
loads it (16-bit words, low byte first: lo and hi interleaved). The app picks the file by
that CRC, so a symbol file is only ever used on the exact revision it was made from.
The file holds names and addresses only (the global symbols of the source): no code and
nothing from the ROM itself.
"""
import os
import re
import sys
import zlib


def main(argv):
    if len(argv) not in (5, 6):
        sys.exit(__doc__)
    mapfile, game, out = argv[1], argv[2], argv[-1]
    if len(argv) == 6:
        lo, hi = open(argv[3], 'rb').read(), open(argv[4], 'rb').read()
        prog = bytearray(len(lo) * 2)
        prog[0::2] = lo
        prog[1::2] = hi
    else:
        prog = open(argv[3], 'rb').read()
    crc = zlib.crc32(prog) & 0xffffffff
    syms = {}
    for line in open(mapfile, encoding='latin-1'):
        f = line.split()
        # section lines have four fields (name, object, base, size); symbols two
        if len(f) == 2 and re.fullmatch(r'[0-9A-Fa-f]{8}', f[1]):
            syms[f[0]] = int(f[1], 16)
    ident = re.compile(r'[A-Za-z_][A-Za-z0-9_]*$')
    keywords = {'and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for', 'function', 'goto', 'if', 'in',
                'local', 'nil', 'not', 'or', 'repeat', 'return', 'then', 'true', 'until', 'while'}
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, '%s_%08x.lua' % (game, crc))
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('-- %s, program ROM CRC-32 %08x: %d global symbols of the source, from %s by tools/map2sym.py.\n'
                % (game, crc, len(syms), os.path.basename(mapfile)))
        f.write('-- Bit addresses, as the TMS34010 sees them. nba.sym("name") reads this file.\nreturn {\n')
        for k in sorted(syms, key=str.lower):
            key = k if ident.match(k) and k not in keywords else '["%s"]' % k.replace('\\', '\\\\').replace('"', '\\"')
            f.write('  %s = 0x%08X,\n' % (key, syms[k]))
        f.write('}\n')
    print('%s: %d symbols' % (path, len(syms)))


if __name__ == '__main__':
    main(sys.argv)
