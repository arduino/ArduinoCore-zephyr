# Manual API test sketches

One sketch per Arduino API group from `../../api_support_tracking.md`.
Each sketch lives in its own folder matching its `.ino` name, e.g.:

    tests/manual_tests/digital_io/digital_io.ino
    tests/manual_tests/analog_io/analog_io.ino

Compile/upload with:

    arduino-cli compile --fqbn arduino-git:zephyr:kit_pse84_ai tests/manual_tests/<name>
    arduino-cli upload  -p <port> --fqbn arduino-git:zephyr:kit_pse84_ai tests/manual_tests/<name>
