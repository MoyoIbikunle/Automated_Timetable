/*
 * scheduler.c - rota rules and the generator
 *
 * Rules (hard constraints):
 *   1. Staff only work shifts they marked as available
 *   2. At most one shift per person per day
 *   3. Never more shifts than their weekly maximum
 *   4. Each shift is filled up to its required number of staff
 *
 * Fairness: when several people can work a shift, the person with the
 * fewest shifts so far is picked first, so work is spread evenly.
 */
#include <stdio.h>
#include "rota.h"

void setRequired(RotaData *data, int morning, int afternoon, int evening)
{
    int req[SHIFTS] = { morning, afternoon, evening };
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            data->rota[d][sh].required = req[sh];
        }
    }
}

/* 1 if the person is already on any shift that day */
static int worksThatDay(const RotaData *data, int staffId, int day)
{
    for (int sh = 0; sh < SHIFTS; sh++)
    {
        const Shift *s = &data->rota[day][sh];
        for (int i = 0; i < s->count; i++)
        {
            if (s->staffIds[i] == staffId)
            {
                return 1;
            }
        }
    }
    return 0;
}

/* 1 if this person can be added to this shift without breaking a rule */
int canWork(const RotaData *data, int staffId, int day, int shift)
{
    const Staff *s = &data->staff[staffId];

    if (!s->available[day][shift])        return 0;  /* rule 1 */
    if (worksThatDay(data, staffId, day)) return 0;  /* rule 2 */
    if (s->assigned >= s->maxShifts)      return 0;  /* rule 3 */
    return 1;
}

void generateRota(RotaData *data)
{
    /* Clear the previous rota */
    for (int i = 0; i < data->staffCount; i++)
    {
        data->staff[i].assigned = 0;
    }
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            data->rota[d][sh].count = 0;
        }
    }

    /* Fill each shift in turn */
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            Shift *shift = &data->rota[d][sh];

            while (shift->count < shift->required && shift->count < MAX_PER_SHIFT)
            {
                int best = -1;
                for (int i = 0; i < data->staffCount; i++)
                {
                    if (canWork(data, i, d, sh) &&
                        (best == -1 || data->staff[i].assigned < data->staff[best].assigned))
                    {
                        best = i;
                    }
                }

                if (best == -1)
                {
                    break;  /* nobody available: shift stays understaffed (rule 4 not met) */
                }

                shift->staffIds[shift->count++] = best;
                data->staff[best].assigned++;
            }
        }
    }
}

int countUnderstaffed(const RotaData *data)
{
    int total = 0;
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            if (data->rota[d][sh].count < data->rota[d][sh].required)
            {
                total++;
            }
        }
    }
    return total;
}

void printRota(const RotaData *data)
{
    for (int d = 0; d < DAYS; d++)
    {
        printf("%s\n", DAY_NAMES[d]);
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            const Shift *s = &data->rota[d][sh];
            printf("  %-9s (%d/%d): ", SHIFT_NAMES[sh], s->count, s->required);
            for (int i = 0; i < s->count; i++)
            {
                printf("%s ", data->staff[s->staffIds[i]].name);
            }
            if (s->count < s->required)
            {
                printf(" <-- UNDERSTAFFED");
            }
            printf("\n");
        }
    }
    printf("\nShifts per person:\n");
    for (int i = 0; i < data->staffCount; i++)
    {
        printf("  %-10s %d / %d\n", data->staff[i].name,
               data->staff[i].assigned, data->staff[i].maxShifts);
    }
    printf("Understaffed shifts: %d\n", countUnderstaffed(data));
}
