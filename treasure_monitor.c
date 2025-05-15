#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "treasure.h"

#define COMMAND_FILE "/tmp/treasure_monitor_cmd"
#define OUTPUT_PIPE "/tmp/treasure_monitor_output"

volatile sig_atomic_t command_received = 0;
int output_fd = -1;

void handle_sigusr1(int sig) {
    command_received = 1;
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = handle_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
}

void send_output(const char *message) {
    if (output_fd == -1) {
        output_fd = open(OUTPUT_PIPE, O_WRONLY);
        if (output_fd == -1) {
            perror("open output pipe");
            return;
        }
    }
    write(output_fd, message, strlen(message));
}

void list_hunts() {
    DIR *dir;
    struct dirent *entry;
    char output[1024] = {0};

    dir = opendir("hunts");
    if (dir == NULL) {
        send_output("Error: Could not open hunts directory\n");
        return;
    }

    strcat(output, "=== List of Hunts ===\n");

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), "hunts/%s/treasures.dat", entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            int count = st.st_size / sizeof(Treasure);
            char line[256];
            snprintf(line, sizeof(line), "%s: %d treasures\n", entry->d_name, count);
            strcat(output, line);
        }
    }

    closedir(dir);
    send_output(output);
}

void list_treasures(const char *hunt_id) {
    char path[256];
    snprintf(path, sizeof(path), "hunts/%s/treasures.dat", hunt_id);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        send_output("Error: Could not open treasures file\n");
        return;
    }

    char output[2048] = {0};
    strcat(output, "=== Treasures in Hunt ===\n");
    strcat(output, "ID\tUser\tLatitude\tLongitude\tValue\n");
    strcat(output, "--------------------------------------------------\n");

    Treasure treasure;
    while (read(fd, &treasure, sizeof(treasure)) == sizeof(treasure)) {
        char line[256];
        snprintf(line, sizeof(line), "%s\t%s\t%.6f\t%.6f\t%d\n",
               treasure.id, treasure.user,
               treasure.latitude, treasure.longitude,
               treasure.value);
        strcat(output, line);
    }

    close(fd);
    send_output(output);
}

void view_treasure(const char *hunt_id, const char *treasure_id) {
    char path[256];
    snprintf(path, sizeof(path), "hunts/%s/treasures.dat", hunt_id);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        send_output("Error: Could not open treasures file\n");
        return;
    }

    char output[1024] = {0};
    Treasure treasure;
    int found = 0;

    while (read(fd, &treasure, sizeof(treasure)) == sizeof(treasure)) {
        if (strcmp(treasure.id, treasure_id) == 0) {
            found = 1;
            snprintf(output, sizeof(output),
                   "=== Treasure Details ===\n"
                   "Hunt ID: %s\n"
                   "Treasure ID: %s\n"
                   "User: %s\n"
                   "Coordinates: %.6f, %.6f\n"
                   "Clue: %s\n"
                   "Value: %d\n",
                   hunt_id, treasure.id, treasure.user,
                   treasure.latitude, treasure.longitude,
                   treasure.clue, treasure.value);
            break;
        }
    }

    if (!found) {
        snprintf(output, sizeof(output), "Treasure with ID %s not found in hunt %s\n", treasure_id, hunt_id);
    }

    close(fd);
    send_output(output);
}

void process_command(const char *cmd) {
    char command[256];
    char hunt_id[256];
    char treasure_id[256];

    if (strcmp(cmd, "list_hunts") == 0) {
        list_hunts();
    } else if (sscanf(cmd, "list_treasures %s", hunt_id) == 1) {
        list_treasures(hunt_id);
    } else if (sscanf(cmd, "view_treasure %s %s", hunt_id, treasure_id) == 2) {
        view_treasure(hunt_id, treasure_id);
    } else {
        send_output("Error: Unknown command\n");
    }
}

int main() {
    // Create named pipe for output
    mkfifo(OUTPUT_PIPE, 0666);

    setup_signal_handlers();

    while (1) {
        pause(); // Wait for signal

        if (command_received) {
            command_received = 0;

            // Read command from file
            int fd = open(COMMAND_FILE, O_RDONLY);
            if (fd == -1) {
                send_output("Error: Could not open command file\n");
                continue;
            }

            char cmd[256];
            ssize_t bytes = read(fd, cmd, sizeof(cmd)-1);
            if (bytes > 0) {
                cmd[bytes] = '\0';
                process_command(cmd);
            }

            close(fd);
        }
    }

    close(output_fd);
    unlink(OUTPUT_PIPE);
    return 0;
}