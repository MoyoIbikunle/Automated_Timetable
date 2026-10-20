/*
 * test_rota.c - console test for the loader and generator (no Raylib)
 * Build and run: make test
 */
#include <stdio.h>
#include "rota.h"

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

    /* Check every rule held */
    int errors = 0;
    for (int d = 0; d < DAYS; d++)
    {
        int seen[MAX_STAFF] = { 0 };
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            const Shift *s = &data.rota[d][sh];
            for (int i = 0; i < s->count; i++)
            {
                int id = s->staffIds[i];
                if (!data.staff[id].available[d][sh])
                {
                    printf("FAIL: %s not available %s %s\n", data.staff[id].name, DAY_NAMES[d], SHIFT_NAMES[sh]);
                    errors++;
                }
                if (seen[id]++)
                {
                    printf("FAIL: %s works twice on %s\n", data.staff[id].name, DAY_NAMES[d]);
                    errors++;
                }
            }
        }
    }
    for (int i = 0; i < data.staffCount; i++)
    {
        if (data.staff[i].assigned > data.staff[i].maxShifts)
        {
            printf("FAIL: %s over max shifts\n", data.staff[i].name);
            errors++;
        }
    }
    printf(errors == 0 ? "\nAll rule checks passed\n" : "\n%d rule checks failed\n", errors);
    return errors != 0;
}
