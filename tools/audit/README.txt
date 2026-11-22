The controller-only audit (roadmap U7.1)
=======================================

Each .pad file is a script for `NBAJamRecomp --pad-script <file>`: a virtual controller
presses buttons as written and the keyboard is ignored, so anything that would still need
a key shows up as a FAIL line or a stuck run. The commands are described in
src/app/pad_script.h.

  a_noroms.pad   first start with no ROMs: every setting changed, quit
  b_flows.pad    with ROMs: a Lua mod, a button bound, News, About, a game, the pause
                 menu (held Start + Back, then Guide), quit to the game list, quit
  c_host.pad     online, the host: host, start, pause menu while the game goes on, leave
  c_join.pad     online, the other side: the host's address on the on-screen keyboard,
                 join, play, back on the game list when the host leaves
  e_deck.pad     the menus at 1280 x 800 (Steam Deck), then inside a TV's safe area
  f_states.pad   save states (N1.4): with save_states=1 in the data folder's ini, Save in
                 slot 1, play on, Load; the log says "as saved"

run_audit.sh runs one under Xvfb (Linux build); run c_host and c_join at the same time,
each with its own data folder. The scripts that need ROMs use NBA JAM, T.E. and Maximum
Hangtime positions in the game list, so a data folder with all five games is assumed.
Screenshots land in the data folder's screenshots/ and contain game pictures: keep them
out of the repository.
