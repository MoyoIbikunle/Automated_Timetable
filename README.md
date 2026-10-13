# Automated Staff Rota

C + Raylib prototype that generates a weekly staff rota for a small café.

## Build and run (Ubuntu)

```
sudo apt install build-essential libraylib-dev
make
./rota
```

If `libraylib-dev` is not available on your Ubuntu version, build Raylib from source:
https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux

## Data

`staff.txt`: one line per person: `name manager(0/1) maxShifts` then 7 groups of 3 digits
(Mon to Sun; Morning, Afternoon, Evening), 1 = available.

## Rules

- Staff only work shifts they are available for
- At most one shift per day
- Never more than their weekly maximum
- Each shift needs a minimum number of staff