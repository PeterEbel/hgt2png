#!/usr/bin/env bash
set -euo pipefail

# ------------------ Einstellungen ------------------
WORKDIR="."           # Verzeichnis mit den Kacheln
OUT="mosaic.png"      # Ausgabedatei
# --------------------------------------------------

cd "$WORKDIR"

shopt -s nullglob
files=(N*.jpg)
if [ ${#files[@]} -eq 0 ]; then
    echo "FEHLER: Keine Dateien N*.jpg gefunden."
    exit 1
fi

# ------------------ Funktionen -------------------
# Extrahiere Latitude und Longitude aus Dateiname
parse_coords() {
    local f=$(basename "$1" .jpg)
    if [[ $f =~ ^([NS])([0-9]{1,3})([EW])([0-9]{1,3})$ ]]; then
        local NS=${BASH_REMATCH[1]}
        local LAT=$((10#${BASH_REMATCH[2]}))
        local EW=${BASH_REMATCH[3]}
        local LON=$((10#${BASH_REMATCH[4]}))
        [[ $NS == S ]] && LAT=$((-LAT))
        [[ $EW == W ]] && LON=$((-LON))
        echo "$LAT $LON"
    else
        echo "ERROR"
    fi
}
# --------------------------------------------------

# Bestimme min/max für Latitude/Longitude
minlat=999; maxlat=-999
minlon=999; maxlon=-999
for f in "${files[@]}"; do
    coords=$(parse_coords "$f")
    [[ $coords == "ERROR" ]] && continue
    lat=${coords%% *}; lon=${coords##* }
    (( lat < minlat )) && minlat=$lat
    (( lat > maxlat )) && maxlat=$lat
    (( lon < minlon )) && minlon=$lon
    (( lon > maxlon )) && maxlon=$lon
done

cols=$(( maxlon - minlon + 1 ))
rows=$(( maxlat - minlat + 1 ))

echo "Raster: ${cols} Spalten x ${rows} Reihen"

# ------------------ Sortiere Dateien ------------------
declare -a sorted_files
for ((r=maxlat;r>=minlat;r--)); do
    for ((c=minlon;c<=maxlon;c++)); do
        for f in "${files[@]}"; do
            coords=$(parse_coords "$f")
            [[ $coords == "ERROR" ]] && continue
            lat=${coords%% *}; lon=${coords##* }
            if [[ $lat -eq $r && $lon -eq $c ]]; then
                sorted_files+=("$f")
                break
            fi
        done
    done
done

# ------------------ Mosaik erstellen ------------------
# montage setzt Bilder automatisch im Raster zusammen
magick montage "${sorted_files[@]}" -tile "${cols}x${rows}" -geometry +0+0 -background none "$OUT"

echo "FERTIG: $OUT erzeugt."

