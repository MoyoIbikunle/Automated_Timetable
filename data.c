/*
 * data.c - loading staff from a text file
 *
 * staff.txt format, one person per line:
 *   name isManager maxShifts MON TUE WED THU FRI SAT SUN
 * where each day is 3 digits for Morning, Afternoon, Evening (1 = available)
 * e.g. Aoife 1 5 111 111 111 000 111 110 000
 */
#include <stdio.h>
#include <string.h>
#include "rota.h"

const char *DAY_NAMES[DAYS] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char *SHIFT_NAMES[SHIFTS] = { "Morning", "Afternoon", "Evening" };

/* Returns the number of staff loaded, or -1 if the file can't be opened. */
int loadStaff(const char *filename, RotaData *data)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Error: could not open %s\n", filename);
        return -1;
    }

    data->staffCount = 0;
    char line[256];
    int lineNo = 0;

    while (fgets(line, sizeof(line), fp) != NULL && data->staffCount < MAX_STAFF)
    {
        lineNo++;
        Staff *s = &data->staff[data->staffCount];
        char days[DAYS][8];

        int read = sscanf(line, "%31s %d %d %7s %7s %7s %7s %7s %7s %7s",
                          s->name, &s->isManager, &s->maxShifts,
                          days[0], days[1], days[2], days[3],
                          days[4], days[5], days[6]);

        if (read != 3 + DAYS)
        {
            if (line[0] != '\n' && line[0] != '\r')
            {
                printf("Warning: skipping line %d (bad format)\n", lineNo);
            }
            continue;
        }

        int valid = 1;
        for (int d = 0; d < DAYS; d++)
        {
            if (strlen(days[d]) != SHIFTS)
            {
                valid = 0;
                break;
            }
            for (int sh = 0; sh < SHIFTS; sh++)
            {
                s->available[d][sh] = (days[d][sh] == '1');
            }
        }

        if (!valid)
        {
            printf("Warning: skipping line %d (each day needs 3 digits)\n", lineNo);
            continue;
        }

        s->assigned = 0;
        data->staffCount++;
    }

    fclose(fp);
    return data->staffCount;
}

void printStaff(const RotaData *data)
{
    printf("Loaded %d staff\n", data->staffCount);
    for (int i = 0; i < data->staffCount; i++)
    {
        const Staff *s = &data->staff[i];
        printf("%-10s %-8s max %d  ", s->name, s->isManager ? "manager" : "staff", s->maxShifts);
        for (int d = 0; d < DAYS; d++)
        {
            printf("%s:", DAY_NAMES[d]);
            for (int sh = 0; sh < SHIFTS; sh++)
            {
                printf("%c", s->available[d][sh] ? "MAE"[sh] : '-');
            }
            printf(" ");
        }
        printf("\n");
    }
}
