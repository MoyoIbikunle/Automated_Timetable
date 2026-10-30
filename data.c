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
const char *SHIFT_TIMES[SHIFTS] = { "7:00-12:00", "12:00-17:00", "17:00-22:00" };

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

/* Removes staff member at index, shifting everyone after them up one place.
   Their shifts are removed from the rota and everyone else's index in the
   rota is updated. Regenerate afterwards to fill the gaps. */
void deleteStaff(RotaData *data, int index)
{
    if (index < 0 || index >= data->staffCount)
    {
        return;
    }
    for (int i = index; i < data->staffCount - 1; i++)
    {
        data->staff[i] = data->staff[i + 1];
    }
    data->staffCount--;

    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            Shift *s = &data->rota[d][sh];
            int kept = 0;
            for (int i = 0; i < s->count; i++)
            {
                int id = s->staffIds[i];
                if (id == index)
                {
                    continue;              /* deleted person: drop them */
                }
                s->staffIds[kept] = (id > index) ? id - 1 : id;
                kept++;
            }
            s->count = kept;
        }
    }
}

/* Writes staff back to the file in the same format loadStaff reads.
   Returns 1 on success, 0 if the file can't be written. */
int saveStaff(const char *filename, const RotaData *data)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        return 0;
    }
    for (int i = 0; i < data->staffCount; i++)
    {
        const Staff *s = &data->staff[i];
        fprintf(fp, "%s %d %d", s->name, s->isManager, s->maxShifts);
        for (int d = 0; d < DAYS; d++)
        {
            fprintf(fp, " ");
            for (int sh = 0; sh < SHIFTS; sh++)
            {
                fprintf(fp, "%d", s->available[d][sh] ? 1 : 0);
            }
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 1;
}

/* Writes the rota as a table that opens in Excel: one row per shift,
   one column per day, then each person's shift count.
   Returns 1 on success, 0 if the file can't be written. */
int exportRotaCSV(const char *filename, const RotaData *data)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        return 0;
    }

    fprintf(fp, "Shift");
    for (int d = 0; d < DAYS; d++)
    {
        fprintf(fp, ",%s", DAY_NAMES[d]);
    }
    fprintf(fp, "\n");

    for (int sh = 0; sh < SHIFTS; sh++)
    {
        fprintf(fp, "%s (%s)", SHIFT_NAMES[sh], SHIFT_TIMES[sh]);
        for (int d = 0; d < DAYS; d++)
        {
            const Shift *s = &data->rota[d][sh];
            fprintf(fp, ",");
            for (int i = 0; i < s->count; i++)
            {
                fprintf(fp, "%s%s", i > 0 ? " / " : "", data->staff[s->staffIds[i]].name);
            }
            if (s->count < s->required)
            {
                fprintf(fp, "%s(needs %d more)", s->count > 0 ? " " : "", s->required - s->count);
            }
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "\nStaff,Shifts,Max\n");
    for (int i = 0; i < data->staffCount; i++)
    {
        fprintf(fp, "%s,%d,%d\n", data->staff[i].name, data->staff[i].assigned, data->staff[i].maxShifts);
    }

    fclose(fp);
    return 1;
}

/* Index of the staff member with this name, or -1 */
int findStaff(const RotaData *data, const char *name)
{
    for (int i = 0; i < data->staffCount; i++)
    {
        if (strcmp(data->staff[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Adds a new staff member with no availability yet (so they aren't put on
   any shift until someone marks when they can work).
   Returns their index, or one of: -1 list full, -2 bad name, -3 name taken.
   Names must be one word (letters, digits, - or ') so staff.txt can read them. */
int addStaff(RotaData *data, const char *name, int isManager, int maxShifts)
{
    if (data->staffCount >= MAX_STAFF)
    {
        return -1;
    }
    size_t len = strlen(name);
    if (len == 0 || len >= NAME_LEN)
    {
        return -2;
    }
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '\'';
        if (!ok)
        {
            return -2;
        }
    }
    if (findStaff(data, name) != -1)
    {
        return -3;
    }

    Staff *s = &data->staff[data->staffCount];
    memset(s, 0, sizeof(*s));
    strcpy(s->name, name);
    s->isManager = isManager;
    s->maxShifts = maxShifts;
    return data->staffCount++;
}

/* Saves the current rota so it can be reopened later. One line per shift:
     day shift locked name name ...
   e.g. "0 0 1 Sean Liam" = Monday Morning, SET by hand, Sean and Liam.
   Names are used (not list positions) so it still loads after staff changes.
   Returns 1 on success, 0 if the file can't be written. */
int saveRota(const char *filename, const RotaData *data)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        return 0;
    }
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            const Shift *s = &data->rota[d][sh];
            fprintf(fp, "%d %d %d", d, sh, s->locked);
            for (int i = 0; i < s->count; i++)
            {
                fprintf(fp, " %s", data->staff[s->staffIds[i]].name);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return 1;
}

/* Loads a rota saved by saveRota. Anyone no longer on the staff list is
   skipped. Returns 1 if a rota was loaded, 0 if there's no usable file. */
int loadRota(const char *filename, RotaData *data)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return 0;
    }

    int loaded = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        int d, sh, locked, used;
        if (sscanf(line, "%d %d %d%n", &d, &sh, &locked, &used) != 3 ||
            d < 0 || d >= DAYS || sh < 0 || sh >= SHIFTS)
        {
            continue;   /* skip anything that isn't a valid shift line */
        }

        Shift *s = &data->rota[d][sh];
        s->count = 0;
        s->locked = locked ? 1 : 0;

        char name[NAME_LEN];
        char *p = line + used;
        int n;
        while (s->count < MAX_PER_SHIFT && sscanf(p, "%31s%n", name, &n) == 1)
        {
            int id = findStaff(data, name);
            if (id != -1)
            {
                s->staffIds[s->count++] = id;
            }
            p += n;
        }
        loaded = 1;
    }
    fclose(fp);

    /* Work out how many shifts each person has from the loaded rota */
    for (int i = 0; i < data->staffCount; i++)
    {
        data->staff[i].assigned = 0;
    }
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            for (int i = 0; i < data->rota[d][sh].count; i++)
            {
                data->staff[data->rota[d][sh].staffIds[i]].assigned++;
            }
        }
    }
    return loaded;
}
