#!/usr/bin/env bash
# IBL ambient r_pbr 0-vs-1 verify (6C-2.3).
#
# The IBL ambient in light_frag's USE_PBR path only renders in the PMLIGHT/dlight
# pass, so a dynamic light must hit the pbrMap weapon view-model. We override the
# rocket launcher with a pbrMap (modfiles) and FIRE a rocket: the rocket's own
# in-flight dlight (~200 intensity, sustained for the whole flight) lights the
# held metallic weapon — a reliable IBL-trigger frame. We capture a burst during
# the flight under r_pbr 0 then r_pbr 1 and region-diff the lower (weapon) band:
# r_pbr 1 should differ measurably (the IBL ambient) and the upper-vs-lower split
# shows the analytic sky is directional.
#
# Button commands (+attack) must be issued from an exec'd cfg — on the command
# line the engine strips the leading '+' as its run-command marker. So this writes
# a cfg into the engine home and +exec's it.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"

# Refuse to measure with byte readers that do not work on this host. Every
# number this script prints comes out of a png2raw|od|awk pipeline, and when od
# fails the pipeline yields no rows: awk then prints 0, which reads as "no
# difference" rather than "nothing was read". Fail loudly instead.
wired_od_selftest || { echo "FAIL: byte readers unusable on this host — refusing to report zeros as measurements"; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
# Isolated capture home, NOT the player's: the capture cfgs below set
# CVAR_ARCHIVE cvars (cg_drawGun, r_pbr, ...) and a clean exit writes them
# back into config.cfg. See wired_isolated_home in lib/wired_paths.sh.
HOME_DIR="${HOME_DIR:-$( wired_isolated_home ibl-pbr-home )}"
HOME_DIR_NATIVE="$(cygpath -w "$HOME_DIR" 2>/dev/null || echo "$HOME_DIR")"
BASE_DIR="$HOME_DIR/base"
SHOT_DIR="$BASE_DIR/screenshots"
FRAME_W=1280
FRAME_H=720

band_mean() {
	local shot="$1" row0="$2" row1="$3"
	[ -s "$shot" ] || { echo "0"; return; }
	"$PNG2RAW" "$shot" \
	  | wired_od_bytes \
	  | awk -v W="$FRAME_W" -v r0="$row0" -v r1="$row1" '
	      BEGIN{ stride=W*3; tot=0; sum=0; idx=0 }
	      { for (i=1;i<=NF;i++){ row=int(idx/stride); if(row>=r0 && row<r1){tot++; sum+=$i} idx++ } }
	      END{ if(tot>0) printf "%.3f", sum/tot; else printf "0" }'
}

# Brightest lower-band frame among a set (most likely on a rocket-dlight frame).
brightest_lower() {
	local best="" bestm=-1 s m
	for s in "$@"; do
		[ -s "$s" ] || continue
		m="$(band_mean "$s" 480 720)"
		awk -v a="$m" -v b="$bestm" 'BEGIN{exit !(a>b)}' && { best="$s"; bestm="$m"; }
	done
	echo "$best $bestm"
}

write_cfg() {
	local tag="$1" pbr="$2"
	# Give + select rocket launcher (weapon 5), turn to face a wall so the rocket
	# detonates close (keeping its dlight near the player/weapon longer), then fire
	# repeatedly and snap a burst across the flight/explosion dlights.
	cat > "$BASE_DIR/iblcap.cfg" <<CFG
set r_pbr $pbr
set cg_drawGun 1
set r_brightness 1
give all
wait 10
weapon 5
wait 10
+attack
wait 4
-attack
wait 2
+attack
wait 4
-attack
wait 2
screenshot ibl_${tag}_1 png silent
+attack
wait 4
-attack
wait 2
screenshot ibl_${tag}_2 png silent
+attack
wait 4
-attack
wait 2
screenshot ibl_${tag}_3 png silent
+attack
wait 4
-attack
wait 2
screenshot ibl_${tag}_4 png silent
+attack
wait 4
-attack
wait 6
screenshot ibl_${tag}_5 png silent
wait 10
quit
CFG
}

run_capture() {
	local tag="$1" pbr="$2"
	rm -f "$SHOT_DIR"/ibl_${tag}_*.png 2>/dev/null
	write_cfg "$tag" "$pbr"
	( cd "$REPO_ROOT" && make run-game DEV=1 EXTRA_ARGS="+set fs_homepath \"$HOME_DIR_NATIVE\" +set r_fullscreen 0 +set r_mode -1 +set r_customwidth $FRAME_W +set r_customheight $FRAME_H +map arena7 +waitForMap +wait 60 +exec iblcap.cfg" >"$WIRED_TMP/ibl-$tag.log" 2>&1 || true )
}

echo "== capturing r_pbr 0 =="
run_capture pbr0 0
echo "== capturing r_pbr 1 =="
run_capture pbr1 1

shot0="$(brightest_lower "$SHOT_DIR"/ibl_pbr0_*.png)"
shot1="$(brightest_lower "$SHOT_DIR"/ibl_pbr1_*.png)"
f0="${shot0%% *}"; m0="${shot0##* }"
f1="${shot1%% *}"; m1="${shot1##* }"

echo
echo "r_pbr 0 set: $(ls "$SHOT_DIR"/ibl_pbr0_*.png 2>/dev/null | wc -l) shots; brightest-lower $f0 mean=$m0"
echo "r_pbr 1 set: $(ls "$SHOT_DIR"/ibl_pbr1_*.png 2>/dev/null | wc -l) shots; brightest-lower $f1 mean=$m1"
[ -s "$f0" ] && [ -s "$f1" ] || { echo "FAIL: missing captures"; exit 1; }

up1="$(band_mean "$f1" 0 240)"
lo1="$(band_mean "$f1" 480 720)"
echo
echo "r_pbr1 upper(sky)-mean=$up1  lower(weapon)-mean=$lo1"
echo "lower-band delta (pbr1 - pbr0) = $(awk -v a="$m1" -v b="$m0" 'BEGIN{printf "%.3f", a-b}')"
