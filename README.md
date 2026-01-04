# MIPS Pipeline Simulator (C)

Simulates a 5-stage MIPS pipeline using instruction traces, including stalls, forwarding options, and branch handling. Prints per-cycle pipeline state and CPI.

## Features
- 5-stage pipeline model (IF/ID/EX/MEM/WB)
- Forwarding and branch mode flags
- Trace-driven execution (`trace1.txt`, `trace2.txt`)
- CPI calculation

## Build and Run
```sh
gcc mips.c -o mips
mips.exe 0 0
```

Then choose a trace file when prompted.
