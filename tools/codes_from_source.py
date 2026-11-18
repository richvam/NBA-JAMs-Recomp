#!/usr/bin/env python3
"""codes_from_source.py - write mods/lib/codes_nbajamte.lua and codes_nbajamse.lua (roadmap 2.1).

    codes_from_source.py <T.E. source dir> <SE source dir> [<MHT 1.3 source dir>] <mods/lib folder>

The Maximum Hangtime 1.3 source is the repository's SRC/ with BACKUP/CODE113M/ copied over it
(as tools/build_roms.sh builds it); it writes codes_nbamht.lua (roadmap 2.2).

The cabinet codes of T.E. 4.0 and SE 5.02 are the same (SCORE.ASM, PLYR.ASM, SELECT.ASM and
SELECT2.ASM differ only in formatting, apart from the secret players); they are listed by
hand below, from a reading of the sources (see the comments). The secret players (initials
and a birth date on the initials screen) are read from SELECT2.ASM's #team_inits table:
the game stores the row number of the match in special_heads, and only scans the first
<limit> rows (61 in T.E., 68 in SE).
"""
import re
import sys

# kind: "flag" (one word, nonzero = on), "players" (bit 0-3 = player 1-4),
# "taps" (the player's tap count pN taps must equal value), "team" (per team),
# "initials" (special_heads[player] = value). sym_note: why sym is not in the symbol file.
CODES = [
    dict(id="tournament", en="Tournament mode", sym="plyr_tourn", kind="flag", when="matchup",
         original="Hold turbo, pass, shoot and right on the matchup screen",
         cleared="start of every matchup screen (flash_plyr_wht) and at game start",
         tournament="this is tournament mode: it also resets special_heads (secret players) to normal heads",
         source="SCORE.ASM flash_plyr_wht, tournament_on"),
    dict(id="max_power", en="Maximum power", sym="plyr_maxpower", kind="players", when="matchup",
         original="Hold turbo, pass, shoot and down on the matchup screen (player 1 without start; players 2-4 with start held)",
         cleared="start of every matchup screen",
         tournament="still set; only its message is not shown (secret_power returns early)",
         source="SCORE.ASM flash_plyr_wht, secret_power"),
    dict(id="shot_percent", en="Show shot percentage", sym="show_shot", kind="flag", when="matchup",
         original="Tap any button 15 times or more, then turn the stick a full circle, on the matchup screen",
         cleared="start of every matchup screen; attract-mode demo games",
         tournament="not blocked",
         source="SCORE.ASM ck_circle_stik, secret_perc"),
    dict(id="assist_off", en="Computer assistance off", sym="assistance_off", kind="flag", when="matchup",
         original="Tap any button exactly 10 times on the matchup screen (about 1.5 s in, when the game checks)",
         cleared="when the matchup check runs (assist_off sets it again); attract-mode demo games",
         tournament="forced on, with the message TOURNAMENT MODE / NO CPU ASSISTANCE, POWERUPS, OR SECRETS ALLOWED!",
         source="SCORE.ASM assist_off"),
    dict(id="goaltend", en="Powerup goaltending", sym="p1taps", kind="taps", value=24, when="matchup",
         original="Tap any button exactly 24 times on the matchup screen",
         cleared="start of every matchup screen",
         tournament="taps are cleared, so it cannot be had",
         effect="the player's goaltends are let go 70% of the time (PLYR2.ASM)",
         source="SCORE.ASM secret_1, PLYR2.ASM"),
    dict(id="quick_hands", en="Quick hands", sym="stlpower", kind="players", when="tipoff",
         original="Tap any button 5 times or more on the matchup screen, then hold shoot and down at tip-off",
         cleared="every game start (plyr_start)",
         tournament="cleared by tournament mode",
         source="PLYR.ASM plyr_main, SCORE.ASM secret_3"),
    dict(id="big_head", en="Big head", sym="plyrbighead", kind="players", when="tipoff",
         original="Hold up, turbo and pass at tip-off",
         cleared="set from the head size adjustment at every game start (plyr_start), then flipped by the code",
         tournament="not blocked",
         source="PLYR.ASM plyr_main, SCORE.ASM secret_2"),
    dict(id="huge_head", en="Huge head", sym="plyrbighead", kind="players", taps=6, when="tipoff",
         original="Tap any button exactly 6 times on the matchup screen, then hold up, turbo and pass at tip-off",
         cleared="as big head; the tap count is cleared at every matchup screen",
         tournament="taps are cleared, so only the normal big head is left",
         conflicts=["big_head"],
         source="PLYR.ASM (head scale), SCORE.ASM secret_2"),
    dict(id="baby", en="Baby-size players", sym="baby_flag", kind="flag", when="matchup",
         original="Hold down and right, and press turbo, shoot, pass three times (one at a time) on the matchup screen",
         cleared="start of every matchup screen and of team select",
         tournament="still set; only its message is not shown",
         source="SCORE.ASM ck_baby, baby_message"),
    dict(id="big_roster", en="Expanded roster", sym="bigroster1", kind="team", sym_note="a local .bss variable of SELECT.ASM: not in the symbol file (only global names are)", when="team_select",
         original="On team select, tap pass 4 times or more while the stick has been turned all the way round",
         cleared="start of team select, and when the team changes before the first quarter",
         tournament="blocked",
         source="SELECT.ASM (bigroster1 / bigroster2, tm1taps, tm1swrl)"),
    dict(id="team_switch", en="Switch teams at halftime", sym="team1_switch", kind="team", sym_note="a local .bss variable of SELECT.ASM: not in the symbol file (only global names are)", when="halftime",
         original="On the halftime team screen, hold pass and right for 2 seconds (120 frames); then pass changes the team",
         cleared="start of team select",
         tournament="blocked",
         source="SELECT.ASM (team1_switch / team2_switch, switch_1)"),
]


def secret_players(path):
    s = open(path, encoding='latin-1').read().replace('\r', '')
    limit = int(eval(re.search(r'cmpi\s+([0-9+]+),a5\s*\n\s*jrlt\s+#next', s, re.I).group(1)))
    t = s[s.index('\n#team_inits\n') + 1:]   # the label, not the movi that loads it
    rows = []
    for line in t.split('\n')[1:]:
        if not line.strip():
            continue
        m = re.match(r"\s*\.word\s+(.+?),\s*(\d+)\s*,\s*(\d+)\s*;\s*(\d+)\s+(.*)", line, re.I)
        if not m:
            if line.strip().startswith(';'):
                continue
            break
        letters = [x.strip() for x in m.group(1).split(',')]
        rows.append(dict(ini=letters, month=int(m.group(2)), day=int(m.group(3)), name=m.group(5).split('*')[0].strip()))
    return rows, limit


def letters_of(raw):
    out = []
    for x in raw:
        x = x.replace("'", '').replace('-A', '').strip()
        out.append(x if re.fullmatch('[A-Z]', x) else '_' if x == '26' else '?')
    return ''.join(out)


def lua_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write(game, rev, path_select2, out):
    rows, limit = secret_players(path_select2)
    L = [f'-- codes_{game}.lua - the cabinet codes of {rev} (roadmap 2.1), from the sources.',
         '-- Written by tools/codes_from_source.py; checked in modtest by tools/modtests/codes_te.lua.',
         '-- kind: "flag" (nonzero = on), "players" (bit 0-3 = player 1-4), "taps" (the player\'s',
         '-- tap count pNtaps must equal value), "team" (bigroster1/2, team1/2_switch), "initials"',
         '-- (special_heads[player] = value). when: matchup, tipoff, team_select, halftime, initials.',
         '-- Spanish labels: roadmap 2.5.',
         'return {']
    for c in CODES:
        f = [f'id = {lua_str(c["id"])}', f'label = {{ en = {lua_str(c["en"])} }}', f'original = {lua_str(c["original"])}',
             f'sym = {lua_str(c["sym"])}', f'kind = {lua_str(c["kind"])}']
        if 'value' in c: f.append(f'value = {c["value"]}')
        if 'taps' in c: f.append(f'taps = {c["taps"]}')
        f.append(f'when = {lua_str(c["when"])}')
        if 'conflicts' in c: f.append('conflicts = { ' + ', '.join(lua_str(x) for x in c['conflicts']) + ' }')
        if 'sym_note' in c: f.append(f'sym_note = {lua_str(c["sym_note"])}')
        if 'effect' in c: f.append(f'effect = {lua_str(c["effect"])}')
        f += [f'cleared = {lua_str(c["cleared"])}', f'tournament = {lua_str(c["tournament"])}', f'source = {lua_str(c["source"])}']
        L.append('  { ' + (',\n    '.join(f)) + ' },')
    L.append(f'  -- secret players: initials and birth date (month, day) on the initials screen; the game scans')
    L.append(f'  -- the first {limit} rows of SELECT2.ASM #team_inits. "_" is the character after Z.')
    L.append(f'  -- Not given when the tournament adjustment is on (ADJTOURNEY); tournament mode by code resets them.')
    for i, r in enumerate(rows[:limit]):
        ini = letters_of(r['ini'])
        slug = re.sub(r'[^a-z0-9]+', '_', r['name'].lower()).strip('_')
        L.append(f'  {{ id = "player_{slug}", label = {{ en = {lua_str(r["name"].title())} }}, original = "{ini} {r["month"]:02d}/{r["day"]:02d}", '
                 f'sym = "special_heads", kind = "initials", value = {i}, when = "initials" }},')
    L.append('}')
    open(out, 'w', newline='\n').write('\n'.join(L) + '\n')
    return rows, limit


# Maximum Hangtime 1.3 (roadmap 2.2): SELECT3.ASM, BB3.ASM, PLYR.ASM, SCORE.ASM, COMBOS.DOC.
# Number codes: the three icons on the versus screen count turbo, shoot and pass presses
# (stick up while pressing counts down); a match in combination_tbl ORs the player's bit into
# the variable while the screen lasts (5 s). When it ends, set_plyrs_powerup_ram turns each of
# pup_baby..pup_fastpass into 0 or 0xF (all four players): in a game with humans on both
# teams the bits must come from both teams, otherwise any one bit is enough. Tournament mode
# is kept per player and is refused when only one human plays.
MHT_NUM = {  # combination_tbl label -> (variable, id, English label)
    'TOURN': ('pup_tournament', 'tournament', 'Tournament mode'),
    'BABYS': ('pup_baby', 'baby', 'Baby-sized players'),
    'NOMUS': ('pup_nomusic', 'no_music', 'No music during play'),
    'GOALT': ('pup_goaltend', 'goaltend', 'Goaltending allowed'),
    'MXBLK': ('pup_maxblock', 'max_block', 'Shot blocking'),
    'MXSTL': ('pup_maxsteal', 'max_steal', 'Maximum steals'),
    'MXPWR': ('pup_maxpower', 'max_power', 'Maximum power'),
    'MXSPD': ('pup_maxspeed', 'max_speed', 'Maximum speed'),
    'HYPER': ('pup_hypspeed', 'hyper_speed', 'Hyper speed'),
    'TBSTL': ('pup_trbstealth', 'stealth_turbo', 'Stealth turbo'),
    'TBINF': ('pup_trbinfinite', 'infinite_turbo', 'Infinite turbo'),
    'NOPSH': ('pup_nopush', 'no_push', 'No pushing'),
    'FPASS': ('pup_fastpass', 'fast_pass', 'Fast turbo passes'),
}
MHT_COSMETIC = {'pup_nomusic'}   # kept by tournament mode (clear_secret_powerup_tmode skips it)
MHT_STICK = [  # BB3.ASM #special_moves, entered on the versus screen, set at once for that player
    dict(id="lock_combo", en="Lock the number code", sym="pup_lockcombo", kind="players",
         original="Press down on the versus screen: the player's three icons stop changing", source="BB3.ASM sp_lockcombo"),
    dict(id="big_head", en="Big head", sym="pup_bighead", kind="players", conflicts=["huge_head"],
         original="Hold up, turbo and pass together", tournament="kept", source="BB3.ASM sp_bighead"),
    dict(id="huge_head", en="Huge head", sym="pup_hugehead", kind="players", conflicts=["big_head"],
         original="Up, up, pass, turbo (within 1 s)", tournament="kept", source="BB3.ASM sp_hugehead"),
    dict(id="shot_percent", en="Show shot percentage", sym="pup_showshotper", kind="players",
         original="Turn the stick a full circle clockwise from up (within 1 s); only the first player to do it",
         tournament="kept", source="BB3.ASM sp_showshotper"),
    dict(id="no_tag", en="No tag arrows", sym="pup_notag", kind="players",
         original="Left, left, pass, turbo (within 1 s)", source="BB3.ASM sp_notag"),
    dict(id="no_drift", en="No drift on block jumps", sym="pup_nodrift", kind="players",
         original="Down, down, shoot, turbo (within 1 s)", source="BB3.ASM sp_nodrift"),
    dict(id="no_assistance", en="Computer assistance off", sym="pup_noassistance", kind="flag",
         original="Hold right and press pass twice (within 1 s)", source="BB3.ASM sp_noassistance"),
    dict(id="aba_ball", en="ABA ball", sym="pup_aba", kind="flag",
         original="Hold right and press shoot, turbo, pass (within 1 s)", tournament="kept", source="BB3.ASM sp_aba"),
    dict(id="outdoor_court", en="Outdoor court", sym="pup_court", kind="flag",
         original="Hold left and press turbo twice (within 1 s)", tournament="kept", source="BB3.ASM sp_court"),
    dict(id="special", en="Special combo (ABA ball, outdoor court, shot percentage, big head)", sym="pup_aba", kind="flag",
         original="Shoot, shoot, then hold shoot and turn the stick a full circle clockwise from up (within 1.3 s)",
         tournament="kept", source="BB3.ASM sp_special"),
]


def write_mht(src, out):
    s = open(src + '/SELECT3.ASM', encoding='latin-1').read().replace('\r', '')
    t = s[s.index('\ncombination_tbl\n'):s.index('\ncombination_tbl_end')]
    nums = re.findall(r'^\s*WL\s+>([0-9A-Fa-f]{3}),(\w+)\s*$', t, re.M)   # commented lines start with ';'
    L = ['-- codes_nbamht.lua - the cabinet codes of NBA Maximum Hangtime 1.3 (roadmap 2.2), from the sources.',
         '-- Written by tools/codes_from_source.py; checked in modtest by tools/modtests/codes_mht.lua.',
         '-- kind "number": a 3-digit code on the versus screen (turbo, shoot, pass presses), value = the',
         '-- digits. When the screen ends the variable is 0 or 0xF for every player; with humans on both',
         '-- teams both teams must enter it (tournament: any one player, refused with one human).',
         '-- kind "players": bit 0-3 = player 1-4, set at once; "flag": nonzero = on.',
         '-- All are cleared at game start and at game over. Tournament mode clears the others than',
         '-- those marked tournament = "kept". Spanish labels: roadmap 2.5.',
         '-- Not in 1.3 although named in the source: show hot spots (pup_showhotspots is only cleared;',
         '-- its number code 030 is commented out).',
         'return {']
    for code, lab in nums:
        sym, cid, en = MHT_NUM[lab]
        kept = sym in MHT_COSMETIC
        L.append(f'  {{ id = {lua_str(cid)}, label = {{ en = {lua_str(en)} }}, original = "{code}", sym = {lua_str(sym)}, '
                 f'kind = "number", value = {int(code)}, when = "matchup", '
                 f'tournament = {lua_str("this is tournament mode" if cid == "tournament" else "kept" if kept else "cleared")} }},')
    for c in MHT_STICK:
        f = [f'id = {lua_str(c["id"])}', f'label = {{ en = {lua_str(c["en"])} }}', f'original = {lua_str(c["original"])}',
             f'sym = {lua_str(c["sym"])}', f'kind = {lua_str(c["kind"])}', 'when = "matchup"']
        if 'conflicts' in c: f.append('conflicts = { ' + ', '.join(lua_str(x) for x in c['conflicts']) + ' }')
        f += [f'tournament = {lua_str(c.get("tournament", "cleared"))}', f'source = {lua_str(c["source"])}']
        L.append('  { ' + ', '.join(f) + ' },')
    L.append('}')
    open(out, 'w', newline='\n').write('\n'.join(L) + '\n')
    return nums


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    if len(sys.argv) == 5:   # <T.E.> <SE> <Maximum Hangtime 1.3 source> <lib>
        nums = write_mht(sys.argv[3], sys.argv[4] + '/codes_nbamht.lua')
        print(f'Maximum Hangtime: {len(nums)} number codes ({", ".join(c for c, _ in nums)}), {len(MHT_STICK)} stick codes')
        del sys.argv[3]
    te, se, lib = sys.argv[1:]
    a, la = write('nbajamte', 'NBA JAM Tournament Edition 4.0', te + '/SELECT2.ASM', lib + '/codes_nbajamte.lua')
    b, lb = write('nbajamse', 'NBA JAM T.E. Special Edition 5.02', se + '/SELECT2.ASM', lib + '/codes_nbajamse.lua')
    na = {letters_of(r['ini']) + str(r['month']) + str(r['day']): r['name'] for r in a[:la]}
    nb = {letters_of(r['ini']) + str(r['month']) + str(r['day']): r['name'] for r in b[:lb]}
    print(f'T.E.: {len(CODES)} codes, {la} secret players; SE: {len(CODES)} codes, {lb} secret players')
    print('SE adds:', ', '.join(sorted(nb[k] for k in nb.keys() - na.keys())))
    print('SE drops:', ', '.join(sorted(na[k] for k in na.keys() - nb.keys())))
