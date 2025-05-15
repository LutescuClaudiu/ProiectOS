#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include "treasure.h"

#define MAX_CMD_LEN 256
#define MAX_HUNT_ID_LEN 50
#define COMMAND_FILE "/tmp/treasure_monitor_cmd"
#define OUTPUT_PIPE "/tmp/treasure_monitor_output"

pid_t monitor_pid = 0;
int monitor_active = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid) {
        monitor_active = 0;
        monitor_pid = 0;
        printf("\nMonitor process terminated with status %d\n", status);
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

void send_command_to_monitor(const char *cmd) {
    int fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open command file");
        return;
    }
    write(fd, cmd, strlen(cmd));
    close(fd);

    if (kill(monitor_pid, SIGUSR1) == -1) {
        perror("kill");
    }
}

void read_monitor_output() {
    int fd = open(OUTPUT_PIPE, O_RDONLY);
    if (fd == -1) {
        perror("open output pipe");
        return;
    }

    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    close(fd);
}

void start_monitor() {
    if (monitor_active) {
        printf("Monitor is already running (PID: %d)\n", monitor_pid);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        execl("./treasure_monitor", "treasure_monitor", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        monitor_pid = pid;
        monitor_active = 1;
        printf("Monitor started with PID: %d\n", pid);
    }
}

void stop_monitor() {
    if (!monitor_active) {
        printf("No monitor is currently running\n");
        return;
    }

    printf("Stopping monitor (PID: %d)...\n", monitor_pid);
    if (kill(monitor_pid, SIGTERM) == -1) {
        perror("kill");
    }
}

void list_hunts() {
    if (!monitor_active) {
        printf("No monitor is currently running\n");
        return;
    }

    send_command_to_monitor("list_hunts");
    usleep(100000); // Small delay to allow monitor to process
    read_monitor_output();
}

void list_treasures(const char *hunt_id) {
    if (!monitor_active) {
        printf("No monitor is currently running\n");
        return;
    }

    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "list_treasures %s", hunt_id);
    send_command_to_monitor(cmd);
    usleep(100000);
    read_monitor_output();
}

void view_treasure(const char *hunt_id, const char *treasure_id) {
    if (!monitor_active) {
        printf("No monitor is currently running\n");
        return;
    }

    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "view_treasure %s %s", hunt_id, treasure_id);
    send_command_to_monitor(cmd);
    usleep(100000);
    read_monitor_output();
}

void calculate_score(const char *hunt_id) {
    printf("Calculating scores for hunt: %s\n", hunt_id);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[1]);

        execl("./calculate_score", "calculate_score", hunt_id, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else { // Parent process
        close(pipefd[1]); // Close write end

        char buffer[1024];
        ssize_t bytes;
        printf("Score results:\n");
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer)-1)) > 0) {
            buffer[bytes] = '\0';
            printf("%s", buffer);
        }

        close(pipefd[0]);
        waitpid(pid, NULL, 0);
    }
}

void calculate_all_scores() {
    DIR *dir;
    struct dirent *entry;

    dir = opendir("hunts");
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        calculate_score(entry->d_name);
    }

    closedir(dir);
}

int main() {
    char input[MAX_CMD_LEN];
    char cmd[MAX_CMD_LEN];
    char hunt_id[MAX_HUNT_ID_LEN];
    char treasure_id[MAX_TREASURE_ID_LEN];

    setup_signal_handlers();

    printf("Treasure Hub - Interactive Monitor Manager\n");
    printf("Available commands:\n");
    printf("  start_monitor\n");
    printf("  list_hunts\n");
    printf("  list_treasures <hunt_id>\n");
    printf("  view_treasure <hunt_id> <treasure_id>\n");
    printf("  calculate_score <hunt_id>\n");
    printf("  calculate_all_scores\n");
    printf("  stop_monitor\n");
    printf("  exit\n\n");

    while (1) {
        printf("hub> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            if (monitor_active) {
                printf("Error: Monitor is still running. Please stop it first.\n");
            } else {
                break;
            }
        } else if (strcmp(input, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(input, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(input, "list_hunts") == 0) {
            list_hunts();
        } else if (sscanf(input, "list_treasures %s", hunt_id) == 1) {
            list_treasures(hunt_id);
        } else if (sscanf(input, "view_treasure %s %s", hunt_id, treasure_id) == 2) {
            view_treasure(hunt_id, treasure_id);
        } else if (sscanf(input, "calculate_score %s", hunt_id) == 1) {
            calculate_score(hunt_id);
        } else if (strcmp(input, "calculate_all_scores") == 0) {
            calculate_all_scores();
        } else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }

    if (monitor_active) {
        printf("Stopping monitor before exit...\n");
        kill(monitor_pid, SIGTERM);
        sleep(1);
    }
    
    printf("Goodbye!\n");
    return 0;
}