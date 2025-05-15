#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "treasure.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "hunts/%s/treasures.dat", argv[1]);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open treasures file for hunt %s\n", argv[1]);
        return 1;
    }

    // Create a structure to store user scores
    typedef struct {
        char name[MAX_NAME_LEN];
        int total;
    } UserScore;

    UserScore scores[100];
    int num_users = 0;

    Treasure treasure;
    while (fread(&treasure, sizeof(Treasure), 1, file) == 1) {
        int found = 0;
        for (int i = 0; i < num_users; i++) {
            if (strcmp(scores[i].name, treasure.user) == 0) {
                scores[i].total += treasure.value;
                found = 1;
                break;
            }
        }

        if (!found && num_users < 100) {
            strcpy(scores[num_users].name, treasure.user);
            scores[num_users].total = treasure.value;
            num_users++;
        }
    }

    fclose(file);

    // Sort scores in descending order
    for (int i = 0; i < num_users - 1; i++) {
        for (int j = i + 1; j < num_users; j++) {
            if (scores[i].total < scores[j].total) {
                UserScore temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }

    // Print results
    printf("=== Scores for Hunt %s ===\n", argv[1]);
    for (int i = 0; i < num_users; i++) {
        printf("%s: %d points\n", scores[i].name, scores[i].total);
    }

    return 0;
}