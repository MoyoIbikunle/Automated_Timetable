/*
 * Automated Staff Rota - starter (Session 1, Tue 13 Oct)
 * Plain C + Raylib. Opens a window and draws the empty 7 x 3 rota grid.
 * Build: make      Run: ./rota
 */
#include "raylib.h"
#include "rota.h"

int main(void)
{
    const int screenWidth = 1100;
    const int screenHeight = 500;

    const char *dayNames[DAYS] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    const char *shiftNames[SHIFTS] = { "Morning", "Afternoon", "Evening" };

    /* Grid layout */
    const int gridX = 260;      /* leave room on the left for the staff panel later */
    const int gridY = 80;
    const int cellW = 115;
    const int cellH = 110;
    const int labelW = 0;

    InitWindow(screenWidth, screenHeight, "Automated Staff Rota");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Automated Staff Rota", 20, 20, 28, DARKGRAY);

        /* Staff panel placeholder */
        DrawRectangleLines(20, gridY, 220, SHIFTS * cellH, GRAY);
        DrawText("Staff", 30, gridY + 10, 20, DARKGRAY);

        /* Day headers */
        for (int d = 0; d < DAYS; d++)
        {
            DrawText(dayNames[d], gridX + labelW + d * cellW + 40, gridY - 25, 20, DARKGRAY);
        }

        /* Cells */
        for (int s = 0; s < SHIFTS; s++)
        {
            for (int d = 0; d < DAYS; d++)
            {
                int x = gridX + labelW + d * cellW;
                int y = gridY + s * cellH;
                DrawRectangleLines(x, y, cellW, cellH, LIGHTGRAY);
                if (d == 0)
                {
                    DrawText(shiftNames[s], x + 5, y + 5, 14, GRAY);
                }
            }
        }

        DrawText("Press ESC to quit", 20, screenHeight - 30, 16, GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}