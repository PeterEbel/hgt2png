#!/usr/bin/env bash
# Lädt SRTM1-Kacheln (.hgt) von der NASA herunter und entpackt sie.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LINKS_FILE="$SCRIPT_DIR/../hgt/srtm1-links"
OUTPUT_DIR="$SCRIPT_DIR/../hgt"

usage() {
    cat <<EOF
Aufruf:
  $(basename "$0") [Optionen] KACHEL...        Einzelne Kacheln, z.B. N37W006 N38W006
  $(basename "$0") [Optionen] -f LINKDATEI     Download-Links aus Datei (eine URL pro Zeile)
  $(basename "$0") [Optionen] -a               Alle Kacheln aus der Standard-Linkliste

Optionen:
  -a, --all           Alle Kacheln laden
  -f, --file DATEI    Datei mit Download-Links
  -l, --links DATEI   Linkliste zum Auflösen von Kachelnamen (Standard: $LINKS_FILE)
  -o, --output DIR    Zielverzeichnis (Standard: $OUTPUT_DIR)
  -h, --help          Hilfe

Zugangsdaten: ~/.netrc (machine urs.earthdata.nasa.gov), sonst EARTHDATA_USER und
EARTHDATA_PASS, sonst Abfrage.
Bereits vorhandene .hgt-Dateien werden übersprungen.
EOF
}

all=0
list_file=""
tiles=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -a|--all) all=1 ;;
        -f|--file) list_file="${2:?Dateiname fehlt}"; shift ;;
        -l|--links) LINKS_FILE="${2:?Dateiname fehlt}"; shift ;;
        -o|--output) OUTPUT_DIR="${2:?Verzeichnis fehlt}"; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unbekannte Option: $1" >&2; usage >&2; exit 1 ;;
        *) tiles+=("$1") ;;
    esac
    shift
done

if (( all + (${#tiles[@]} > 0) + (${#list_file} > 0) != 1 )); then
    echo "Genau einen Modus wählen: Kacheln, --file oder --all." >&2
    usage >&2
    exit 1
fi

for cmd in curl unzip; do
    command -v "$cmd" >/dev/null || { echo "$cmd fehlt." >&2; exit 1; }
done

# --- URLs bestimmen ---
urls=()
if (( all )); then
    [[ -f "$LINKS_FILE" ]] || { echo "Linkliste nicht gefunden: $LINKS_FILE" >&2; exit 1; }
    mapfile -t urls < <(grep '^https' "$LINKS_FILE")
elif [[ -n "$list_file" ]]; then
    if [[ ! -f "$list_file" && -f "$SCRIPT_DIR/../$list_file" ]]; then
        list_file="$SCRIPT_DIR/../$list_file"
    fi
    [[ -f "$list_file" ]] || { echo "Datei nicht gefunden: $list_file" >&2; exit 1; }

    declare -A url_by_tile=()
    link_map_loaded=0

    while IFS= read -r entry || [[ -n "$entry" ]]; do
        entry="${entry%$'\r'}"
        entry="${entry%%#*}"
        read -r entry _ <<< "$entry"
        [[ -z "$entry" ]] && continue
        if [[ "$entry" == https://* ]]; then
            urls+=("$entry")
            continue
        fi

        if (( ! link_map_loaded )); then
            [[ -f "$LINKS_FILE" ]] || { echo "Linkliste nicht gefunden: $LINKS_FILE" >&2; exit 1; }
            while IFS= read -r link || [[ -n "$link" ]]; do
                link="${link%$'\r'}"
                [[ "$link" == https://* ]] || continue
                link_file="${link##*/}"
                tile="${link_file%%.SRTMGL1.hgt.zip}"
                url_by_tile["${tile^^}"]="$link"
            done < "$LINKS_FILE"
            link_map_loaded=1
        fi

        name="${entry##*/}"
        name="${name%%.*}"
        name="${name^^}"
        if [[ -n "${url_by_tile[$name]:-}" ]]; then
            urls+=("${url_by_tile[$name]}")
        else
            echo "WARNUNG: Kein Link für $name gefunden" >&2
        fi
    done < "$list_file"
else
    [[ -f "$LINKS_FILE" ]] || { echo "Linkliste nicht gefunden: $LINKS_FILE" >&2; exit 1; }
    for t in "${tiles[@]}"; do
        name="${t##*/}"
        name="${name%%.*}"
        name="${name^^}"
        url="$(grep -m1 "/${name}\.SRTMGL1\.hgt\.zip" "$LINKS_FILE" || true)"
        if [[ -z "$url" ]]; then
            echo "WARNUNG: Kein Link für $name gefunden" >&2
            continue
        fi
        urls+=("$url")
    done
fi

if (( ${#urls[@]} == 0 )); then
    echo "Keine Links zum Herunterladen." >&2
    exit 1
fi

# --- Zugangsdaten: ~/.netrc, sonst Umgebungsvariablen, sonst Abfrage ---
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cookies="$tmpdir/cookies"
umask 077

if [[ -f "$HOME/.netrc" ]] && grep -q 'machine urs.earthdata.nasa.gov' "$HOME/.netrc"; then
    netrc="$HOME/.netrc"
else
    user="${EARTHDATA_USER:-}"
    pass="${EARTHDATA_PASS:-}"
    if [[ -z "$user" ]]; then read -r -p "Earthdata-Benutzer: " user; fi
    if [[ -z "$pass" ]]; then read -r -s -p "Earthdata-Passwort: " pass; echo; fi
    netrc="$tmpdir/netrc"
    printf 'machine urs.earthdata.nasa.gov login %s password %s\n' "$user" "$pass" > "$netrc"
    unset pass
fi

mkdir -p "$OUTPUT_DIR"

total=${#urls[@]}
ok=0; skipped=0; failed=0
i=0
for url in "${urls[@]}"; do
    i=$((i + 1))
    file="${url##*/}"
    file="${file%%\?*}"
    hgt="${file%.SRTMGL1.hgt.zip}.hgt"

    if [[ -f "$OUTPUT_DIR/$hgt" ]]; then
        echo "[$i/$total] $hgt existiert bereits, übersprungen"
        skipped=$((skipped + 1))
        continue
    fi

    echo "[$i/$total] $file"
    if curl -fsS -L -b "$cookies" -c "$cookies" --netrc-file "$netrc" -g -o "$tmpdir/$file" -- "$url" \
        && unzip -q -o -j "$tmpdir/$file" -d "$tmpdir/x"; then
        src="$(find "$tmpdir/x" -iname '*.hgt' | head -n1)"
        mv "$src" "$OUTPUT_DIR/$hgt"
        ok=$((ok + 1))
    else
        echo "FEHLER: $file" >&2
        failed=$((failed + 1))
    fi
    rm -rf "$tmpdir/$file" "$tmpdir/x"
done

echo "Fertig: $ok geladen, $skipped übersprungen, $failed fehlgeschlagen"
(( failed == 0 ))
