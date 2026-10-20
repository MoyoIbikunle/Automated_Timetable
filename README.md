# Automated Staff Rota

C + Raylib prototype that generates a weekly staff rota for a small café.

## Setup (Windows)

- Compiler: w64devkit extracted to `C:\w64devkit`
- Raylib 6.0 (`raylib-6.0_win64_mingw-w64.zip`) extracted to `C:\raylib`

## Data

`staff.txt`: one line per person: `name manager(0/1) maxShifts` then 7 groups of 3 digits
(Mon to Sun; Morning, Afternoon, Evening), 1 = available.

## Rules

- Staff only work shifts they are available for
- At most one shift per day
- Never more than their weekly maximum
- Each shift needs a minimum number of staff

## Run (Windows, w64devkit terminal)

```
make            # builds rota.exe
./rota.exe      # opens the rota window (G = regenerate)
make test       # console test of loader + generator, checks every rule
```

## Files

| File | Purpose |
| --- | --- |
| `rota.h` | Structs, constants, function declarations |
| `data.c` | Loads `staff.txt` |
| `scheduler.c` | Rule checks (`canWork`) and the generator |
| `main.c` | Raylib window: staff panel and rota grid |
| `test_rota.c` | Console test, no Raylib |

