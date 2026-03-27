# MIPS Pipeline Simulator

A trace-driven 5-stage MIPS pipeline simulator written in C. Given a sequence of MIPS instructions (a *trace file*), the simulator advances those instructions through the classic IF → ID → EX → MEM → WB pipeline, detects data and control hazards, and optionally inserts stalls or uses forwarding to resolve them. At the end it prints the per-cycle state of every pipeline stage and the overall **CPI** (Cycles Per Instruction).

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Pipeline Architecture](#2-pipeline-architecture)
3. [Supported Instructions](#3-supported-instructions)
4. [Control Signals](#4-control-signals)
5. [Hazard Handling Rules](#5-hazard-handling-rules)
6. [Input Format (Trace Files)](#6-input-format-trace-files)
7. [Output Format](#7-output-format)
8. [Build & Run](#8-build--run)
9. [Command-Line Options](#9-command-line-options)
10. [Project Structure](#10-project-structure)
11. [Detailed Function Reference](#11-detailed-function-reference)

---

## 1. Project Overview

The simulator reads a list of MIPS assembly instructions from a text file (one instruction per line, each prefixed by its memory address). It then replays those instructions through a 5-stage pipeline, tracking hazards and inserting **stalls** (pipeline bubbles) or using **data forwarding** as configured. After draining the pipeline it reports:

- The state of every pipeline stage at every clock cycle.
- The number of stall cycles counted at the WB stage.
- The final CPI.

---

## 2. Pipeline Architecture

The simulator models the classic **5-stage MIPS pipeline**:

| Stage | Index | Constant | Description |
|-------|-------|----------|-------------|
| Instruction Fetch | 0 | `IF` | Fetch the next instruction from the trace file |
| Instruction Decode / Register Read | 1 | `ID` | Decode the instruction and read source registers |
| Execute / ALU | 2 | `EX` | Perform the ALU operation or compute the effective address |
| Memory Access | 3 | `MEM` | Read from or write to data memory |
| Write Back | 4 | `WB` | Write the result back to the register file |

Each stage holds a `Command` struct (see [Function Reference](#11-detailed-function-reference)). Unused or flushed slots are filled with **bubble** (stall) instructions that carry all-zero control signals.

---

## 3. Supported Instructions

| Mnemonic | Type Code | Category |
|----------|-----------|----------|
| `add`    | `R`       | R-type arithmetic |
| `sub`    | `R`       | R-type arithmetic |
| `and`    | `R`       | R-type logical |
| `or`     | `R`       | R-type logical |
| `addi`   | `I`       | I-type ALU (immediate) |
| `subi`   | `I`       | I-type ALU (immediate) |
| `ori`    | `I`       | I-type logical (immediate) |
| `andi`   | `I`       | I-type logical (immediate) |
| `lw`     | `L`       | Load word |
| `sw`     | `S`       | Store word |
| `beq`    | `B`       | Branch if equal |
| `bneq`   | `B`       | Branch if not equal |
| `j`      | `J`       | Jump (parsing only; not fully simulated) |

The internal type codes (`R`, `I`, `L`, `S`, `B`, `J`, `0`) are used throughout the simulator to select the correct control signals and operand layout.

---

## 4. Control Signals

Every instruction carries a `Control` struct with nine 1-bit flags:

| Signal | Stages that use it | Meaning |
|--------|-------------------|---------|
| `RegDst` | EX | Select destination register (`rd` for R-type, `rt` for I-type) |
| `ALUOp0` | EX | ALU operation selector bit 0 |
| `ALUOp1` | EX | ALU operation selector bit 1 |
| `ALUSrc` | EX | ALU second operand source (`0` = register, `1` = immediate) |
| `Branch`  | MEM | This instruction is a branch |
| `MemRead` | MEM | Read from data memory |
| `MemWrite`| MEM | Write to data memory |
| `RegWrite`| WB | Write result back to a register |
| `MemToReg`| WB | Write-back source (`0` = ALU result, `1` = memory data) |

Pre-defined signal bundles used by each instruction category:

| Bundle | RegDst | ALUOp0 | ALUOp1 | ALUSrc | Branch | MemRead | MemWrite | RegWrite | MemToReg |
|--------|--------|--------|--------|--------|--------|---------|----------|----------|----------|
| `rControl` (R-type) | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| `iControl` (I-type ALU) | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 |
| `lControl` (lw) | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 1 | 1 |
| `sControl` (sw) | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| `bControl` (branch) | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| `stallControl` (bubble) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

---

## 5. Hazard Handling Rules

### 5.1 Data Hazards — No Forwarding (`forward = 0`)

The simulator checks two pairs of RAW (read-after-write) hazards per cycle:

1. **EX hazard** — the instruction in EX will write a register (`RegWrite = 1`) whose number matches a source operand of the instruction currently in ID.  
   → Insert **1 stall** cycle.

2. **MEM hazard** — same check but for the instruction in MEM.  
   → Insert **1 stall** cycle.

### 5.2 Data Hazards — With Forwarding (`forward = 1`)

Forwarding eliminates most RAW hazards. The only case that still requires a stall is a **load-use hazard**:

- The instruction in EX is a `lw` (`MemRead = 1`) and its destination register matches a source operand of the instruction in ID.  
  → Insert **1 stall** cycle.

### 5.3 Control Hazards (Branches)

Branch resolution is controlled by a second flag (`branch`):

| `branch` value | Resolution stage | Action on taken branch |
|----------------|-----------------|------------------------|
| `0` | MEM (default) | Flush 2 instructions already fetched (`flush1`: 2 bubbles inserted) |
| `1` | ID (early) | Insert 1 stall cycle before the branch completes |

Both modes check whether `pipe->stage[IF].address != pipe->stage[ID].address + 4` (i.e., the next fetched instruction is *not* the sequential successor of the branch, meaning the branch is taken).

---

## 6. Input Format (Trace Files)

Each line of a trace file represents one instruction and follows this pattern:

```
<address> <mnemonic> <operands...>
```

- **`<address>`** — decimal memory address (e.g., `1000`, `1004`, …). Addresses are assumed to be sequential and 4 bytes apart.
- **`<mnemonic>`** — one of the 13 supported instruction names.
- **`<operands>`** — register numbers prefixed with `$` and/or immediate values.

### Operand layouts by type

| Type | Layout | Example |
|------|--------|---------|
| R-type | `$rd $rs $rt` | `add $3 $2 $1` |
| I-type ALU | `$rt $rs imm` | `addi $10 $10 4` |
| Load (`lw`) | `$rt imm($rs)` style: `$rt imm $rs` | `lw $1 100 $10` |
| Store (`sw`) | `$rt imm $rs` | `sw $3 300 $10` |
| Branch | `$rs $rt label` | `bneq $10 $11 loop` |
| Jump | `label` | `j start` |

Register numbers are stored internally as their **binary representation** encoded as a decimal integer (e.g., register 3 → binary `11` → stored as integer `11`).

The file ends when `EOF` is reached; the simulator then drains the pipeline with 4 NOP (bubble) cycles.

---

## 7. Output Format

For every clock cycle the simulator prints:

```
Clock <N>:
Fetch instruction: <command string>
Decode instruction: <command string>
Execute instruction: <command string>
Memory instruction: <command string>
Writeback instruction: <command string>
```

Where `<command string>` is the raw text of the instruction as it appeared in the trace file, or `stall` for a pipeline bubble, or `No Command` for empty slots at startup/drain.

At the very end:

```
CPI: <value>
```

CPI is calculated as `total_cycles / (total_cycles - stall_cycles - 4)`, where the `- 4` accounts for the drain NOPs added at the end of the trace.

---

## 8. Build & Run

### Prerequisites
- A C compiler (GCC, Clang, or MSVC).
- `trace1.txt` and/or `trace2.txt` in the same directory as the executable.

### Build

```sh
gcc mips.c -o mips
```

### Run

```sh
./mips <forward> <branch>
```

When prompted, enter `1` to use `trace1.txt` or `2` to use `trace2.txt`.

> **Note:** The current `main` function contains a bug where `argv[1]` / `argv[2]` (strings) are assigned directly to the integer fields `pipe.forward` / `pipe.branch`. Until that is fixed, pass `0 0` as arguments and hard-code the desired modes inside the source, or patch `main` to call `atoi(argv[1])`.

---

## 9. Command-Line Options

| Argument | Values | Meaning |
|----------|--------|---------|
| `argv[1]` (forward) | `0` | No data forwarding — stall on every RAW hazard |
| | `1` | Data forwarding enabled — only load-use hazards cause a stall |
| `argv[2]` (branch) | `0` | Branch resolved in MEM stage — flush 2 instructions on taken branch |
| | `1` | Branch resolved in ID stage — stall 1 cycle on taken branch |

---

## 10. Project Structure

```
MIPS-Comiper-/
├── mips.c          # Full simulator source
├── trace1.txt      # Sample trace: loop with lw/add/sw/addi/bneq
├── trace2.txt      # Sample trace: loop with load-use and store hazards
├── README.md       # This file
└── docs/
    └── DOCUMENTATION.md  # In-depth function and data-structure reference
```

---

## 11. Detailed Function Reference

See [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) for a complete description of every struct, global variable, and function in `mips.c`.

