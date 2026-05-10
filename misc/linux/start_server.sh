#!/bin/sh
echo "Edit this script to change the path to the wired engine's dedicated server executable"
echo "Set the sv_dlURL setting to a url like http://yoursite.com/q3now_path for q3now clients to download extra data."

# sv_dlURL needs to have quotes escaped like \"http://yoursite.com/q3now_path\" or it will be set to "http:" in-game.

~/wired/wired-ded +set dedicated 2 +set sv_allowDownload 1 +set sv_dlURL \"\" +set com_hunkmegs 128 "$@"
