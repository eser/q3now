#!/bin/bash
# Generate MSDF font atlases for q3now
# Requires: msdf-atlas-gen (https://github.com/Chlumsky/msdf-atlas-gen)
#
# Usage: ./generate_atlases.sh [output_dir]
# Default output: ../../baseq3/fonts/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${1:-${SCRIPT_DIR}/../../baseq3/fonts}"
FONT_DIR="${SCRIPT_DIR}/fonts"
CHARSET_LATIN="${SCRIPT_DIR}/charset_latin.txt"
CHARSET_CONSOLE="${SCRIPT_DIR}/charset_console.txt"

# Atlas generation parameters
ATLAS_SIZE=1024
FONT_SIZE=72
DISTANCE_RANGE=8
TYPE=msdf

# Check for msdf-atlas-gen
if ! command -v msdf-atlas-gen &>/dev/null; then
    echo "Error: msdf-atlas-gen not found in PATH"
    echo "Install from: https://github.com/Chlumsky/msdf-atlas-gen"
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

generate_font() {
    local name="$1"
    local ttf="$2"
    local charset="$3"
    local out_name="$4"

    if [ ! -f "${FONT_DIR}/${ttf}" ]; then
        echo "Warning: ${ttf} not found in ${FONT_DIR}, skipping ${name}"
        return 1
    fi

    echo "Generating ${name} atlas..."
    msdf-atlas-gen \
        -font "${FONT_DIR}/${ttf}" \
        -charset "${charset}" \
        -type "${TYPE}" \
        -format png \
        -size "${FONT_SIZE}" \
        -pxrange "${DISTANCE_RANGE}" \
        -dimensions "${ATLAS_SIZE}" "${ATLAS_SIZE}" \
        -imageout "${OUTPUT_DIR}/${out_name}_atlas.png" \
        -json "${OUTPUT_DIR}/${out_name}.json"

    echo "  -> ${OUTPUT_DIR}/${out_name}_atlas.png"
    echo "  -> ${OUTPUT_DIR}/${out_name}.json"
}

# Generate atlases for each font
generate_font "Enter Sansman" "EnterSansman-Regular.ttf" "${CHARSET_LATIN}" "sansman"
generate_font "Oxanium Regular" "Oxanium-Regular.ttf" "${CHARSET_LATIN}" "oxanium"
generate_font "Share Tech Mono" "ShareTechMono-Regular.ttf" "${CHARSET_CONSOLE}" "console"

echo ""
echo "Done. Atlas files written to: ${OUTPUT_DIR}"
echo "Expected files:"
echo "  sansman_atlas.png + sansman.json"
echo "  oxanium_atlas.png + oxanium.json"
echo "  console_atlas.png + console.json"
