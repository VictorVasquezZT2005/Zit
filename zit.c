#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FILES 100

typedef struct {
    int id;
    char message[256];
    char timestamp[64];
} Commit;

int commit_count = 0;
Commit commits[MAX_FILES];

void save_commit(const char *message) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    commits[commit_count].id = commit_count + 1;
    strncpy(commits[commit_count].message, message, 255);
    snprintf(commits[commit_count].timestamp, 64, "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    commit_count++;
    printf("Commit realizado: %d - %s\n", commit_count, message);
}

void init_repo() {
    FILE *f = fopen(".zit", "w");
    if (!f) {
        perror("Error creando repositorio");
        return;
    }
    fclose(f);
    printf("Repositorio Zit inicializado.\n");
}

void add_file(const char *filename) {
    FILE *f = fopen(".zit", "a");
    if (!f) {
        perror("Error accediendo al repositorio");
        return;
    }
    fprintf(f, "%s\n", filename);
    fclose(f);
    printf("Archivo agregado a Zit: %s\n", filename);
}

void show_log() {
    if (commit_count == 0) {
        printf("No hay commits en Zit.\n");
        return;
    }
    for (int i = commit_count - 1; i >= 0; i--) {
        printf("Commit %d: %s (%s)\n", commits[i].id, commits[i].message, commits[i].timestamp);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Zit C - usa: init | add <file> | commit <msg> | log\n");
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        init_repo();
    } else if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            printf("Error: especifica un archivo para agregar.\n");
            return 1;
        }
        add_file(argv[2]);
    } else if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            printf("Error: especifica un mensaje para el commit.\n");
            return 1;
        }
        save_commit(argv[2]);
    } else if (strcmp(argv[1], "log") == 0) {
        show_log();
    } else {
        printf("Comando desconocido.\n");
        return 1;
    }

    return 0;
}
