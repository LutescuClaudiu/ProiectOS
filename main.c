#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

// Constants
#define MAX_NAME_LEN 50
#define MAX_CLUE_LEN 200
#define MAX_ID_LEN 20
#define MAX_PATH_LEN 256

// Treasure structure
typedef struct {
    char id[MAX_ID_LEN];
    char user[MAX_NAME_LEN];
    float latitude;
    float longitude;
    char clue[MAX_CLUE_LEN];
    int value;
} Treasure;

// Function prototypes
void print_usage();
void log_operation(const char *hunt_id, const char *operation) {
    // Create hunt directory if it doesn't exist
    char dir_path[MAX_PATH_LEN];
    snprintf(dir_path, MAX_PATH_LEN, "hunts/%s", hunt_id);
    mkdir("hunts", 0777); // Create hunts directory if it doesn't exist
    mkdir(dir_path, 0777); // Create hunt directory if it doesn't exist

    // Open or create log file
    char log_path[MAX_PATH_LEN];
    snprintf(log_path, MAX_PATH_LEN, "%s/logged_hunt", dir_path);
    FILE *log_file = fopen(log_path, "a");
    if (!log_file) {
        perror("Failed to open log file");
        return;
    }

    // Get current time
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0'; // Remove newline

    // Write log entry
    fprintf(log_file, "[%s] %s\n", time_str, operation);
    fclose(log_file);

    // Create symbolic link (Windows uses junctions which are different)
    // For Windows, we'll create a hard link instead
    char link_path[MAX_PATH_LEN];
    snprintf(link_path, MAX_PATH_LEN, "logged_hunt-%s", hunt_id);
    remove(link_path); // Remove existing link if any
    if (link(log_path, link_path) != 0) {
        perror("Failed to create link");
    }
}
void add_treasure(const char *hunt_id) {
    // Create hunt directory if it doesn't exist
    char dir_path[MAX_PATH_LEN];
    snprintf(dir_path, MAX_PATH_LEN, "hunts/%s", hunt_id);
    mkdir("hunts", 0777);
    mkdir(dir_path, 0777);

    // Open or create treasure file
    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/treasures.dat", dir_path);
    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }

    // Get treasure details from user
    Treasure treasure;
    printf("Enter treasure ID: ");
    scanf("%s", treasure.id);
    printf("Enter your name: ");
    scanf("%s", treasure.user);
    printf("Enter latitude: ");
    scanf("%f", &treasure.latitude);
    printf("Enter longitude: ");
    scanf("%f", &treasure.longitude);
    printf("Enter clue: ");
    scanf(" %[^\n]", treasure.clue); // Read entire line including spaces
    printf("Enter value: ");
    scanf("%d", &treasure.value);

    // Write to file
    if (write(fd, &treasure, sizeof(Treasure)) == -1) {
        perror("Failed to write treasure");
    } else {
        printf("Treasure added successfully!\n");

        // Log the operation
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "ADD treasure_id=%s user=%s", treasure.id, treasure.user);
        log_operation(hunt_id, log_msg);
    }

    close(fd);
}
void list_treasures(const char *hunt_id) {
    char dir_path[MAX_PATH_LEN];
    snprintf(dir_path, MAX_PATH_LEN, "hunts/%s", hunt_id);

    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/treasures.dat", dir_path);

    // Get file info
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Failed to get file info");
        return;
    }

    // Print hunt info
    printf("Hunt: %s\n", hunt_id);
    printf("File size: %lld bytes\n", (long long)file_stat.st_size);
    printf("Last modified: %s", ctime(&file_stat.st_mtime));

    // Open file for reading
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }

    // Read and print all treasures
    Treasure treasure;
    printf("\nTreasures:\n");
    printf("ID\tUser\tLatitude\tLongitude\tValue\n");
    printf("--------------------------------------------------\n");

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("%s\t%s\t%.6f\t%.6f\t%d\n",
               treasure.id, treasure.user,
               treasure.latitude, treasure.longitude,
               treasure.value);
    }

    close(fd);

    // Log the operation
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "LIST");
    log_operation(hunt_id, log_msg);
}
void view_treasure(const char *hunt_id, const char *treasure_id) {
    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "hunts/%s/treasures.dat", hunt_id);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }

    Treasure treasure;
    int found = 0;

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(treasure.id, treasure_id) == 0) {
            found = 1;
            printf("\nTreasure Details:\n");
            printf("ID: %s\n", treasure.id);
            printf("User: %s\n", treasure.user);
            printf("Coordinates: %.6f, %.6f\n", treasure.latitude, treasure.longitude);
            printf("Clue: %s\n", treasure.clue);
            printf("Value: %d\n", treasure.value);
            break;
        }
    }

    if (!found) {
        printf("Treasure with ID %s not found.\n", treasure_id);
    }

    close(fd);

    // Log the operation
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "VIEW treasure_id=%s", treasure_id);
    log_operation(hunt_id, log_msg);
}
void remove_treasure(const char *hunt_id, const char *treasure_id) {
    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "hunts/%s/treasures.dat", hunt_id);

    // Open original file
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }

    // Create temporary file
    char temp_path[MAX_PATH_LEN];
    snprintf(temp_path, MAX_PATH_LEN, "hunts/%s/treasures.tmp", hunt_id);
    int temp_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (temp_fd == -1) {
        perror("Failed to create temporary file");
        close(fd);
        return;
    }

    Treasure treasure;
    int found = 0;

    // Copy all treasures except the one to remove
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(treasure.id, treasure_id) != 0) {
            write(temp_fd, &treasure, sizeof(Treasure));
        } else {
            found = 1;
        }
    }

    close(fd);
    close(temp_fd);

    if (!found) {
        printf("Treasure with ID %s not found.\n", treasure_id);
        remove(temp_path);
        return;
    }

    // Replace original file with temporary file
    remove(file_path);
    rename(temp_path, file_path);

    printf("Treasure %s removed successfully.\n", treasure_id);

    // Log the operation
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "REMOVE_TREASURE treasure_id=%s", treasure_id);
    log_operation(hunt_id, log_msg);
}
void remove_hunt(const char *hunt_id) {
    char dir_path[MAX_PATH_LEN];
    snprintf(dir_path, MAX_PATH_LEN, "hunts/%s", hunt_id);

    // First remove the treasure file
    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/treasures.dat", dir_path);
    remove(file_path);

    // Remove the log file
    char log_path[MAX_PATH_LEN];
    snprintf(log_path, MAX_PATH_LEN, "%s/logged_hunt", dir_path);
    remove(log_path);

    // Remove the directory
    if (rmdir(dir_path) == -1) {
        perror("Failed to remove hunt directory");
        return;
    }

    // Remove the symbolic link
    char link_path[MAX_PATH_LEN];
    snprintf(link_path, MAX_PATH_LEN, "logged_hunt-%s", hunt_id);
    remove(link_path);

    printf("Hunt %s removed successfully.\n", hunt_id);

    // No need to log this as the log file is being removed
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    // Parse command line arguments
    if (strcmp(argv[1], "--add") == 0 && argc == 3) {
        add_treasure(argv[2]);
    } else if (strcmp(argv[1], "--list") == 0 && argc == 3) {
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "--view") == 0 && argc == 4) {
        view_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_treasure") == 0 && argc == 4) {
        remove_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_hunt") == 0 && argc == 3) {
        remove_hunt(argv[2]);
    } else {
        print_usage();
        return 1;
    }

    return 0;
}

void print_usage() {
    printf("Usage:\n");
    printf("  treasure_manager --add <hunt_id>\n");
    printf("  treasure_manager --list <hunt_id>\n");
    printf("  treasure_manager --view <hunt_id> <treasure_id>\n");
    printf("  treasure_manager --remove_treasure <hunt_id> <treasure_id>\n");
    printf("  treasure_manager --remove_hunt <hunt_id>\n");
}