#!/bin/bash
# run_audit.sh <app> <script.pad> <data folder> [WxH]
# One controller-only audit script under Xvfb. Prints the PASS / FAIL lines; exit status 1
# when any FAIL. The data folder needs a roms folder for every script but a_noroms.
set -u
APP=$1; PAD=$2; DATA=$3; SIZE=${4:-1280x960}
mkdir -p "$DATA"
LOG=$(mktemp)
NBAJAM_DATA_DIR="$DATA" SDL_AUDIODRIVER=dummy timeout 600 \
	xvfb-run -a -s "-screen 0 ${SIZE}x24" "$APP" --pad-script "$PAD" > "$LOG" 2>&1
grep -aE "pad script|open folder:|Copied [0-9]|lifecycle:" "$LOG"
! grep -aq "FAIL" "$LOG"
