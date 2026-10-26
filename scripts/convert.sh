#!/usr/bin/env bash

hgt2png --remove-islands 5000 --scale-factor 2 --threads 8 --quiet --min-height 0 --max-height 3482 --curve log --alpha-nodata liste.txt

