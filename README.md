#  cfs-scheduler-simulator

A simplified **Completely Fair Scheduler (CFS)** clone written in C.  
This project simulates Linux-style task scheduling using an **AVL tree** as the run queue and a **hash map** for managing sleeping tasks (wake map).

---

##  Project Structure

.
├── avl.c # AVL tree implementation (ordered by vmruntime)
├── avl.h
├── map.c # Hash map implementation (used for wake/sleep tracking)
├── map.h
├── main.c # CFS simulation driver
└── scheduler_input.txt # Example input file for events


---

##  Features

- **AVL Tree**  
  - Balanced binary search tree keyed by `vmruntime`.  
  - Provides `O(log n)` insertion, deletion, and min-finding (`avl_find_min`).

- **Hash Map**  
  - Used as a wake/sleep map (`wake_queue_task_map`).  
  - Stores tasks that are sleeping, keyed by PID.

- **CFS-inspired Scheduler**  
  - Simulates `START`, `SLEEP`, `WAKEUP`, and `EXIT` events.  
  - Each task runs for a time slice (`min_granularity` or based on load).  
  - Tracks `vmruntime` and `remaining_time` of each process.

- **Valgrind/ASan Tested**  
  - Verified for memory safety.  
  - No leaks or use-after-free in deterministic and randomized test runs.

---

##  Input Format

Each line in `scheduler_input.txt` describes a scheduling event:


- `time` — simulation time at which event occurs  
- `action` — one of `START`, `SLEEP`, `WAKEUP`, `EXIT`  
- `pid` — process ID (unique per task)  
- `duration` — runtime if `START`, otherwise ignored  

### Example:

0  START   1  30
0  START   2  40
5  SLEEP   1  0
8  START   3  25
10 WAKEUP  1  0
12 SLEEP   2  0
15 START   4  50
18 WAKEUP  2  0
20 SLEEP   3  0
22 WAKEUP  3  0
25 EXIT    1  0
28 SLEEP   4  0
30 START   5  35
32 WAKEUP  4  0
35 SLEEP   5  0
38 START   6  20
40 WAKEUP  5  0
42 EXIT    2  0
45 SLEEP   6  0
48 WAKEUP  6  0
50 EXIT    3  0
55 EXIT    4  0
60 SLEEP   5  0
62 WAKEUP  5  0
65 EXIT    5  0
70 EXIT    6  0


---

##  Usage

### Build

`gcc -fsanitize=address -g -o main main.c avl.c map.c`
`./main`

### Debug with Valgrind

`valgrind --leak-check=full --show-leak-kinds=all ./main`

Process event: 0 START 1 30
[TIME 0] PID=1 STARTED (runtime=30)
[TIME 0] PID=1 ran for 0 ms → new vruntime=0, remaining=30
Process event: 0 START 2 40
[TIME 0] PID=2 STARTED (runtime=40)
[TIME 5] PID=1 ran for 5 ms → new vruntime=5, remaining=25
Process event: 5 SLEEP 1 0
[TIME 8] PID=2 ran for 3 ms → new vruntime=3, remaining=37
Process event: 8 START 3 25
[TIME 8] PID=3 STARTED (runtime=25)
[TIME 10] PID=2 ran for 2 ms → new vruntime=5, remaining=35
Process event: 10 WAKEUP 1 0
[TIME 10] PID=1 WOKE UP (vruntime=5, remaining=25)
[TIME 12] PID=3 ran for 2 ms → new vruntime=5, remaining=23
Process event: 12 SLEEP 2 0
[TIME 15] PID=1 ran for 3 ms → new vruntime=8, remaining=22
Process event: 15 START 4 50
[TIME 15] PID=4 STARTED (runtime=50)
[TIME 18] PID=3 ran for 3 ms → new vruntime=8, remaining=20
Process event: 18 WAKEUP 2 0
[TIME 18] PID=2 WOKE UP (vruntime=5, remaining=23)
[TIME 20] PID=2 ran for 2 ms → new vruntime=7, remaining=21
Process event: 20 SLEEP 3 0
[TIME 22] PID=4 ran for 2 ms → new vruntime=7, remaining=48
Process event: 22 WAKEUP 3 0
[TIME 22] PID=3 WOKE UP (vruntime=8, remaining=20)
[TIME 25] PID=2 ran for 3 ms → new vruntime=10, remaining=18
Process event: 25 EXIT 1 0
[TIME 25] PID=1 EXITED
[TIME 28] PID=4 ran for 3 ms → new vruntime=10, remaining=45
Process event: 28 SLEEP 4 0
[TIME 30] PID=3 ran for 2 ms → new vruntime=10, remaining=18
....
