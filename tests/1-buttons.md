# Test 1: Button Communication

## Expected behavior

| Input | Expected action | Expected LED | Expected audio |
| --- | --- | --- | --- |
| Button 1 | Pace Up | Green | `pace_up.mp3` |
| Button 2 | Pace Down | Yellow | `pace_down.mp3` |
| Button 3 | STOP! | Red | `stop.mp3` |
| Button 4 | Local output reset | Off | Stop immediately |

## Pass criteria

Press Buttons 1-3 once and confirm the action, LED color, and audio file match. While RGB, buzzer, or audio is active, press Button 4 and confirm all outputs stop and no pending local message plays afterward.
