/*
 * Automated Staff Rota - main window
 *
 * The rota is built automatically; a manager only steps in if they want to.
 *
 *   1. Generate (button or G): builds the whole rota automatically
 *   2. Change a shift (optional): click a shift box, tick who works it,
 *      press Set & Generate. The shift is marked SET and Generate keeps it.
 *      Unlock hands it back to the generator.
 *   3. Change a person (optional): click a name, then
 *        - click boxes to mark when they're available (green = available)
 *        - buttons under the staff list: max shifts - / +, Manager, Rename, Delete, Done
 *      Hovering over a name highlights that person's shifts in the grid
 *      + Add staff: type a name; they start with no availability, so the app
 *        goes straight to editing them to mark when they can work
 *
 *   Rules (always on): staff only work when available, one shift a day,
 *   never over their max, rest between an evening and the next morning,
 *   and at least one manager on every shift.
 *
 *   Export CSV button:   writes the rota to rota.csv (opens in Excel)
 *   Save button:         writes the staff (staff.txt) and the rota with its SET
 *                        shifts (rota.txt); the saved rota opens next time
 *   Unlock all SET:      every shift back to automatic (also the C key)
 *   ? button:            shows the help screen again
 *   Closing with unsaved changes asks whether to save first
 *
 *   Keyboard shortcuts: G generate, C unlock all, + / - max shifts, M manager,
 *   Delete (twice) remove, Esc close / finish editing
 *
 * Build: make      Run: ./rota.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib.h"
#include "rota.h"

#define SCREEN_W 1100
#define SCREEN_H 610

#define GRID_X 260
#define GRID_Y 110
#define CELL_W 115
#define CELL_H 120

#define PANEL_X 20
#define PANEL_W 220
#define STAFF_FILE "staff.txt"
#define ROTA_FILE "rota.csv"        /* Export CSV (for Excel) */
#define ROTA_SAVE_FILE "rota.txt"   /* Save / reopen the rota */

#define POPUP_W 440
#define POPUP_ROW 26

static RotaData data;          /* static: too big to sit on the stack comfortably */
static int selected = -1;      /* index of staff being edited, -1 = none */
static int pendingDelete = -1; /* staff waiting for a second Delete */
static int unsaved = 0;        /* 1 if staff changed since last save */
static int showHelp = 1;       /* help screen shows when the app opens */
static int closeAsk = 0;       /* "Save before closing?" is showing */
static int closeNow = 0;       /* leave the main loop */

/* Add staff / rename form */
static int addOpen = 0;
static int renaming = 0;       /* 1 = the form renames the selected person instead of adding */
static char newName[NAME_LEN] = "";
static int newManager = 0;
static int newMax = 4;
static char addError[80] = "";

/* Shift list (popup) */
static int popupOpen = 0;
static int popupDay, popupShift;
static int ticked[MAX_STAFF];  /* 1 = ticked in the list (not applied until Set) */

static char status[200] = "";
static Color statusColour;
static double statusTime = 0;

/* Top bar */
static const Rectangle generateBtn = { 520, 22, 170, 36 };
static const Rectangle exportBtn = { 700, 22, 125, 36 };
static const Rectangle saveBtn = { 835, 22, 120, 36 };
static const Rectangle helpBtn = { 965, 22, 44, 36 };

/* Help screen */
static const Rectangle helpBox = { 170, 50, 760, 510 };
static const Rectangle gotItBtn = { 470, 505, 160, 40 };

/* Add staff form */
static const Rectangle addStaffBtn = { PANEL_X + 10, GRID_Y + SHIFTS * CELL_H - 44, 200, 32 };
static const Rectangle addBox = { 340, 150, 420, 280 };
static const Rectangle nameBox = { 365, 230, 370, 36 };
static const Rectangle managerTick = { 365, 288, 220, 24 };
static const Rectangle maxMinus = { 640, 285, 30, 30 };
static const Rectangle maxPlus = { 705, 285, 30, 30 };
static const Rectangle addOkBtn = { 365, 375, 170, 38 };
static const Rectangle addCancelBtn = { 565, 375, 170, 38 };

/* Save before closing? */
static const Rectangle closeBox = { 300, 200, 500, 200 };
static const Rectangle closeSaveBtn = { 320, 330, 150, 40 };
static const Rectangle closeDiscardBtn = { 480, 330, 150, 40 };
static const Rectangle closeCancelBtn = { 640, 330, 140, 40 };

static const Rectangle unlockAllBtn = { 905, GRID_Y + SHIFTS * CELL_H + 8, 160, 28 };

static void setStatus(const char *msg, Color colour)
{
    snprintf(status, sizeof(status), "%s", msg);
    statusColour = colour;
    statusTime = GetTime();
}

static int mouseOver(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

/* 1 when no help screen, shift list or form is covering the main window */
static int mainActive(void)
{
    return !popupOpen && !showHelp && !addOpen && !closeAsk;
}

/* Draws a button; returns 1 if clicked this frame (only when active) */
static int button(Rectangle r, const char *label, int active, Color fill)
{
    int hover = active && mouseOver(r);
    DrawRectangleRec(r, hover ? SKYBLUE : fill);
    DrawRectangleLinesEx(r, 1, GRAY);
    int size = MeasureText(label, 18) <= r.width - 8 ? 18 : 16;   /* smaller text if it wouldn't fit */
    int w = MeasureText(label, size);
    DrawText(label, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - size) / 2), size, active ? DARKGRAY : GRAY);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* ---------- Layout ---------- */

/* Staff rows shrink if there are many people, so the edit buttons still fit */
static int rowHeight(void)
{
    int space = SHIFTS * CELL_H - 38 - 125;
    return (data.staffCount > 0 && data.staffCount * 24 > space) ? space / data.staffCount : 24;
}

static Rectangle staffRow(int i)
{
    Rectangle r = { PANEL_X, GRID_Y + 38 + i * rowHeight(), PANEL_W, rowHeight() };
    return r;
}

static Rectangle cellRect(int day, int shift)
{
    Rectangle r = { GRID_X + day * CELL_W, GRID_Y + shift * CELL_H, CELL_W, CELL_H };
    return r;
}

/* Edit buttons under the staff list:
   0 = max -, 1 = max +, 2 = manager, 3 = delete, 4 = done, 5 = rename */
static Rectangle editButton(int n)
{
    float y = GRID_Y + SHIFTS * CELL_H - 118;
    Rectangle r[6] = {
        { PANEL_X + 120, y, 30, 30 },
        { PANEL_X + 180, y, 30, 30 },
        { PANEL_X + 10, y + 40, 120, 30 },
        { PANEL_X + 10, y + 80, 95, 30 },
        { PANEL_X + 115, y + 80, 95, 30 },
        { PANEL_X + 135, y + 40, 75, 30 },
    };
    return r[n];
}

static int anyLocked(void)
{
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            if (data.rota[d][sh].locked)
            {
                return 1;
            }
        }
    }
    return 0;
}

/* ---------- Actions ---------- */

static void regenerate(void)
{
    generateRota(&data);
    unsaved = 1;   /* the rota changed */
}

/* Generate, then say what's left and why */
static void generateAndExplain(void)
{
    regenerate();

    int missing = countUnderstaffed(&data);
    int noManager = countNoManager(&data);
    if (missing == 0 && noManager == 0)
    {
        setStatus("New rota generated - every shift covered", DARKGREEN);
        return;
    }
    if (noManager > 0)
    {
        /* Explain why a correct rota isn't possible with the staff as they are */
        int capacity, used;
        managerCapacity(&data, &capacity, &used);
        if (capacity < DAYS * SHIFTS)
        {
            setStatus(TextFormat("Not possible: managers can only work %d of %d shifts - need %d more "
                                 "(raise a manager's max or make someone a manager)",
                                 capacity, DAYS * SHIFTS, DAYS * SHIFTS - capacity), ORANGE);
            return;
        }
        setStatus(TextFormat("%d shift(s) without a manager - no manager is free then (check managers' availability)",
                             noManager), ORANGE);
        return;
    }
    setStatus(TextFormat("%d shift(s) understaffed - nobody else is free (check availability and max shifts)", missing), RED);
}

static void unlockEverything(void)
{
    unlockAll(&data);
    regenerate();
    setStatus("All shifts are automatic again", DARKBLUE);
}

static void changeMax(int delta)
{
    Staff *s = &data.staff[selected];
    if (s->maxShifts + delta < 0 || s->maxShifts + delta > DAYS)
    {
        return;
    }
    s->maxShifts += delta;
    unsaved = 1;
    regenerate();
    setStatus(TextFormat("%s can now work up to %d shifts a week", s->name, s->maxShifts), DARKBLUE);
}

static void toggleManager(void)
{
    Staff *s = &data.staff[selected];
    s->isManager = !s->isManager;
    unsaved = 1;
    regenerate();
    setStatus(TextFormat("%s is %s a manager", s->name, s->isManager ? "now" : "no longer"), DARKBLUE);
}

/* First press asks to confirm, second press deletes */
static void deletePressed(void)
{
    Staff *s = &data.staff[selected];
    if (pendingDelete != selected)
    {
        pendingDelete = selected;
        setStatus(TextFormat("Remove %s? Press Delete again to confirm (anything else cancels)", s->name), RED);
        return;
    }
    setStatus(TextFormat("Removed %s", s->name), DARKBLUE);
    deleteStaff(&data, selected);
    selected = -1;
    pendingDelete = -1;
    unsaved = 1;
    regenerate();
}

static void toggleAvailability(int d, int sh)
{
    int *a = &data.staff[selected].available[d][sh];
    *a = !*a;
    unsaved = 1;
    regenerate();
    setStatus(TextFormat("%s %s %s: %s", data.staff[selected].name, DAY_NAMES[d], SHIFT_NAMES[sh],
                         *a ? "available" : "not available"), DARKBLUE);
}

/* ---------- Add staff form ---------- */

static void openRenameForm(void)
{
    addOpen = 1;
    renaming = 1;
    snprintf(newName, sizeof(newName), "%s", data.staff[selected].name);
    addError[0] = '\0';
}

static void tryRename(void)
{
    char oldName[NAME_LEN];
    snprintf(oldName, sizeof(oldName), "%s", data.staff[selected].name);
    int result = renameStaff(&data, selected, newName);
    if (result == -2)
    {
        snprintf(addError, sizeof(addError), "Type a one-word name (letters, numbers, - or ')");
        return;
    }
    if (result == -3)
    {
        snprintf(addError, sizeof(addError), "Someone else is already called %s", newName);
        return;
    }
    addOpen = 0;
    renaming = 0;
    unsaved = 1;
    setStatus(TextFormat("Renamed %s to %s", oldName, newName), DARKBLUE);
}

static void openAddForm(void)
{
    addOpen = 1;
    renaming = 0;
    newName[0] = '\0';
    newManager = 0;
    newMax = 4;
    addError[0] = '\0';
}

static void tryAddStaff(void)
{
    if (renaming)
    {
        tryRename();
        return;
    }
    int idx = addStaff(&data, newName, newManager, newMax);
    if (idx == -1)
    {
        snprintf(addError, sizeof(addError), "The staff list is full (%d people max)", MAX_STAFF);
        return;
    }
    if (idx == -2)
    {
        snprintf(addError, sizeof(addError), "Type a one-word name (letters, numbers, - or ')");
        return;
    }
    if (idx == -3)
    {
        snprintf(addError, sizeof(addError), "Someone is already called %s", newName);
        return;
    }
    addOpen = 0;
    selected = idx;                  /* go straight to editing them */
    unsaved = 1;
    setStatus(TextFormat("Added %s - now click the boxes when %s can work, then Done", newName, newName), DARKGREEN);
}

static void handleAddInput(int key)
{
    /* Typing the name */
    int c = GetCharPressed();
    while (c > 0)
    {
        size_t len = strlen(newName);
        if (c < 128 && c != ' ' && len < NAME_LEN - 1)
        {
            newName[len] = (char)c;
            newName[len + 1] = '\0';
        }
        c = GetCharPressed();
    }
    if ((key == KEY_BACKSPACE || IsKeyPressedRepeat(KEY_BACKSPACE)) && newName[0] != '\0')
    {
        newName[strlen(newName) - 1] = '\0';
    }

    if (key == KEY_ESCAPE)
    {
        addOpen = 0;
        renaming = 0;
        return;
    }
    if (key == KEY_ENTER || key == KEY_KP_ENTER)
    {
        tryAddStaff();
        return;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }
    if (!renaming && mouseOver(managerTick))
    {
        newManager = !newManager;
    }
    if (!renaming && mouseOver(maxMinus) && newMax > 0)
    {
        newMax--;
    }
    if (!renaming && mouseOver(maxPlus) && newMax < DAYS)
    {
        newMax++;
    }
    if (mouseOver(addOkBtn))
    {
        tryAddStaff();
    }
    if (mouseOver(addCancelBtn))
    {
        addOpen = 0;
        renaming = 0;
    }
}

static void drawAddForm(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.35f));
    DrawRectangleRec(addBox, RAYWHITE);
    DrawRectangleLinesEx(addBox, 2, DARKGRAY);

    DrawText(renaming ? TextFormat("Rename %s", data.staff[selected].name) : "Add a staff member",
             (int)addBox.x + 25, (int)addBox.y + 20, 24, DARKGRAY);
    DrawText("Name (one word)", (int)nameBox.x, (int)nameBox.y - 22, 16, DARKGRAY);

    DrawRectangleRec(nameBox, WHITE);
    DrawRectangleLinesEx(nameBox, 2, DARKBLUE);
    DrawText(newName, (int)nameBox.x + 10, (int)nameBox.y + 9, 20, BLACK);
    if (((int)(GetTime() * 2)) % 2 == 0)   /* blinking cursor */
    {
        DrawText("_", (int)nameBox.x + 12 + MeasureText(newName, 20), (int)nameBox.y + 9, 20, DARKGRAY);
    }

    if (renaming)
    {
        DrawText("Their shifts and availability stay the same.",
                 (int)addBox.x + 25, (int)addBox.y + 190, 14, GRAY);
        if (addError[0] != '\0')
        {
            DrawText(addError, (int)addBox.x + 25, (int)addBox.y + 208, 16, RED);
        }
        button(addOkBtn, "Rename", 1, Fade(GREEN, 0.35f));
        button(addCancelBtn, "Cancel", 1, LIGHTGRAY);
        return;
    }

    Rectangle box = { managerTick.x, managerTick.y + 2, 20, 20 };
    DrawRectangleLinesEx(box, 2, DARKGRAY);
    if (newManager)
    {
        DrawRectangle((int)box.x + 5, (int)box.y + 5, 10, 10, DARKGREEN);
    }
    DrawText("Manager", (int)managerTick.x + 30, (int)managerTick.y + 3, 18, DARKBLUE);

    DrawText("Max shifts", (int)maxMinus.x - 105, (int)maxMinus.y + 7, 16, DARKGRAY);
    button(maxMinus, "-", 1, LIGHTGRAY);
    DrawText(TextFormat("%d", newMax), (int)maxMinus.x + 45, (int)maxMinus.y + 6, 18, DARKGRAY);
    button(maxPlus, "+", 1, LIGHTGRAY);

    DrawText("They start with no availability - you'll mark that next.",
             (int)addBox.x + 25, (int)addBox.y + 190, 14, GRAY);
    if (addError[0] != '\0')
    {
        DrawText(addError, (int)addBox.x + 25, (int)addBox.y + 208, 16, RED);
    }

    button(addOkBtn, "Add", 1, Fade(GREEN, 0.35f));
    button(addCancelBtn, "Cancel", 1, LIGHTGRAY);
}

/* ---------- Shift list ---------- */

static Rectangle popupRect(void)
{
    float h = 90 + data.staffCount * POPUP_ROW + 60;
    Rectangle r = { (SCREEN_W - POPUP_W) / 2.0f, (SCREEN_H - h) / 2.0f, POPUP_W, h };
    if (r.y < 10)
    {
        r.y = 10;
    }
    return r;
}

static Rectangle popupRow(int i)
{
    Rectangle p = popupRect();
    Rectangle r = { p.x + 10, p.y + 80 + i * POPUP_ROW, p.width - 20, POPUP_ROW };
    return r;
}

static Rectangle popupButton(int n)
{
    /* 0 = Set & Generate (wider), 1 = Unlock, 2 = Cancel */
    const float xs[3] = { 15, 185, 310 };
    const float ws[3] = { 160, 115, 115 };
    Rectangle p = popupRect();
    Rectangle r = { p.x + xs[n], p.y + p.height - 50, ws[n], 36 };
    return r;
}

static int tickedCount(void)
{
    int n = 0;
    for (int i = 0; i < data.staffCount; i++)
    {
        n += ticked[i];
    }
    return n;
}

static void openShiftList(int d, int sh)
{
    popupOpen = 1;
    popupDay = d;
    popupShift = sh;
    for (int i = 0; i < data.staffCount; i++)
    {
        ticked[i] = findOnShift(&data, i, d, sh) != -1;
    }
}

static void applyShiftList(void)
{
    int ids[MAX_PER_SHIFT];
    int n = 0;
    char reason[64];
    char warning[160] = "";

    for (int i = 0; i < data.staffCount && n < MAX_PER_SHIFT; i++)
    {
        if (!ticked[i])
        {
            continue;
        }
        ids[n++] = i;
        if (warning[0] == '\0' && shiftProblem(&data, i, popupDay, popupShift, reason, sizeof(reason)))
        {
            snprintf(warning, sizeof(warning), "Set, but %s: %s", data.staff[i].name, reason);
        }
    }

    setShift(&data, popupDay, popupShift, ids, n);
    regenerate();
    popupOpen = 0;

    if (warning[0] != '\0')
    {
        setStatus(warning, ORANGE);
    }
    else
    {
        setStatus(TextFormat("%s %s set by hand - the rest of the rota was rebuilt around it",
                             DAY_NAMES[popupDay], SHIFT_NAMES[popupShift]), DARKBLUE);
    }
}

static void handleShiftListInput(int key)
{
    if (key == KEY_ESCAPE)
    {
        popupOpen = 0;
        return;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    for (int i = 0; i < data.staffCount; i++)
    {
        if (mouseOver(popupRow(i)))
        {
            if (!ticked[i] && tickedCount() >= MAX_PER_SHIFT)
            {
                setStatus(TextFormat("A shift can have at most %d people", MAX_PER_SHIFT), ORANGE);
            }
            else
            {
                ticked[i] = !ticked[i];
            }
        }
    }

    if (mouseOver(popupButton(0)))
    {
        applyShiftList();
    }
    else if (data.rota[popupDay][popupShift].locked && mouseOver(popupButton(1)))
    {
        unlockShift(&data, popupDay, popupShift);
        regenerate();
        popupOpen = 0;
        setStatus(TextFormat("%s %s is automatic again", DAY_NAMES[popupDay], SHIFT_NAMES[popupShift]), DARKBLUE);
    }
    else if (mouseOver(popupButton(2)))
    {
        popupOpen = 0;
    }
}

static void drawShiftList(void)
{
    Rectangle p = popupRect();
    const Shift *shift = &data.rota[popupDay][popupShift];

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.35f));
    DrawRectangleRec(p, RAYWHITE);
    DrawRectangleLinesEx(p, 2, DARKGRAY);

    DrawText(TextFormat("%s %s - who works it?", DAY_NAMES[popupDay], SHIFT_NAMES[popupShift]),
             (int)p.x + 15, (int)p.y + 10, 22, DARKGRAY);
    const char *times = SHIFT_TIMES[popupShift];
    DrawText(times, (int)(p.x + p.width - 15 - MeasureText(times, 16)), (int)p.y + 48, 16, GRAY);

    /* How many ticked compared to how many needed */
    int n = tickedCount();
    int hasManager = 0;
    for (int i = 0; i < data.staffCount; i++)
    {
        hasManager |= ticked[i] && data.staff[i].isManager;
    }
    int managerMissing = data.requireManager && !hasManager;

    Color c = DARKGREEN;                      /* right number, rules met */
    if (n < shift->required)
    {
        c = RED;                              /* not enough people */
    }
    else if (n > shift->required || managerMissing)
    {
        c = ORANGE;                           /* too many, or no manager */
    }
    DrawText(TextFormat("%d of %d needed%s", n, shift->required,
                        !managerMissing ? "" : (popupShift == SHIFTS - 1 ? "  - must be a manager (closes up)"
                                                                         : "  - no manager ticked")),
             (int)p.x + 15, (int)p.y + 45, 18, c);

    char reason[64];
    for (int i = 0; i < data.staffCount; i++)
    {
        Rectangle r = popupRow(i);
        int problem = shiftProblem(&data, i, popupDay, popupShift, reason, sizeof(reason));

        if (mouseOver(r))
        {
            DrawRectangleRec(r, Fade(SKYBLUE, 0.3f));
        }

        Rectangle box = { r.x + 5, r.y + 4, 18, 18 };
        DrawRectangleLinesEx(box, 2, DARKGRAY);
        if (ticked[i])
        {
            DrawRectangle((int)box.x + 4, (int)box.y + 4, 10, 10, DARKGREEN);
        }

        Color nameColour = (problem && !ticked[i]) ? GRAY : (data.staff[i].isManager ? DARKBLUE : BLACK);
        DrawText(TextFormat("%s%s", data.staff[i].name, data.staff[i].isManager ? " (M)" : ""),
                 (int)r.x + 32, (int)r.y + 5, 18, nameColour);
        if (problem)
        {
            DrawText(reason, (int)r.x + 190, (int)r.y + 7, 14, ticked[i] ? ORANGE : GRAY);
        }
    }

    button(popupButton(0), "Set & Generate", 1, Fade(GREEN, 0.35f));
    if (shift->locked)
    {
        button(popupButton(1), "Unlock", 1, LIGHTGRAY);
    }
    button(popupButton(2), "Cancel", 1, LIGHTGRAY);
}

/* ---------- Help screen ---------- */

static void drawHelp(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.45f));
    DrawRectangleRec(helpBox, RAYWHITE);
    DrawRectangleLinesEx(helpBox, 2, DARKGRAY);

    int x = (int)helpBox.x + 30;
    int y = (int)helpBox.y + 25;
    DrawText("How it works", x, y, 28, DARKGRAY);
    y += 45;

    DrawText("1.  Press Generate", x, y, 20, DARKGREEN);
    DrawText("The whole rota is built automatically from everyone's availability and the rules.", x + 30, y + 26, 16, DARKGRAY);
    DrawText("Press it again any time for a different rota.", x + 30, y + 46, 16, DARKGRAY);
    y += 80;

    DrawText("2.  Change a shift  (optional)", x, y, 20, DARKBLUE);
    DrawText("Click a shift box, tick who should work it, then press Set & Generate.", x + 30, y + 26, 16, DARKGRAY);
    DrawText("It's marked SET and Generate keeps it. Unlock gives it back to the generator.", x + 30, y + 46, 16, DARKGRAY);
    y += 80;

    DrawText("3.  Change a person  (optional)", x, y, 20, DARKBLUE);
    DrawText("Click a name, then click boxes to mark when they're available (green = yes).", x + 30, y + 26, 16, DARKGRAY);
    DrawText("Buttons under the staff list change max shifts, manager, name, or remove them.", x + 30, y + 46, 16, DARKGRAY);
    y += 85;

    DrawText("Colours:", x, y, 18, DARKGRAY);
    DrawRectangle(x + 90, y, 16, 16, Fade(RED, 0.25f));
    DrawText("not enough staff", x + 112, y, 16, DARKGRAY);
    DrawRectangleLinesEx((Rectangle){ (float)(x + 270), (float)y, 16, 16 }, 2, ORANGE);
    DrawText("no manager on that shift", x + 292, y, 16, DARKGRAY);
    DrawText("SET", x + 540, y, 16, DARKBLUE);
    DrawText("= chosen by hand", x + 575, y, 16, DARKGRAY);
    y += 30;
    DrawText("Rules Generate always follows: staff only work when available, one shift a day,", x, y, 16, DARKGRAY);
    y += 20;
    DrawText("never over their max shifts, a rest after an evening, and a manager on every shift.", x, y, 16, DARKGRAY);
    y += 20;
    DrawText("The Evening shift has one person, so it's always a manager, who closes up.", x, y, 16, DARKGRAY);
    y += 26;
    DrawText("Save keeps the staff and this rota (with SET shifts) for next time.  Export CSV is for Excel.", x, y, 16, DARKGRAY);

    button(gotItBtn, "Got it", 1, Fade(GREEN, 0.35f));
}

/* ---------- Save before closing? ---------- */

static void handleCloseInput(int key)
{
    int clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (key == KEY_ESCAPE || (clicked && mouseOver(closeCancelBtn)))
    {
        closeAsk = 0;
        return;
    }
    if (clicked && mouseOver(closeSaveBtn))
    {
        if (saveStaff(STAFF_FILE, &data) && saveRota(ROTA_SAVE_FILE, &data))
        {
            closeNow = 1;
        }
        else
        {
            closeAsk = 0;
            setStatus("Could not save (is staff.txt or rota.txt open in another program?)", RED);
        }
    }
    if (clicked && mouseOver(closeDiscardBtn))
    {
        closeNow = 1;
    }
}

static void drawCloseAsk(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.35f));
    DrawRectangleRec(closeBox, RAYWHITE);
    DrawRectangleLinesEx(closeBox, 2, DARKGRAY);
    DrawText("Save before closing?", (int)closeBox.x + 25, (int)closeBox.y + 25, 24, DARKGRAY);
    DrawText("Changes to the staff or rota aren't saved yet.",
             (int)closeBox.x + 25, (int)closeBox.y + 70, 16, DARKGRAY);
    DrawText("If you close without saving, they'll be lost.", (int)closeBox.x + 25, (int)closeBox.y + 92, 16, DARKGRAY);
    button(closeSaveBtn, "Save & close", 1, Fade(GREEN, 0.35f));
    button(closeDiscardBtn, "Don't save", 1, Fade(RED, 0.25f));
    button(closeCancelBtn, "Cancel", 1, LIGHTGRAY);
}

/* ---------- Input ---------- */

static void handleInput(void)
{
    int key = GetKeyPressed();
    int clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (closeAsk)
    {
        handleCloseInput(key);
        return;
    }
    if (showHelp)
    {
        if (key == KEY_ESCAPE || key == KEY_ENTER || (clicked && mouseOver(gotItBtn)))
        {
            showHelp = 0;
        }
        return;
    }
    if (addOpen)
    {
        handleAddInput(key);
        return;
    }
    if (popupOpen)
    {
        handleShiftListInput(key);
        return;
    }

    /* Edit buttons under the staff list (checked first so the Delete
       button's second click confirms instead of cancelling) */
    if (clicked && selected != -1)
    {
        if (mouseOver(editButton(3)))
        {
            deletePressed();
            return;
        }
        if (mouseOver(editButton(0))) { changeMax(-1); }
        if (mouseOver(editButton(1))) { changeMax(+1); }
        if (mouseOver(editButton(2))) { toggleManager(); }
        if (mouseOver(editButton(5))) { openRenameForm(); return; }
        if (mouseOver(editButton(4))) { selected = -1; }
    }

    /* Anything other than Delete cancels a pending delete */
    if (pendingDelete != -1 && ((key != 0 && key != KEY_DELETE) || clicked))
    {
        pendingDelete = -1;
        setStatus("Delete cancelled", GRAY);
    }

    if (key == KEY_ESCAPE)
    {
        selected = -1;
    }
    if (key == KEY_G)
    {
        generateAndExplain();
    }
    if (key == KEY_C)
    {
        unlockEverything();
    }

    if (clicked)
    {
        if (anyLocked() && mouseOver(unlockAllBtn))
        {
            unlockEverything();
        }
        if (selected == -1 && mouseOver(addStaffBtn))
        {
            openAddForm();
            return;
        }

        /* Click a staff name to edit them / finish editing */
        for (int i = 0; i < data.staffCount; i++)
        {
            if (mouseOver(staffRow(i)))
            {
                selected = (selected == i) ? -1 : i;
            }
        }

        /* Click a box: availability if editing a person, otherwise the shift list */
        for (int d = 0; d < DAYS; d++)
        {
            for (int sh = 0; sh < SHIFTS; sh++)
            {
                if (!mouseOver(cellRect(d, sh)))
                {
                    continue;
                }
                if (selected != -1)
                {
                    toggleAvailability(d, sh);
                }
                else
                {
                    openShiftList(d, sh);
                }
            }
        }
    }

    if (selected == -1)
    {
        return;
    }
    if (key == KEY_EQUAL || key == KEY_KP_ADD)
    {
        changeMax(+1);
    }
    if (key == KEY_MINUS || key == KEY_KP_SUBTRACT)
    {
        changeMax(-1);
    }
    if (key == KEY_M)
    {
        toggleManager();
    }
    if (key == KEY_DELETE)
    {
        deletePressed();
    }
}

/* ---------- Drawing ---------- */

static void drawStaffPanel(void)
{
    DrawRectangleLines(PANEL_X, GRID_Y, PANEL_W, SHIFTS * CELL_H, GRAY);
    DrawText("Staff", PANEL_X + 10, GRID_Y + 10, 18, DARKGRAY);
    DrawText("shifts", PANEL_X + 150, GRID_Y + 12, 16, GRAY);

    int fontSize = rowHeight() >= 20 ? 16 : 10;   /* sizes the built-in font draws cleanly */
    for (int i = 0; i < data.staffCount; i++)
    {
        const Staff *s = &data.staff[i];
        Rectangle r = staffRow(i);

        if (i == selected)
        {
            DrawRectangleRec(r, Fade(GREEN, 0.3f));
        }
        else if (mainActive() && mouseOver(r))
        {
            DrawRectangleRec(r, Fade(SKYBLUE, 0.3f));
        }

        Color c = s->assigned > s->maxShifts ? ORANGE : (s->isManager ? DARKBLUE : DARKGRAY);
        DrawText(TextFormat("%s%s", s->name, s->isManager ? " (M)" : ""), (int)r.x + 10, (int)r.y + 3, fontSize, c);
        DrawText(TextFormat("%d / %d", s->assigned, s->maxShifts), (int)r.x + 150, (int)r.y + 3, fontSize, c);
    }

    int active = mainActive();
    if (selected == -1)
    {
        DrawText("Click a name to edit them", PANEL_X + 10, GRID_Y + SHIFTS * CELL_H - 68, 16, GRAY);
        button(addStaffBtn, "+ Add staff", active, LIGHTGRAY);
        return;
    }

    /* Edit buttons for the selected person (clicks handled in handleInput) */
    const Staff *s = &data.staff[selected];
    Rectangle minus = editButton(0);
    DrawText("Max shifts", PANEL_X + 10, (int)minus.y + 7, 16, DARKGRAY);
    button(editButton(0), "-", active, LIGHTGRAY);
    DrawText(TextFormat("%d", s->maxShifts), PANEL_X + 160, (int)minus.y + 6, 18, DARKGRAY);
    button(editButton(1), "+", active, LIGHTGRAY);
    button(editButton(2), s->isManager ? "Manager: Yes" : "Manager: No", active, LIGHTGRAY);
    button(editButton(5), "Rename", active, LIGHTGRAY);
    button(editButton(3), pendingDelete == selected ? "Confirm?" : "Delete", active,
           pendingDelete == selected ? Fade(RED, 0.4f) : LIGHTGRAY);
    button(editButton(4), "Done", active, Fade(GREEN, 0.35f));
}

/* The person whose name the mouse is over (when nobody is being edited), or -1 */
static int hoveredStaff(void)
{
    if (selected != -1 || !mainActive())
    {
        return -1;
    }
    for (int i = 0; i < data.staffCount; i++)
    {
        if (mouseOver(staffRow(i)))
        {
            return i;
        }
    }
    return -1;
}

static void drawGrid(void)
{
    int hover = hoveredStaff();

    for (int d = 0; d < DAYS; d++)
    {
        DrawText(DAY_NAMES[d], GRID_X + d * CELL_W + 40, GRID_Y - 25, 20, DARKGRAY);
    }

    for (int sh = 0; sh < SHIFTS; sh++)
    {
        for (int d = 0; d < DAYS; d++)
        {
            const Shift *shift = &data.rota[d][sh];
            Rectangle r = cellRect(d, sh);
            int x = (int)r.x;
            int y = (int)r.y;
            int understaffed = shift->count < shift->required;

            if (selected != -1)
            {
                /* Editing a person: green = available, grey = not */
                Color tint = data.staff[selected].available[d][sh] ? Fade(GREEN, 0.25f)
                                                                   : Fade(LIGHTGRAY, 0.6f);
                DrawRectangleRec(r, tint);
            }
            else if (understaffed)
            {
                DrawRectangleRec(r, Fade(RED, 0.25f));
            }
            if (hover != -1 && findOnShift(&data, hover, d, sh) != -1)
            {
                DrawRectangleRec(r, Fade(GOLD, 0.35f));   /* this person's shifts */
            }
            if (mainActive() && mouseOver(r))
            {
                DrawRectangleRec(r, Fade(SKYBLUE, 0.25f));
            }
            DrawRectangleLinesEx(r, 1, LIGHTGRAY);

            if (data.requireManager && !shiftHasManager(&data, d, sh))
            {
                Rectangle inner = { r.x + 2, r.y + 2, r.width - 4, r.height - 4 };
                DrawRectangleLinesEx(inner, 2, ORANGE);
                DrawText("no manager", x + 10, y + CELL_H - 20, 12, ORANGE);
            }

            /* Header: shift name with people on it / needed, and the times underneath */
            DrawText(SHIFT_NAMES[sh], x + 5, y + 4, 12, GRAY);
            const char *count = TextFormat("%d/%d", shift->count, shift->required);
            DrawText(count, x + CELL_W - 6 - MeasureText(count, 12), y + 4, 12, understaffed ? RED : GRAY);
            DrawText(SHIFT_TIMES[sh], x + 5, y + 18, 10, GRAY);
            if (shift->locked)
            {
                DrawText("SET", x + CELL_W - 30, y + CELL_H - 20, 14, DARKBLUE);
            }

            for (int i = 0; i < shift->count; i++)
            {
                int id = shift->staffIds[i];
                Color c = (id == selected) ? DARKGREEN : (id == hover ? DARKBLUE : BLACK);
                DrawText(data.staff[id].name, x + 10, y + 34 + i * 19, 16, c);
            }
        }
    }
}

static void drawKeyItem(int x, int y, Color colour, const char *label)
{
    DrawRectangle(x, y, 16, 16, colour);
    DrawRectangleLines(x, y, 16, 16, GRAY);
    DrawText(label, x + 22, y, 16, DARKGRAY);
}

static void drawKey(int y)
{
    if (selected == -1)
    {
        drawKeyItem(20, y, Fade(RED, 0.25f), "Not enough staff");
        int x = 200;
        DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, 16, 16 }, 2, ORANGE);
        DrawText("No manager", x + 22, y, 16, DARKGRAY);
        x += 140;
        DrawText("SET", x, y, 16, DARKBLUE);
        DrawText("= chosen by hand (Generate keeps it)", x + 38, y, 16, DARKGRAY);
        DrawText("(M) = manager", 800, y, 16, DARKBLUE);
    }
    else
    {
        drawKeyItem(20, y, Fade(GREEN, 0.25f), "Available");
        drawKeyItem(150, y, Fade(LIGHTGRAY, 0.6f), "Not available");
        DrawText("Orange count = over max shifts", 330, y, 16, ORANGE);
    }
}

/* One line at the bottom saying what clicking will do, based on the mouse */
static const char *hintText(void)
{
    if (showHelp)
    {
        return "";
    }
    if (closeAsk)
    {
        return "";
    }
    if (addOpen)
    {
        return renaming ? "Type the new one-word name, then Rename  (Esc to cancel)"
                        : "Type a one-word name, set Manager and max shifts, then Add  (Esc to cancel)";
    }
    if (popupOpen)
    {
        return "Tick who should work this shift, then press Set & Generate  (Cancel or Esc to leave it as it is)";
    }
    if (mouseOver(generateBtn)) return "Build a new rota automatically (SET shifts are kept)";
    if (mouseOver(exportBtn))   return "Save the rota as rota.csv to open in Excel";
    if (mouseOver(saveBtn))     return "Save the staff and this rota (with SET shifts) - it opens again next time";
    if (selected == -1 && mouseOver(addStaffBtn)) return "Add a new person to the staff list";
    if (mouseOver(helpBtn))     return "Show how to use the app";
    if (anyLocked() && mouseOver(unlockAllBtn)) return "Hand every SET shift back to the automatic generator";

    for (int i = 0; i < data.staffCount; i++)
    {
        if (mouseOver(staffRow(i)))
        {
            return i == selected ? TextFormat("Click to finish editing %s", data.staff[i].name)
                                 : TextFormat("%s's shifts are highlighted - click to edit them", data.staff[i].name);
        }
    }
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            if (!mouseOver(cellRect(d, sh)))
            {
                continue;
            }
            if (selected != -1)
            {
                return TextFormat("Click to mark %s as %s on %s %s", data.staff[selected].name,
                                  data.staff[selected].available[d][sh] ? "NOT available" : "available",
                                  DAY_NAMES[d], SHIFT_NAMES[sh]);
            }
            return TextFormat("Click to choose who works %s %s yourself (optional - Generate fills it automatically)",
                              DAY_NAMES[d], SHIFT_NAMES[sh]);
        }
    }
    if (selected != -1)
    {
        return TextFormat("Editing %s - click boxes for availability, use the buttons on the left, then Done",
                          data.staff[selected].name);
    }
    return "The rota is automatic - press Generate any time. Click a shift or a name only to change something.";
}

int main(void)
{
    srand((unsigned)time(NULL));

    if (loadStaff(STAFF_FILE, &data) <= 0)
    {
        printf("No staff loaded - check staff.txt is in the same folder\n");
        return 1;
    }
    setRequired(&data, 2, 2, 1);   /* Morning, Afternoon, Evening */
    data.requireManager = 1;       /* every shift needs a manager */
    if (loadRota(ROTA_SAVE_FILE, &data))
    {
        setStatus("Opened your saved rota - press Generate for a new one", DARKBLUE);
    }
    else
    {
        generateAndExplain();
    }
    unsaved = 0;

    InitWindow(SCREEN_W, SCREEN_H, "Automated Staff Rota");
    SetExitKey(KEY_NULL);          /* Esc closes things, it doesn't quit (use the window's X) */
    SetTargetFPS(60);

    while (!closeNow)
    {
        /* Closing the window: ask first if anything isn't saved */
        if (WindowShouldClose())
        {
            if (!unsaved)
            {
                break;
            }
            closeAsk = 1;
        }

        handleInput();

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Automated Staff Rota", 20, 25, 28, DARKGRAY);
        if (selected != -1)
        {
            DrawText(TextFormat("Editing: %s", data.staff[selected].name), 20, 66, 20, DARKGREEN);
            DrawText("click boxes to mark when they can work (green = available)", 260, 69, 16, DARKGREEN);
        }
        else
        {
            DrawText("Built automatically - change a shift or a person by hand only if you need to", 20, 69, 16, GRAY);
        }

        int active = mainActive();
        if (button(exportBtn, "Export CSV", active, LIGHTGRAY))
        {
            if (exportRotaCSV(ROTA_FILE, &data))
            {
                setStatus("Rota exported to rota.csv", DARKBLUE);
            }
            else
            {
                setStatus("Could not write rota.csv (is it open in Excel?)", RED);
            }
        }
        if (button(generateBtn, "Generate", active, Fade(GREEN, 0.35f)))
        {
            generateAndExplain();
        }
        if (button(saveBtn, unsaved ? "Save *" : "Save", active, LIGHTGRAY))
        {
            if (saveStaff(STAFF_FILE, &data) && saveRota(ROTA_SAVE_FILE, &data))
            {
                unsaved = 0;
                setStatus("Saved - staff and rota will open like this next time", DARKBLUE);
            }
            else
            {
                setStatus("Could not save (is staff.txt or rota.txt open in another program?)", RED);
            }
        }
        if (button(helpBtn, "?", active, LIGHTGRAY))
        {
            showHelp = 1;
        }

        drawStaffPanel();
        drawGrid();

        int bottom = GRID_Y + SHIFTS * CELL_H + 12;
        int missing = countUnderstaffed(&data);
        int noManager = countNoManager(&data);
        DrawText(missing == 0 ? "All shifts covered"
                              : TextFormat("%d shift(s) understaffed", missing),
                 20, bottom, 18, missing == 0 ? DARKGREEN : RED);
        DrawText(noManager == 0 ? "Every shift has a manager"
                                : TextFormat("%d shift(s) without a manager", noManager),
                 300, bottom, 18, noManager == 0 ? DARKGREEN : ORANGE);
        if (anyLocked())
        {
            button(unlockAllBtn, "Unlock all SET", active, LIGHTGRAY);   /* click handled in handleInput */
        }

        if (status[0] != '\0' && GetTime() - statusTime < 6.0)
        {
            DrawText(status, 20, bottom + 26, 18, statusColour);
        }

        drawKey(bottom + 56);
        DrawText(hintText(), 20, SCREEN_H - 28, 16, DARKBLUE);

        if (popupOpen)
        {
            drawShiftList();
        }
        if (addOpen)
        {
            drawAddForm();
        }
        if (showHelp)
        {
            drawHelp();
        }
        if (closeAsk)
        {
            drawCloseAsk();
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
