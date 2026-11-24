#!/bin/sh
# The Flatpak's launcher: the program lives next to its mods in /app/lib/nbajamrecomp, which
# it reads to seed the data folder on first start.
exec /app/lib/nbajamrecomp/NBAJamRecomp "$@"
