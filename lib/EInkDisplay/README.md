# EInkDisplay

Wrapper for dual-cadence e-ink updates:
- `Refill` mode: accepts 1s samples and coalesces to panel-safe refreshes.
- `Regular` mode: refreshes summary screen every 15 minutes.

Current implementation contains the scheduling/state API and serial render hooks.
To drive real hardware, connect `renderRefill()` and `renderRegular()` to the
Waveshare 2.9" V4 drawing calls.
