#!/usr/bin/env python3
"""sym_carry.py - carry a symbol file to a revision without source, by opcode pattern (roadmap 2.3).

    sym_carry.py <from.lua> <from.lo> <from.hi> <to.lo> <to.hi> <game_id> <out folder>
                 [--agree <from2.lua> <from2.lo> <from2.hi>] [--check <to.lua>]

For every name of <from.lua> (a symbol file of a revision built from source), the instructions
of the <from> program that use the name's address (as a 32-bit operand) are looked up in the
<to> program, with every address-like operand in them taken as "any word". A name is carried
when at least two such places are found exactly once each and all give the same new address,
or, for a code label, when the code at the label is found exactly once (and no use of the name
points elsewhere). A RAM name between two carried neighbours that moved by the same amount
moves by it too, when the target program uses that address (the variables keep their layout). Each entry gets the
words that prove it: { addr = new address, pattern = { words, -1 = any }, at = where }, which
the program checks against the ROM before it uses the name (mods.cpp).

--agree: carry from a second source revision too and keep only the names both give the same
address.
--check <to.lua>: compare with the true symbol file of <to> (when its source exists) and print
how many names are right, wrong and missing; nothing is written.
"""
import re
import sys
import zlib
from collections import defaultdict

BASE = 0xFF800000
CTX = 6        # words of context on each side of an operand
CODE_LEN = 24  # words compared at a code label


def words(lo, hi):
    a, b = open(lo, 'rb').read(), open(hi, 'rb').read()
    return [a[i] | b[i] << 8 for i in range(len(a))]


def crc(w):
    return zlib.crc32(bytes(x for v in w for x in (v & 0xff, v >> 8))) & 0xffffffff


def load_syms(p):
    return {k: int(v, 16) for k, v in re.findall(r'^\s*(\w+) = (0x[0-9A-Fa-f]+),', open(p).read(), re.M)}


def addrlike(v):
    return 0x01000000 <= v < 0x01800000 or v >= BASE or 0x00000000 < v < 0x00100000 and False


def masked(w, i, j):
    """words i..j-1 with every 32-bit address-like pair turned into None"""
    out = list(w[i:j])
    k = 0
    while k < len(out) - 1:
        v = out[k] | out[k + 1] << 16 if out[k] is not None and out[k + 1] is not None else None
        if v is not None and addrlike(v):
            out[k] = out[k + 1] = None
            k += 2
        else:
            k += 1
    return out


class Finder:
    def __init__(self, w):
        self.w = w
        self.idx = defaultdict(list)
        for i in range(len(w) - 2):
            self.idx[(w[i], w[i + 1], w[i + 2])].append(i)

    def find(self, pat):
        """positions where pat (None = any) matches; anchored on 3 fixed words"""
        a = next((k for k in range(len(pat) - 2) if None not in pat[k:k + 3]), None)
        if a is None:
            return []
        hits = []
        for p in self.idx.get(tuple(pat[a:a + 3]), ()):
            s = p - a
            if s < 0 or s + len(pat) > len(self.w):
                continue
            if all(x is None or self.w[s + k] == x for k, x in enumerate(pat)):
                hits.append(s)
                if len(hits) > 1:
                    break
        return hits


def carry(syms, fw, tw):
    refs = defaultdict(list)                     # address -> word positions of operands in fw
    for i in range(len(fw) - 1):
        v = fw[i] | fw[i + 1] << 16
        if addrlike(v):
            refs[v].append(i)
    f = Finder(tw)
    out = {}
    for name, a in syms.items():
        votes = defaultdict(list)
        for i in refs.get(a, [])[:12]:
            s, e = max(0, i - CTX), min(len(fw), i + 2 + CTX)
            pat = masked(fw, s, e)
            if sum(x is not None for x in pat) < 6:
                continue
            hits = f.find(pat)
            if len(hits) == 1:
                j = hits[0] + (i - s)
                votes[tw[j] | tw[j + 1] << 16].append((hits[0], pat))
        best = None
        if votes:
            na, places = max(votes.items(), key=lambda kv: len(kv[1]))
            others = sum(len(p) for k, p in votes.items() if k != na)
            if len(places) >= MIN_VOTES and others == 0:
                best = (na, places[0])
        if best is None and a >= BASE:           # a code label: the code itself
            i = (a - BASE) >> 4
            pat = masked(fw, i, i + CODE_LEN)
            if sum(x is not None for x in pat) >= 8:
                hits = f.find(pat)
                # not when a use of the name already points somewhere else (a data table)
                if len(hits) == 1 and (not votes or BASE + (hits[0] << 4) in votes):
                    best = (BASE + (hits[0] << 4), (hits[0], pat))
        if best:
            na, (pos, pat) = best
            out[name] = (na, BASE + (pos << 4), [tw[pos + k] if x is not None else -1 for k, x in enumerate(pat)])
    if GAP_FILL:
        fill_gaps(syms, out, tw)
    return out


def fill_gaps(syms, out, tw):
    """RAM names keep their order and spacing between revisions within a block: a name between
    two carried neighbours that moved by the same amount moves by it too, if the target program
    uses that address; the use (with that address kept in the pattern) is its proof."""
    ram = sorted((a, n) for n, a in syms.items() if 0x01000000 <= a < 0x01800000)
    tref = defaultdict(list)
    for i in range(len(tw) - 1):
        tref[tw[i] | tw[i + 1] << 16].append(i)
    added = {}
    for k, (a, n) in enumerate(ram):
        if n in out:
            continue
        lo = next(((b, m) for b, m in reversed(ram[:k]) if m in out), None)
        hi = next(((b, m) for b, m in ram[k + 1:] if m in out), None)
        if not lo or not hi:
            continue
        d1, d2 = out[lo[1]][0] - lo[0], out[hi[1]][0] - hi[0]
        if d1 != d2 or hi[0] - lo[0] > 0x2000:        # same shift, and neighbours close by
            continue
        na = a + d1
        uses = tref.get(na, [])
        if not uses:
            continue
        i = uses[0]
        s = max(0, i - CTX)
        pat = masked(tw, s, min(len(tw), i + 2 + CTX))
        pat[i - s], pat[i - s + 1] = tw[i], tw[i + 1]   # the address itself is part of the proof
        added[n] = (na, BASE + (s << 4), [x if x is not None else -1 for x in pat])
    out.update(added)


MIN_VOTES = 2
GAP_FILL = True


def main(a):
    check = agree = None
    if '--check' in a:
        k = a.index('--check'); check = a[k + 1]; a = a[:k] + a[k + 2:]
    if '--agree' in a:
        k = a.index('--agree'); agree = a[k + 1:k + 4]; a = a[:k] + a[k + 4:]
    if len(a) != 8:
        sys.exit(__doc__)
    syms = load_syms(a[1])
    fw, tw = words(a[2], a[3]), words(a[4], a[5])
    res = carry(syms, fw, tw)
    note = ''
    if agree:
        # carried again from a second source revision: keep the names both give the same address
        res2 = carry(load_syms(agree[0]), words(agree[1], agree[2]), tw)
        both = {n: r for n, r in res.items() if n in res2 and res2[n][0] == r[0]}
        print(f'--agree: {len(res)} and {len(res2)} carried, {len(both)} on both with the same address, '
              f'{sum(1 for n in res if n in res2 and res2[n][0] != res[n][0])} differ')
        res = both
        note = f' and {agree[0].split("/")[-1]} (only names both give the same address)'
    if check:
        true = load_syms(check)
        right = sum(1 for n, (v, _, _) in res.items() if true.get(n) == v)
        wrong = [n for n, (v, _, _) in res.items() if n in true and true[n] != v]
        print(f'{len(syms)} names: {len(res)} carried, {right} right, {len(wrong)} wrong, {len(syms) - len(res)} not carried'
              + (f'; wrong: {", ".join(wrong[:10])}' if wrong else ''))
        return
    c = crc(tw)
    path = f'{a[7]}/{a[6]}_{c:08x}.lua'
    L = [f'-- {a[6]}, program ROM CRC-32 {c:08x}: {len(res)} of the {len(syms)} names of {a[1].split("/")[-1]}{note},',
         '-- carried by tools/sym_carry.py (no source for this revision). Each entry holds the words',
         '-- of the program that prove it; the program uses the name only when they match.',
         'return {']
    for n in sorted(res, key=str.lower):
        v, at, pat = res[n]
        L.append(f'  {n} = {{ addr = 0x{v:08X}, at = 0x{at:08X}, pattern = {{ {", ".join(str(x) for x in pat)} }} }},')
    L.append('}')
    open(path, 'w', newline='\n').write('\n'.join(L) + '\n')
    print(f'{path}: {len(res)} of {len(syms)} names carried')


if __name__ == '__main__':
    main(sys.argv)
