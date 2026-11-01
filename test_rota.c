/*
 * test_rota.c - console tests for the loader, generator, delete and save
 * (no Raylib needed). Build and run: make test
 */
#include <stdio.h>
#include <string.h>
#include "rota.h"

static int errors = 0;

static void check(int ok, const char *what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        errors++;
    }
}

/* Checks every hard rule holds for the current rota */
static void checkRules(const RotaData *data)
{
    for (int d = 0; d < DAYS; d++)
    {
        int seen[MAX_STAFF] = { 0 };
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            const Shift *s = &data->rota[d][sh];
            for (int i = 0; i < s->count; i++)
            {
                int id = s->staffIds[i];
                check(data->staff[id].available[d][sh], "staff only work shifts they are available for");
                check(seen[id]++ == 0, "at most one shift per day");
            }
        }
    }
    for (int i = 0; i < data->staffCount; i++)
    {
        check(data->staff[i].assigned <= data->staff[i].maxShifts, "never over weekly max");
        for (int d = 0; d < DAYS - 1; d++)
        {
            check(!(findOnShift(data, i, d, SHIFTS - 1) != -1 && findOnShift(data, i, d + 1, 0) != -1),
                  "rest: no Evening followed by next-day Morning");
        }
    }
}

int main(void)
{
    static RotaData data;

    if (loadStaff("staff.txt", &data) <= 0)
    {
        return 1;
    }
    printStaff(&data);

    setRequired(&data, 2, 2, 1);
    generateRota(&data);
    printf("\n");
    printRota(&data);

    /* Generation is random, so check the rules over many runs */
    for (int run = 0; run < 100; run++)
    {
        generateRota(&data);
        checkRules(&data);
    }
    printf("\nRules checked over 100 generated rotas\n");

    /* Manager rule: switching it on should give fewer shifts without a manager */
    int withoutRule = 0, withRule = 0;
    for (int run = 0; run < 20; run++)
    {
        data.requireManager = 0;
        generateRota(&data);
        data.requireManager = 1;          /* count as if the rule were on */
        withoutRule += countNoManager(&data);
        generateRota(&data);
        withRule += countNoManager(&data);
        checkRules(&data);
    }
    check(withRule < withoutRule, "manager rule reduces shifts without a manager");

    /* With the sample staff every shift can be covered, and managers can cover
       at most (their max shifts added up) shifts - the generator should reach both */
    int capacity, used;
    managerCapacity(&data, &capacity, &used);
    int bestPossible = DAYS * SHIFTS - capacity > 0 ? DAYS * SHIFTS - capacity : 0;
    for (int run = 0; run < 20; run++)
    {
        data.requireManager = 0;
        generateRota(&data);
        check(countUnderstaffed(&data) == 0, "hardest-first covers every shift");
        data.requireManager = 1;
        generateRota(&data);
        check(countNoManager(&data) == bestPossible, "managers never wasted (best possible reached)");
    }
    printf("Best possible coverage checked (no-manager minimum %d)\n", bestPossible);
    printf("Manager rule checked (no-manager shifts over 20 runs: %d off, %d on)\n", withoutRule, withRule);
    data.requireManager = 0;

    /* Export: file has a header row, 3 shift rows, a blank line, a staff header and one row per person */
    check(exportRotaCSV("test_rota.csv", &data), "rota.csv written");
    FILE *csv = fopen("test_rota.csv", "r");
    int lines = 0;
    char buf[512];
    while (csv != NULL && fgets(buf, sizeof(buf), csv) != NULL)
    {
        lines++;
    }
    if (csv != NULL)
    {
        fclose(csv);
    }
    check(lines == 1 + SHIFTS + 1 + 1 + data.staffCount, "rota.csv has the right number of lines");
    remove("test_rota.csv");
    printf("Export checked\n");

    /* Save then reload: everything should come back the same */
    static RotaData copy;
    check(saveStaff("test_save.txt", &data), "save file written");
    check(loadStaff("test_save.txt", &copy) == data.staffCount, "same number of staff after reload");
    for (int i = 0; i < data.staffCount; i++)
    {
        check(strcmp(copy.staff[i].name, data.staff[i].name) == 0, "name survives save/reload");
        check(copy.staff[i].maxShifts == data.staff[i].maxShifts, "max shifts survive save/reload");
        check(memcmp(copy.staff[i].available, data.staff[i].available,
                     sizeof(data.staff[i].available)) == 0, "availability survives save/reload");
    }
    remove("test_save.txt");
    printf("Save/reload checked\n");

    /* Delete the first person: count drops, next person moves up, rules still hold */
    char second[NAME_LEN];
    strcpy(second, data.staff[1].name);
    int before = data.staffCount;
    deleteStaff(&data, 0);
    check(data.staffCount == before - 1, "delete reduces staff count");
    check(strcmp(data.staff[0].name, second) == 0, "staff after deleted one move up");
    generateRota(&data);
    checkRules(&data);
    printf("Delete checked\n");

    /* SET a shift by hand (swap one person): Generate keeps it exactly as set */
    generateRota(&data);
    int day = -1, sh = -1, keep = -1, swapIn = -1;
    for (int d = 0; d < DAYS && day == -1; d++)
    {
        for (int t = 0; t < SHIFTS && day == -1; t++)
        {
            int k = -1, in = -1;
            for (int i = 1; i < data.staffCount; i++)   /* skip index 0 so the delete check below works */
            {
                if (findOnShift(&data, i, d, t) != -1)
                {
                    if (k == -1) k = i;
                }
                else if (in == -1 && data.staff[i].available[d][t])
                {
                    in = i;
                }
            }
            if (k != -1 && in != -1)
            {
                day = d; sh = t; keep = k; swapIn = in;
            }
        }
    }
    if (day == -1)
    {
        printf("FAIL: test data has no shift where a swap is possible\n");
        return 1;
    }
    int chosen[2] = { keep, swapIn };
    setShift(&data, day, sh, chosen, 2);
    for (int run = 0; run < 50; run++)
    {
        generateRota(&data);
        const Shift *s = &data.rota[day][sh];
        check(s->locked && s->count == 2 && findOnShift(&data, keep, day, sh) != -1 &&
              findOnShift(&data, swapIn, day, sh) != -1, "SET shift kept exactly after Generate");
        checkRules(&data);
    }
    printf("Set shift checked\n");

    /* Warnings in the shift list */
    char reason[64];
    int unavailable = -1;
    for (int i = 0; i < data.staffCount && unavailable == -1; i++)
    {
        if (!data.staff[i].available[day][sh])
        {
            unavailable = i;
        }
    }
    if (unavailable != -1)
    {
        check(shiftProblem(&data, unavailable, day, sh, reason, sizeof(reason)), "warns when not available");
    }
    int other = (sh + 1) % SHIFTS;
    {
        check(shiftProblem(&data, swapIn, day, other, reason, sizeof(reason)),
              "warns when already SET on another shift that day");
    }
    printf("Shift list warnings checked\n");

    /* Deleting someone earlier in the list keeps the SET shift on the right people */
    char keepName[NAME_LEN], swapName[NAME_LEN];
    strcpy(keepName, data.staff[keep].name);
    strcpy(swapName, data.staff[swapIn].name);
    deleteStaff(&data, 0);
    generateRota(&data);
    check(strcmp(data.staff[data.rota[day][sh].staffIds[0]].name, keepName) == 0 &&
          strcmp(data.staff[data.rota[day][sh].staffIds[1]].name, swapName) == 0,
          "SET shift follows the right people after a delete");
    printf("Delete with a SET shift checked\n");

    /* Unlock hands the shift back to Generate; unlockAll does every shift */
    unlockShift(&data, day, sh);
    check(!data.rota[day][sh].locked, "unlock one shift");
    setShift(&data, (day + 1) % DAYS, sh, chosen, 1);
    unlockAll(&data);
    int anyLocked = 0;
    for (int d = 0; d < DAYS; d++)
    {
        for (int s2 = 0; s2 < SHIFTS; s2++)
        {
            anyLocked |= data.rota[d][s2].locked;
        }
    }
    check(!anyLocked, "unlock all");
    generateRota(&data);
    checkRules(&data);
    printf("Unlock checked\n");

    /* Add staff: good name works, bad or duplicate names are refused,
       and a new person isn't given shifts until they have availability */
    int before2 = data.staffCount;
    int added = addStaff(&data, "Ola", 1, 4);
    check(added == before2 && data.staffCount == before2 + 1, "add staff");
    check(addStaff(&data, "Ola", 0, 3) == -3, "duplicate name refused");
    check(addStaff(&data, "Two words", 0, 3) == -2, "name with a space refused");
    check(addStaff(&data, "", 0, 3) == -2, "empty name refused");
    generateRota(&data);
    check(data.staff[added].assigned == 0, "new person gets no shifts until they have availability");
    data.staff[added].available[2][2] = 1;   /* free Wednesday evening */
    for (int run = 0; run < 20; run++)
    {
        generateRota(&data);
        checkRules(&data);
    }
    printf("Add staff checked\n");

    /* Save the rota (with a SET shift) and load it back: same people, same SET marks */
    int pick[1] = { data.rota[4][1].staffIds[0] };
    setShift(&data, 4, 1, pick, 1);
    generateRota(&data);
    static RotaData before3;
    before3 = data;
    check(saveRota("test_rota.txt", &data), "rota saved");
    for (int d = 0; d < DAYS; d++)
    {
        for (int t = 0; t < SHIFTS; t++)
        {
            data.rota[d][t].count = 0;
            data.rota[d][t].locked = 0;
        }
    }
    check(loadRota("test_rota.txt", &data), "rota loaded");
    for (int d = 0; d < DAYS; d++)
    {
        for (int t = 0; t < SHIFTS; t++)
        {
            check(data.rota[d][t].count == before3.rota[d][t].count &&
                  data.rota[d][t].locked == before3.rota[d][t].locked &&
                  memcmp(data.rota[d][t].staffIds, before3.rota[d][t].staffIds,
                         sizeof(int) * data.rota[d][t].count) == 0, "rota the same after save/load");
        }
    }
    for (int i = 0; i < data.staffCount; i++)
    {
        check(data.staff[i].assigned == before3.staff[i].assigned, "shift counts the same after load");
    }
    /* A saved rota still loads after someone is deleted: they're just skipped */
    deleteStaff(&data, data.staffCount - 1);
    check(loadRota("test_rota.txt", &data), "rota loads after a delete");
    remove("test_rota.txt");
    check(!loadRota("no_such_file.txt", &data), "no saved rota = nothing loaded");
    printf("Save and reopen rota checked\n");

    /* Rename: works, keeps their shifts, refuses bad or taken names, and the
       saved rota uses the new name */
    generateRota(&data);
    int who = data.rota[0][0].staffIds[0];
    int shiftsBefore = data.staff[who].assigned;
    check(renameStaff(&data, who, "Renamed") == 0 && strcmp(data.staff[who].name, "Renamed") == 0, "rename");
    check(data.staff[who].assigned == shiftsBefore && findOnShift(&data, who, 0, 0) != -1, "rename keeps their shifts");
    check(renameStaff(&data, who, "Renamed") == 0, "renaming to the same name is fine");
    check(renameStaff(&data, who, data.staff[(who + 1) % data.staffCount].name) == -3, "rename to someone else's name refused");
    check(renameStaff(&data, who, "Two words") == -2, "rename with a space refused");
    saveRota("test_rename.txt", &data);
    check(loadRota("test_rename.txt", &data) && findOnShift(&data, who, 0, 0) != -1, "saved rota uses the new name");
    remove("test_rename.txt");
    printf("Rename checked\n");

    printf(errors == 0 ? "\nAll checks passed\n" : "\n%d checks failed\n", errors);
    return errors != 0;
}
