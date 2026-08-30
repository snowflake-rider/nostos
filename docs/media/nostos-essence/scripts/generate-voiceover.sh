#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
audio_dir="$project_dir/public/audio"
mkdir -p "$audio_dir"

voice="Samantha"
rate="176"

render_line() {
  local id="$1"
  local text="$2"
  local aiff="$audio_dir/$id.aiff"
  local mp3="$audio_dir/$id.mp3"

  /usr/bin/say -v "$voice" -r "$rate" -o "$aiff" "$text"
  /opt/homebrew/bin/ffmpeg -hide_banner -loglevel error -y \
    -i "$aiff" -codec:a libmp3lame -q:a 3 "$mp3"
  rm "$aiff"
}

render_line "scene01" "In a group ride, the most dangerous moment is the one another rider cannot see."
render_line "scene02" "Nostos connects three bicycles as one shared sense."
render_line "scene03" "The lead rider measures speed. The middle rider reads temperature and humidity. The tail rider watches motion and orientation. Three different sensors become one group state."
render_line "scene04" "On every node, an STM thirty two interprets local input. An ESP thirty two S three carries the message into Bluetooth Mesh. The result returns as a display, light, warning tone, or voice."
render_line "scene05" "Speed up. Slow down. Stop. A single button turns one rider's intent into a signal the whole line can understand."
render_line "scene06" "And when the signal is a fall or an S O S, it becomes an urgent state that every node must share."
render_line "scene07" "Now each rider sees more than one local sensor. They see the speed, environment, and safety state of the entire group."
render_line "scene08" "Three riders. One shared state. A system designed to leave together, and return together. Nostos."
