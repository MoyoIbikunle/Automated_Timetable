/*
 * scheduler.c - rota rules and the generator
 *
 * Rules (hard constraints):
 *   1. Staff only work shifts they marked as available
 *   2. At most one shift per person per day
 *   3. Never more shifts than their weekly maximum
 *   4. Each shift is filled up to its required number of staff
 *   5. Rest: nobody works an Evening then the next day's Morning
 *   6. Each shift has at least one manager (can be switched off)
 *
 * Fairness: when several people can work a shift, the person with the
 * fewest shifts so far is picked first, so work is spread evenly.
 * If several people are tied, one is picked at random. Generate tries
 * 200 random rotas and keeps the one with the fewest understaffed shifts.
 *
 * Manual changes: when a manager edits a shift by hand (ticking people on
 * or off in the shift list), that shift is locked - Generate leaves it
 * exactly as it is and fills everything else around it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* 1 if working this shift would mean Evening followed by next-day Morning */
static int breaksRest(const RotaData *data, int staffId, int day, int shift)
{
    if (shift == 0 && day > 0 && findOnShift(data, staffId, day - 1, SHIFTS - 1) != -1)
    {
        return 1;   /* worked last night's evening */
    }
    if (shift == SHIFTS - 1 && day < DAYS - 1 && findOnShift(data, staffId, day + 1, 0) != -1)
    {
        return 1;   /* already on tomorrow morning (e.g. put there by hand) */
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
    if (breaksRest(data, staffId, day, shift)) return 0;  /* rule 5 */
    return 1;
}

/* 1 if anyone who is (wantManager = 1) or isn't (wantManager = 0) a manager can work it */
static int anyoneCanWork(const RotaData *data, int day, int shift, int wantManager)
{
    for (int i = 0; i < data->staffCount; i++)
    {
        if (data->staff[i].isManager == wantManager && canWork(data, i, day, shift))
        {
            return 1;
        }
    }
    return 0;
}

/* Builds one rota, breaking ties at random. saveManagers = 1 fills the
   other places on a shift that already has a manager with non-managers,
   keeping managers free for shifts that still need one. */
static void generateOnce(RotaData *data, int saveManagers, int hardestFirst)
{
    /* Clear the previous rota, keeping SET (locked) shifts exactly as they are */
    for (int i = 0; i < data->staffCount; i++)
    {
        data->staff[i].assigned = 0;
    }
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            Shift *shift = &data->rota[d][sh];
            if (!shift->locked)
            {
                shift->count = 0;
                continue;
            }
            for (int i = 0; i < shift->count; i++)
            {
                data->staff[shift->staffIds[i]].assigned++;
            }
        }
    }

    /* Decide the order to fill shifts: Monday to Sunday, or (hardestFirst)
       the shifts with the fewest available people first, so they aren't left
       with whoever is still free at the end of the week */
    int order[DAYS * SHIFTS];
    int choices[DAYS * SHIFTS];
    for (int k = 0; k < DAYS * SHIFTS; k++)
    {
        order[k] = k;
        choices[k] = 0;
        for (int i = 0; i < data->staffCount; i++)
        {
            choices[k] += data->staff[i].available[k / SHIFTS][k % SHIFTS];
        }
    }
    if (hardestFirst)
    {
        /* shuffle, then a simple insertion sort by number of choices,
           so shifts with equal choices come in a random order */
        for (int k = DAYS * SHIFTS - 1; k > 0; k--)
        {
            int j = rand() % (k + 1);
            int t = order[k]; order[k] = order[j]; order[j] = t;
        }
        for (int k = 1; k < DAYS * SHIFTS; k++)
        {
            int cur = order[k];
            int j = k - 1;
            while (j >= 0 && choices[order[j]] > choices[cur])
            {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = cur;
        }
    }

    /* Fill each shift in that order */
    for (int k = 0; k < DAYS * SHIFTS; k++)
    {
        int d = order[k] / SHIFTS;
        int sh = order[k] % SHIFTS;
        {
            Shift *shift = &data->rota[d][sh];

            while (!shift->locked && shift->count < shift->required && shift->count < MAX_PER_SHIFT)
            {
                /* Find everyone allowed to work this shift, keeping only those
                   with the fewest shifts so far (fairness rule) */
                int tied[MAX_STAFF];
                int tiedCount = 0;
                int fewest = -1;

                /* Rule 6: a shift with no manager yet looks at managers first.
                   With saveManagers on, once it has one the other places go to
                   non-managers. Either way, fall back to anyone if nobody
                   suitable is free. */
                int only = -1;   /* -1 = anyone, 1 = managers only, 0 = non-managers only */
                if (data->requireManager)
                {
                    int need = !shiftHasManager(data, d, sh);
                    if ((need || saveManagers) && anyoneCanWork(data, d, sh, need))
                    {
                        only = need;
                    }
                }

                for (int i = 0; i < data->staffCount; i++)
                {
                    if (!canWork(data, i, d, sh) || (only != -1 && data->staff[i].isManager != only))
                    {
                        continue;
                    }
                    if (fewest == -1 || data->staff[i].assigned < fewest)
                    {
                        fewest = data->staff[i].assigned;
                        tiedCount = 0;
                    }
                    if (data->staff[i].assigned == fewest)
                    {
                        tied[tiedCount++] = i;
                    }
                }

                if (tiedCount == 0)
                {
                    break;  /* nobody available: shift stays understaffed (rule 4 not met) */
                }

                /* Break ties at random, so each Generate gives a different valid rota */
                int best = tied[rand() % tiedCount];

                shift->staffIds[shift->count] = best;
                shift->count++;
                data->staff[best].assigned++;
            }
        }
    }
}

#define ATTEMPTS 200

/* Random tie-breaks mean some attempts are better than others, so try
   several and keep the best: fewest understaffed shifts first, then fewest
   without a manager. Attempts mix two ideas: saving managers for shifts that
   need one, and filling the hardest shifts (fewest people available) first */
void generateRota(RotaData *data)
{
    static Shift bestRota[DAYS][SHIFTS];
    int bestAssigned[MAX_STAFF];
    int bestScore = -1;

    for (int attempt = 0; attempt < ATTEMPTS; attempt++)
    {
        /* attempts take turns: save managers or not, Monday-first or hardest-first */
        generateOnce(data, attempt % 2, (attempt / 2) % 2);
        int score = countUnderstaffed(data) * 100 + countNoManager(data);

        if (bestScore == -1 || score < bestScore)
        {
            bestScore = score;
            memcpy(bestRota, data->rota, sizeof(bestRota));
            for (int i = 0; i < data->staffCount; i++)
            {
                bestAssigned[i] = data->staff[i].assigned;
            }
        }
        if (score == 0)
        {
            break;  /* can't do better than every rule met */
        }
    }

    memcpy(data->rota, bestRota, sizeof(bestRota));
    for (int i = 0; i < data->staffCount; i++)
    {
        data->staff[i].assigned = bestAssigned[i];
    }
}

/* Returns the person's position in the shift, or -1 if they're not on it */
int findOnShift(const RotaData *data, int staffId, int day, int shift)
{
    const Shift *s = &data->rota[day][shift];
    for (int i = 0; i < s->count; i++)
    {
        if (s->staffIds[i] == staffId)
        {
            return i;
        }
    }
    return -1;
}

/* 1 if the person is on a SET (locked) shift - those stay put when Generate runs */
static int onLockedShift(const RotaData *data, int staffId, int day, int shift)
{
    return data->rota[day][shift].locked && findOnShift(data, staffId, day, shift) != -1;
}

/* Shift list "Set & Generate": the shift gets exactly these people and is
   locked, so Generate keeps it as the manager left it and fills the rest */
void setShift(RotaData *data, int day, int shift, const int *staffIds, int n)
{
    Shift *s = &data->rota[day][shift];

    if (n > MAX_PER_SHIFT)
    {
        n = MAX_PER_SHIFT;
    }
    for (int i = 0; i < s->count; i++)
    {
        data->staff[s->staffIds[i]].assigned--;
    }
    for (int i = 0; i < n; i++)
    {
        s->staffIds[i] = staffIds[i];
        data->staff[staffIds[i]].assigned++;
    }
    s->count = n;
    s->locked = 1;
}

/* Hands a shift back to the generator */
void unlockShift(RotaData *data, int day, int shift)
{
    data->rota[day][shift].locked = 0;
}

/* Every shift back to automatic */
void unlockAll(RotaData *data)
{
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            data->rota[d][sh].locked = 0;
        }
    }
}

/* Why putting this person on this shift would break a rule. Only SET shifts
   are counted, because every other shift is rebuilt by Generate and will move
   out of the way. Writes a short reason into msg and returns 1, or returns 0. */
int shiftProblem(const RotaData *data, int staffId, int day, int shift, char *msg, int msgSize)
{
    const Staff *s = &data->staff[staffId];

    if (!s->available[day][shift])
    {
        snprintf(msg, msgSize, "not available");
        return 1;
    }
    for (int sh = 0; sh < SHIFTS; sh++)
    {
        if (sh != shift && onLockedShift(data, staffId, day, sh))
        {
            snprintf(msg, msgSize, "SET on %s", SHIFT_NAMES[sh]);
            return 1;
        }
    }
    if ((shift == 0 && day > 0 && onLockedShift(data, staffId, day - 1, SHIFTS - 1)) ||
        (shift == SHIFTS - 1 && day < DAYS - 1 && onLockedShift(data, staffId, day + 1, 0)))
    {
        snprintf(msg, msgSize, "no rest (evening then morning)");
        return 1;
    }
    int lockedShifts = 0;
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            if (!(d == day && sh == shift) && onLockedShift(data, staffId, d, sh))
            {
                lockedShifts++;
            }
        }
    }
    if (lockedShifts >= s->maxShifts)
    {
        snprintf(msg, msgSize, "would go over max (%d)", s->maxShifts);
        return 1;
    }
    return 0;
}

int shiftHasManager(const RotaData *data, int day, int shift)
{
    const Shift *s = &data->rota[day][shift];
    for (int i = 0; i < s->count; i++)
    {
        if (data->staff[s->staffIds[i]].isManager)
        {
            return 1;
        }
    }
    return 0;
}

/* Shifts with staff on them but no manager (0 if the rule is switched off) */
int countNoManager(const RotaData *data)
{
    if (!data->requireManager)
    {
        return 0;
    }
    int total = 0;
    for (int d = 0; d < DAYS; d++)
    {
        for (int sh = 0; sh < SHIFTS; sh++)
        {
            if (!shiftHasManager(data, d, sh))
            {
                total++;
            }
        }
    }
    return total;
}

/* Total shifts managers may work (their max shifts added up) and how many
   they've been given, so the app can say when managers are fully booked */
void managerCapacity(const RotaData *data, int *capacity, int *used)
{
    *capacity = 0;
    *used = 0;
    for (int i = 0; i < data->staffCount; i++)
    {
        if (data->staff[i].isManager)
        {
            *capacity += data->staff[i].maxShifts;
            *used += data->staff[i].assigned;
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
            printf("  %-9s (%d/%d)%s: ", SHIFT_NAMES[sh], s->count, s->required, s->locked ? " SET" : "");
            for (int i = 0; i < s->count; i++)
            {
                printf("%s ", data->staff[s->staffIds[i]].name);
            }
            if (s->count < s->required)
            {
                printf(" <-- UNDERSTAFFED");
            }
            if (data->requireManager && !shiftHasManager(data, d, sh))
            {
                printf(" <-- NO MANAGER");
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
    printf("Shifts without a manager: %d\n", countNoManager(data));
}
