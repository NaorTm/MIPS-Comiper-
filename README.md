# MIPS Pipeline Simulator

A trace-driven, five-stage MIPS pipeline simulator written in C. It visualizes instruction movement through **fetch → decode → execute → memory → write-back**, inserts bubbles for selected data and control hazards, and reports cycles and CPI.

## Try it

Requirements: a C11 compiler such as GCC. Run commands from the repository root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -o mips mips.c
./mips 0 0 trace1.txt
./mips 1 1 trace2.txt
```

On Windows use `mips.exe`. The first flag toggles forwarding (`0` off, `1` on); the second selects branch handling (`0` for later resolution, `1` for earlier resolution). Omit the trace path to choose one of the two bundled traces interactively. Invalid flags, missing files, and malformed instructions return an error.

Each line contains a decimal address and an instruction, for example:

```text
1000 lw $1 100 $10
1004 add $3 $1 $2
1008 sw $3 300 $10
```

Supported operations: `add`, `sub`, `and`, `or`, `addi`, `subi`, `ori`, `andi`, `lw`, `sw`, `beq`, and `bneq`. Branch lines use a label as the final token. Registers are numbered 0–31. The input is an execution trace: branch outcomes are inferred from successive addresses, rather than executing an actual program.

## Tests

After compiling, run `python -m unittest discover -s tests -v`. The tests cover the CLI, invalid input, a single-instruction trace without a trailing newline, and a load-use example with and without forwarding. GitHub Actions also compiles with warnings treated as errors and runs both bundled traces.

## Scope and limitations

This is an educational timing model, not a complete MIPS processor or assembler. It does not calculate register values or memory contents, resolve branch labels, or implement jump instructions. Hazard and branch behavior are simplified; results should not be treated as a cycle-accurate reference for a particular CPU. CPI is total simulated cycles divided by trace instructions, including pipeline fill and drain.

See [detailed design notes](docs/DOCUMENTATION.md) for the original project documentation; where those notes disagree, the current code and this README describe the current interface.

