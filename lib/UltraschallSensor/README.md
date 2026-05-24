# UltraschallSensor Library

## Architecture Decision

### Decision
Use a local, in-repo `A02yyuwFrameParser` implementation for the A02YYUW UART protocol instead of depending on a third-party Arduino sensor library.

### Why this decision
- The protocol surface is very small (4-byte frame with fixed header and checksum), so a dedicated parser stays compact and easy to review.
- The parser is plain C++ and independent of Arduino runtime APIs, which allows fast host-side unit tests in `test/test_ultraschall/test_ultraschall_parser.cpp`.
- Keeping parsing logic in-repo avoids external dependency management, version drift, and API changes from third-party libraries.
- This project's runtime behavior includes board-specific power control and UART lifecycle handling (`powerOn`, `powerOff`, settle time, serial buffer drain). A generic library would still need local wrapping for these concerns.
- Local ownership of parser behavior makes failure handling and resynchronization rules explicit and directly testable in this codebase.

### Consequences
- We maintain a small amount of protocol code ourselves.
- If sensor protocol requirements change, we must update parser and tests in this repo.
- In exchange, behavior is deterministic, test coverage is straightforward, and integration remains tightly aligned with project-specific hardware control.
