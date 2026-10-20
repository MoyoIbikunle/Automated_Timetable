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
} Shift;

typedef struct {
    Staff staff[MAX_STAFF];
    int staffCount;
    Shift rota[DAYS][SHIFTS];
} RotaData;

extern const char *DAY_NAMES[DAYS];
extern const char *SHIFT_NAMES[SHIFTS];

/* data.c */
int loadStaff(const char *filename, RotaData *data);
void printStaff(const RotaData *data);

/* scheduler.c */
void setRequired(RotaData *data, int morning, int afternoon, int evening);
int canWork(const RotaData *data, int staffId, int day, int shift);
void generateRota(RotaData *data);
int countUnderstaffed(const RotaData *data);
void printRota(const RotaData *data);

#endif