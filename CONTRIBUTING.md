# Contributing

Use a focused change and include its tests and limitations. Run the native/Python checks in [BUILDING](docs/BUILDING.md); firmware changes should compile for both C6 and H2 against the pinned core. CI checks compilation and simulated protocol behavior, not RF reception or certification.

Never include network keys, credentials, coordinator dumps, device databases, serial payload logs or personal screenshots in a contribution. Use synthetic addresses and payloads in fixtures. Describe hardware/host versions, expected versus observed behavior, and a minimal reproduction without private state.

Treat an installed network as persistent user data. Keep commissioning, security, transport and persistence changes separate from diagnostic work. Obtain the operator's approval before live service interruption or flashing; preserve the working application and relevant private host/network state. Do not erase or recommission a working network to make a regression test pass.

Mark hardware checks PASS, FAIL or NOT RUN. A passing simulated host test does not establish device-family coverage or full TI/Zigbee equivalence. Use [the acceptance plan](docs/GO_FORWARD.md) for broader testing. Contributions are submitted under the project's MIT license; third-party dependencies keep their own licenses.
