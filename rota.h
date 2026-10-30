#ifndef ROTA_H
#define ROTA_H

#define DAYS 7
#define SHIFTS 3          /* 0 = Morning, 1 = Afternoon, 2 = Evening */
#define MAX_STAFF 20
#define MAX_PER_SHIFT 4
#define NAME_LEN 32

typedef struct {
    char name[NAME_LEN];
    int isManager;                 /* 1 = manager */
    int maxShifts;                 /* max shifts per week */
    int available[DAYS][SHIFTS];   /* 1 = can work that shift */
    int assigned;                  /* shifts given this week */
} Staff;

typedef struct {
    int staffIds[MAX_PER_SHIFT];   /* indexes into RotaData.staff */
    int count;                     /* how many assigned */
    int required;                  /* minimum needed */
    int locked;                    /* 1 = edited by hand: Generate leaves it exactly as it is */
} Shift;

typedef struct {
    Staff staff[MAX_STAFF];
    int staffCount;
    Shift rota[DAYS][SHIFTS];
    int requireManager;            /* 1 = every shift needs a manager */
} RotaData;

extern const char *DAY_NAMES[DAYS];
extern const char *SHIFT_NAMES[SHIFTS];
extern const char *SHIFT_TIMES[SHIFTS];   /* e.g. "7:00-12:00" */

/* data.c */
int loadStaff(const char *filename, RotaData *data);
void printStaff(const RotaData *data);
void deleteStaff(RotaData *data, int index);
int saveStaff(const char *filename, const RotaData *data);
int exportRotaCSV(const char *filename, const RotaData *data);
int findStaff(const RotaData *data, const char *name);
int addStaff(RotaData *data, const char *name, int isManager, int maxShifts);
int saveRota(const char *filename, const RotaData *data);
int loadRota(const char *filename, RotaData *data);

/* scheduler.c */
void setRequired(RotaData *data, int morning, int afternoon, int evening);
int canWork(const RotaData *data, int staffId, int day, int shift);
void generateRota(RotaData *data);
int countUnderstaffed(const RotaData *data);
int shiftHasManager(const RotaData *data, int day, int shift);
int countNoManager(const RotaData *data);
void managerCapacity(const RotaData *data, int *capacity, int *used);
int findOnShift(const RotaData *data, int staffId, int day, int shift);
void setShift(RotaData *data, int day, int shift, const int *staffIds, int n);
void unlockShift(RotaData *data, int day, int shift);
void unlockAll(RotaData *data);
int shiftProblem(const RotaData *data, int staffId, int day, int shift, char *msg, int msgSize);
void printRota(const RotaData *data);

#endif