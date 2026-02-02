# Project 3: Multi-threaded Ticket Seller Simulation

## Overview
Simulates a concert ticket selling system with 10 sellers (1 High, 3 Medium, 6 Low price) using Pthreads. 100 seats, 60-minute simulation.

## Build & Run

```bash
make                # Compile
./proj3 <N>         # Run with N customers per seller
./proj3 10 > out.txt  # Save output
```

**Examples:**
```bash
./proj3 5    # 5 customers per seller (50 total)
./proj3 10   # 10 customers per seller (100 total)
./proj3 15   # 15 customers per seller (150 total)
```

## Output

**Events:** `[MM:SS] Seller XX: Customer YY <action>`
- `[00:05] Seller H1: Customer 01 arrives.`
- `[00:05] Seller H1: Customer 01 assigned seat.`
- `[00:07] Seller H1: Customer 01 leaves.`

**Seating Chart:** Displayed after each seat assignment
```
Seating Chart:
           1     2     3     4     5     6     7     8     9    10
    +------------------------------------------------------------+
   1|  H101     -     -     -     -     -     -     -     -     - |
   ...
```

**Final Report:** Statistics by seller type (H/M/L) and overall totals

## Seller Types
- **H1**: Row 1, service time 1-2 min
- **M1-M3**: Rows 5-6, service time 2-4 min
- **L1-L6**: Rows 8-10, service time 4-7 min

## Customer ID Format
- H sellers: H101, H102, ...
- M sellers: M101, M102, ... M201, M202, ... M301, M302, ...
- L sellers: L101, L102, ... L201, L202, ... L601, L602, ...
