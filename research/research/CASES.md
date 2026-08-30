# NOSTOS Test Cases

Start with mock messages. Test real boards later.

## Normal cases

| ID | Scenario | Expected result |
| --- | --- | --- |
| C01 | Send each message type once | The same type and value reach the receiver. |
| C02 | Send two messages together | Both messages arrive in the same order. |
| C03 | Send one message in small parts | The receiver waits and rebuilds the message. |
| C04 | Send data with minimum and maximum valid values | The receiver accepts the values. |

Message groups: button requests, rear status, fall/SOS, sensor data, heartbeat, and ACK.

## Error cases

| ID | Scenario | Expected result |
| --- | --- | --- |
| C05 | Send a message with a wrong length | It is rejected. |
| C06 | Send a message with a wrong version or unknown type | It is rejected. |
| C07 | Send a message with a bad CRC in protocol v2 | It is rejected. |
| C08 | Send the same protocol v2 message twice | It does not cause the same action twice. |
| C09 | Send a message from an unknown node | It is rejected. |

## Load and recovery cases

| ID | Scenario | Expected result |
| --- | --- | --- |
| C10 | Fill the message queue | The system reports full and does not crash. |
| C11 | Fill the normal queue, then send FALL or SOS in protocol v2 | The urgent message still gets a place. |
| C12 | Send while Mesh is not ready | The message is not sent as if it succeeded. |
| C13 | Send a valid message after a broken message | The valid message works normally. |
| C14 | Restart the bridge, then send a new message | The bridge starts cleanly and handles the message. |

Record each result as `PASS`, `FAIL`, or `BLOCKED`.
