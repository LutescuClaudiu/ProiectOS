#ifndef TREASURE_H
#define TREASURE_H

#define MAX_NAME_LEN 50
#define MAX_CLUE_LEN 200
#define MAX_ID_LEN 20
#define MAX_TREASURE_ID_LEN 20

typedef struct {
    char id[MAX_ID_LEN];
    char user[MAX_NAME_LEN];
    float latitude;
    float longitude;
    char clue[MAX_CLUE_LEN];
    int value;
} Treasure;

#endif