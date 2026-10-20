/*
 * Automated Staff Rota - main window
 * Loads staff.txt, generates the rota and draws it as a 7 x 3 grid.
 * Understaffed shifts are shown in red. Press G to regenerate.
 * Build: make      Run: ./rota.exe
 */
#include <stdio.h>
#include "raylib.h"
#include "rota.h"

#define SCREEN_W 1100
#define SCREEN_H 520

#define GRID_X 260
#define GRID_Y 80
#define CELL_W 115
#define CELL_H 120

static RotaData data;   /* static: too big to sit on the stack comfortably */

static void drawStaffPanel(void)
{
    DrawRectangleLines(20, GRID_Y, 220, SHIFTS * CELL_H, GRAY);
    DrawText("Staff   shifts", 30, GRID_Y + 10, 18, DARKGRAY);

    for (int i = 0; i < data.staffCount; i++)
    {
        const Staff *s = &data.staff[i];
        int y = GRID_Y + 40 + i * 24;
        Color c = s->isManager ? DARKBLUE : DARKGRAY;
        DrawText(TextFormat("%s%s", s->name, s->isManager ? " (M)" : ""), 30, y, 16, c);
        DrawText(TextFormat("%d / %d", s->assigned, s->maxShifts), 170, y, 16, c);
    }
}

static void drawGrid(void)
{
    for (int d = 0; d < DAYS; d++)
    {
        DrawText(DAY_NAMES[d], GRID_X + d * CELL_W + 40, GRID_Y - 25, 20, DARKGRAY);
    }

    for (int sh = 0; sh < SHIFTS; sh++)
    {
        for (int d = 0; d < DAYS; d++)
        {
            const Shift *shift = &data.rota[d][sh];
            int x = GRID_X + d * CELL_W;
            int y = GRID_Y + sh * CELL_H;

            if (shift->count < shift->required)
            {
                DrawRectangle(x, y, CELL_W, CELL_H, Fade(RED, 0.25f));
            }
            DrawRectangleLines(x, y, CELL_W, CELL_H, LIGHTGRAY);

            DrawText(TextFormat("%s %d/%d", SHIFT_NAMES[sh], shift->count, shift->required),
                     x + 5, y + 5, 12, GRAY);

            for (int i = 0; i < shift->count; i++)
            {
                DrawText(data.staff[shift->staffIds[i]].name, x + 10, y + 28 + i * 20, 16, BLACK);
            }
        }
    }
}

int main(void)
{
    if (loadStaff("staff.txt", &data) <= 0)
    {
        printf("No staff loaded - check staff.txt is in the same folder\n");
        return 1;
    }
    setRequired(&data, 2, 2, 1);   /* Morning, Afternoon, Evening */
    generateRota(&data);

    InitWindow(SCREEN_W, SCREEN_H, "Automated Staff Rota");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_G))
        {
            generateRota(&data);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Automated Staff Rota", 20, 20, 28, DARKGRAY);
        drawStaffPanel();
        drawGrid();

        int missing = countUnderstaffed(&data);
        DrawText(missing == 0 ? "All shifts covered"
                              : TextFormat("%d shift(s) understaffed (red)", missing),
                 GRID_X, GRID_Y + SHIFTS * CELL_H + 15, 18, missing == 0 ? DARKGREEN : RED);
        DrawText("G = regenerate   ESC = quit", 20, SCREEN_H - 30, 16, GRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
