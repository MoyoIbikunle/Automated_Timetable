# Automated Staff Rota

C + Raylib prototype that generates a weekly staff rota for a small café.

## Setup (Windows)

- Compiler: w64devkit extracted to `C:\w64devkit`
- Raylib 6.0 (`raylib-6.0_win64_mingw-w64.zip`) extracted to `C:\raylib`

## Data

`staff.txt`: one line per person: `name manager(0/1) maxShifts` then 7 groups of 3 digits
(Mon to Sun; Morning, Afternoon, Evening), 1 = available.

The sample file has 5 managers so that a fully correct rota exists (a manager on all 21 shifts needs managers who can work at least 21 shifts between them).

## The week

| Shift | Time | People needed |
| --- | --- | --- |
| Morning | 7:00-12:00 | 2 |
| Afternoon | 12:00-17:00 | 2 |
| Evening | 17:00-22:00 | 1, always a manager (closes up) |

## Rules

- Staff only work shifts they are available for
- At most one shift per day
- Never more than their weekly maximum
- Each shift needs a minimum number of staff
- Rest: nobody works an Evening followed by the next day's Morning
- Every shift needs at least one manager

All rules are always on. If the staff make a correct rota impossible (e.g. managers can't cover enough shifts), Generate still fills every shift it can and says exactly what's missing, e.g. "Not possible: managers can only work 15 of 21 shifts - need 6 more".

## Using the app

The rota is built automatically; a manager only steps in if they want to. A help screen explains this when the app opens (press **?** to see it again), and the line at the bottom of the window always says what clicking will do.

1. **Generate** (green button, or G): builds the whole rota automatically, following every rule. Press again any time for a different rota. With the sample staff, every rota it makes is fully correct
2. **Change a shift (optional)**: click a shift box, tick who should work it, press **Set & Generate**
   - the counter shows how many are ticked vs needed; names that would break a rule are greyed with the reason
   - the shift is marked **SET** and Generate keeps it; the rest of the rota is rebuilt around it
   - **Unlock** hands it back to the generator; **Unlock all SET** (or C) does every shift
   - **Cancel** (or Esc): close without changing anything
3. **Change a person (optional)**: click a name, then
   - click boxes to mark when they can work (green = available, grey = not)
   - buttons under the staff list: **Max shifts - / +**, **Manager: Yes/No**, **Delete** (click twice to confirm), **Done**
4. **Add staff**: click **+ Add staff** under the staff list, type a one-word name, tick Manager if needed, set max shifts, then **Add**. They start with no availability, so the app goes straight to editing them: click the boxes when they can work

Other buttons:
- **Export CSV**: writes the rota to `rota.csv` (opens in Excel)
- **Save**: writes the staff to `staff.txt` and the rota (including SET shifts) to `rota.txt`. Next time the app opens, it shows the saved rota; press Generate for a new one. `Save *` = unsaved changes

Shifts without a manager get an orange border; shifts without enough staff are red. After Generate, a message explains anything left over.

Keyboard shortcuts: G generate, C unlock all, + / - max shifts, M manager, Delete twice remove, Esc close or finish editing. Close the window with its X.

## How Generate works

It tries 200 random rotas and keeps the best: fewest understaffed shifts first, then fewest shifts without a manager. Ties between equally fair people are broken at random, so each press gives a different rota. The attempts mix two ideas:
- **hardest shifts first**: fill shifts with the fewest available people before the easy ones, so they aren't left with whoever is still free on Sunday
- **save managers**: once a shift has a manager, its other places go to non-managers, keeping managers for shifts that still need one

## Run (Windows, w64devkit terminal)

```
make            # builds rota.exe
./rota.exe      # opens the rota window (G = regenerate)
make test       # console tests: rules, manager rule, export, save/reload, add/delete staff, SET shifts, saved rota
```

## Files

| File | Purpose |
| --- | --- |
| `rota.h` | Structs, constants, function declarations |
| `data.c` | Loads, saves, adds and deletes staff (`staff.txt`); saves/reopens the rota (`rota.txt`); exports `rota.csv` |
| `scheduler.c` | Rule checks (`canWork`) and the generator |
| `main.c` | Raylib window: staff panel and rota grid |
| `test_rota.c` | Console test, no Raylib |

