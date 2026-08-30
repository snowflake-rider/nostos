#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
audio_dir="$project_dir/public/audio"
mkdir -p "$audio_dir"

/opt/homebrew/bin/ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "sine=frequency=55:duration=84:sample_rate=44100" \
  -f lavfi -i "sine=frequency=82.41:duration=84:sample_rate=44100" \
  -f lavfi -i "anoisesrc=color=pink:duration=84:sample_rate=44100:amplitude=0.015" \
  -filter_complex "[0:a]volume=0.05[a0];[1:a]volume=0.025[a1];[2:a]lowpass=f=900,volume=0.18[a2];[a0][a1][a2]amix=inputs=3:duration=longest,afade=t=in:st=0:d=3,afade=t=out:st=79:d=5" \
  -codec:a libmp3lame -q:a 4 "$audio_dir/ambient.mp3"

