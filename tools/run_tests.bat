@echo off
rem PhyriadFG - run the test suite (4.3, 2026-09-06). The suite is CPU-only and takes ~1.5 s: R2's 148 seam checks,
rem R1's PhaseClock bit-parity oracle over a recorded arrival log, the registry's parity corpus (34 token
rem combinations), the pinned contract hash, the CLI's exit codes, and two negative tests (a clock log it cannot
rem open must FAIL; the layer generator must REFUSE a malformed row body).
rem
rem   tools\run_tests.bat            run everything
rem   tools\run_tests.bat -R clock   run the tests whose name matches
rem
rem Every one of these was confirmed to go RED under a deliberate perturbation before it was committed
rem (records/S4_3_GATE.md); a test whose failure has never been observed is a claim, not a gate.
ctest --test-dir "%~dp0..\build-release" --output-on-failure %*
rem Made with my soul - Swately <3
