#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// Headers específicos para Windows
#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
#endif

#define MAX_FILES 1000
#define MAX_PATH 1024
#define ZIT_DIR ".zit"
#define INDEX_FILE ".zit/index"
#define COMMIT_FILE ".zit/commits"
#define CONFIG_FILE ".zit/config"

typedef struct {
    char filename[MAX_PATH];
    char hash[65];
    int tracked;
} FileEntry;

typedef struct {
    int id;
    char message[256];
    char timestamp[64];
    char author[64];
    int file_count;
    char files[MAX_FILES][MAX_PATH];
} Commit;

typedef struct {
    FileEntry files[MAX_FILES];
    int file_count;
    Commit commits[MAX_FILES];
    int commit_count;
    char author[64];
} Repository;

Repository repo = {0};

// Función para crear directorio (compatible Windows/Unix)
void create_dir(const char *path) {
    #ifdef _WIN32
        _mkdir(path);
    #else
        mkdir(path, 0755);
    #endif
}

// Verificar si existe un archivo o directorio
int path_exists(const char *path) {
    #ifdef _WIN32
        return _access(path, 0) == 0;
    #else
        return access(path, F_OK) == 0;
    #endif
}

void generate_hash(const char *filename, char *hash) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        snprintf(hash, 65, "%lx%lx", (long)st.st_size, (long)st.st_mtime);
    } else {
        strcpy(hash, "0");
    }
}

int is_repo_initialized() {
    return path_exists(ZIT_DIR);
}

void load_repo() {
    FILE *f;
    char line[512];
    
    // Resetear contadores
    repo.file_count = 0;
    repo.commit_count = 0;
    
    // Cargar archivos trackeados
    f = fopen(INDEX_FILE, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && repo.file_count < MAX_FILES) {
            char *filename = strtok(line, "|");
            char *hash = strtok(NULL, "|");
            char *tracked_str = strtok(NULL, "\n");
            
            if (filename && hash) {
                strncpy(repo.files[repo.file_count].filename, filename, MAX_PATH-1);
                strncpy(repo.files[repo.file_count].hash, hash, 64);
                repo.files[repo.file_count].tracked = tracked_str ? atoi(tracked_str) : 1;
                repo.file_count++;
            }
        }
        fclose(f);
    }
    
    // Cargar commits
    f = fopen(COMMIT_FILE, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && repo.commit_count < MAX_FILES) {
            Commit *c = &repo.commits[repo.commit_count];
            if (sscanf(line, "%d|%255[^|]|%63[^|]|%63[^|]|%d", 
                       &c->id, c->message, c->timestamp, c->author, &c->file_count) == 5) {
                
                for (int i = 0; i < c->file_count && i < MAX_FILES && fgets(line, sizeof(line), f); i++) {
                    line[strcspn(line, "\n")] = 0;
                    strncpy(c->files[i], line, MAX_PATH-1);
                }
                repo.commit_count++;
            }
        }
        fclose(f);
    }
    
    // Cargar configuración
    f = fopen(CONFIG_FILE, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            strncpy(repo.author, line, 63);
        }
        fclose(f);
    } else {
        strcpy(repo.author, "usuario");
    }
}

void save_repo() {
    FILE *f;
    
    // Guardar archivos trackeados
    f = fopen(INDEX_FILE, "w");
    if (f) {
        for (int i = 0; i < repo.file_count; i++) {
            fprintf(f, "%s|%s|%d\n", 
                   repo.files[i].filename, 
                   repo.files[i].hash, 
                   repo.files[i].tracked);
        }
        fclose(f);
    }
    
    // Guardar commits
    f = fopen(COMMIT_FILE, "w");
    if (f) {
        for (int i = 0; i < repo.commit_count; i++) {
            Commit *c = &repo.commits[i];
            fprintf(f, "%d|%s|%s|%s|%d\n", 
                   c->id, c->message, c->timestamp, c->author, c->file_count);
            
            for (int j = 0; j < c->file_count; j++) {
                fprintf(f, "%s\n", c->files[j]);
            }
        }
        fclose(f);
    }
}

int find_file_index(const char *filename) {
    for (int i = 0; i < repo.file_count; i++) {
        if (strcmp(repo.files[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

int has_file_changed(const char *filename) {
    if (!path_exists(filename)) {
        return 0; // Archivo no existe
    }
    
    int idx = find_file_index(filename);
    if (idx == -1) return 1; // Nuevo archivo
    
    char current_hash[65];
    generate_hash(filename, current_hash);
    return strcmp(repo.files[idx].hash, current_hash) != 0;
}

void add_file_recursive(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);
    
    if (!dp) {
        // Es un archivo, no un directorio
        if (path_exists(path) && has_file_changed(path)) {
            int idx = find_file_index(path);
            if (idx == -1) {
                // Nuevo archivo
                if (repo.file_count < MAX_FILES) {
                    strncpy(repo.files[repo.file_count].filename, path, MAX_PATH-1);
                    generate_hash(path, repo.files[repo.file_count].hash);
                    repo.files[repo.file_count].tracked = 1;
                    repo.file_count++;
                    printf("Agregado: %s\n", path);
                }
            } else {
                // Archivo modificado
                generate_hash(path, repo.files[idx].hash);
                repo.files[idx].tracked = 1;
                printf("Actualizado: %s\n", path);
            }
        }
        return;
    }
    
    // Es un directorio, procesar recursivamente
    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ZIT_DIR) == 0) {
            continue;
        }
        
        char full_path[MAX_PATH];
        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }
        add_file_recursive(full_path);
    }
    closedir(dp);
}

void init_repo() {
    if (is_repo_initialized()) {
        printf("El repositorio Zit ya está inicializado.\n");
        return;
    }
    
    create_dir(ZIT_DIR);
    
    FILE *f = fopen(INDEX_FILE, "w");
    if (f) fclose(f);
    
    f = fopen(COMMIT_FILE, "w");
    if (f) fclose(f);
    
    f = fopen(CONFIG_FILE, "w");
    if (f) {
        fprintf(f, "usuario\n");
        fclose(f);
    }
    
    printf("Repositorio Zit inicializado en %s/\n", ZIT_DIR);
}

void add_files(int argc, char *argv[]) {
    if (!is_repo_initialized()) {
        printf("Error: repositorio no inicializado. Usa 'zit init' primero.\n");
        return;
    }
    
    if (argc < 3) {
        printf("Error: especifica archivos o '.' para agregar todo\n");
        return;
    }
    
    load_repo();
    
    if (strcmp(argv[2], ".") == 0) {
        printf("Agregando todos los archivos nuevos y modificados...\n");
        add_file_recursive(".");
    } else {
        for (int i = 2; i < argc; i++) {
            if (path_exists(argv[i]) && has_file_changed(argv[i])) {
                add_file_recursive(argv[i]);
            } else if (!path_exists(argv[i])) {
                printf("Error: archivo no existe: %s\n", argv[i]);
            } else {
                printf("Sin cambios: %s\n", argv[i]);
            }
        }
    }
    
    save_repo();
}

void save_commit(const char *message) {
    if (!is_repo_initialized()) {
        printf("Error: repositorio no inicializado. Usa 'zit init' primero.\n");
        return;
    }
    
    load_repo();
    
    // Verificar si hay archivos preparados
    int has_staged_files = 0;
    for (int i = 0; i < repo.file_count; i++) {
        if (repo.files[i].tracked) {
            has_staged_files = 1;
            break;
        }
    }
    
    if (!has_staged_files) {
        printf("Error: no hay archivos preparados para commit. Usa 'zit add' primero.\n");
        return;
    }
    
    Commit *commit = &repo.commits[repo.commit_count];
    commit->id = repo.commit_count + 1;
    strncpy(commit->message, message, 255);
    strncpy(commit->author, repo.author, 63);
    commit->file_count = 0;
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(commit->timestamp, 64, "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    // Agregar archivos trackeados al commit
    for (int i = 0; i < repo.file_count && commit->file_count < MAX_FILES; i++) {
        if (repo.files[i].tracked) {
            strncpy(commit->files[commit->file_count], repo.files[i].filename, MAX_PATH-1);
            commit->file_count++;
            repo.files[i].tracked = 0;
        }
    }
    
    repo.commit_count++;
    save_repo();
    
    printf("Commit %d realizado: %s (%d archivos)\n", 
           commit->id, commit->message, commit->file_count);
}

void show_status() {
    if (!is_repo_initialized()) {
        printf("Error: repositorio no inicializado. Usa 'zit init' primero.\n");
        return;
    }
    
    load_repo();
    
    int new_files = 0, modified_files = 0, staged_files = 0;
    
    // Contar archivos preparados
    for (int i = 0; i < repo.file_count; i++) {
        if (repo.files[i].tracked) {
            staged_files++;
        }
    }
    
    // Verificar archivos en el directorio actual
    struct dirent *entry;
    DIR *dp = opendir(".");
    
    if (dp) {
        while ((entry = readdir(dp))) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, ZIT_DIR) == 0) {
                continue;
            }
            
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                int idx = find_file_index(full_path);
                if (idx == -1) {
                    new_files++;
                } else if (has_file_changed(full_path) && !repo.files[idx].tracked) {
                    modified_files++;
                }
            }
        }
        closedir(dp);
    }
    
    printf("Estado del repositorio:\n");
    printf("  Archivos nuevos: %d\n", new_files);
    printf("  Archivos modificados: %d\n", modified_files);
    printf("  Archivos preparados: %d\n", staged_files);
    
    // Mostrar archivos nuevos
    if (new_files > 0) {
        printf("\nArchivos nuevos (usa 'zit add .' para agregar):\n");
        dp = opendir(".");
        while ((entry = readdir(dp))) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, ZIT_DIR) == 0) continue;
            
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                if (find_file_index(full_path) == -1) {
                    printf("    %s\n", entry->d_name);
                }
            }
        }
        closedir(dp);
    }
    
    // Mostrar archivos modificados
    if (modified_files > 0) {
        printf("\nArchivos modificados (usa 'zit add .' para preparar):\n");
        dp = opendir(".");
        while ((entry = readdir(dp))) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, ZIT_DIR) == 0) continue;
            
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                int idx = find_file_index(full_path);
                if (idx != -1 && has_file_changed(full_path) && !repo.files[idx].tracked) {
                    printf("    %s\n", entry->d_name);
                }
            }
        }
        closedir(dp);
    }
}

void show_log() {
    if (!is_repo_initialized()) {
        printf("Error: repositorio no inicializado. Usa 'zit init' primero.\n");
        return;
    }
    
    load_repo();
    
    if (repo.commit_count == 0) {
        printf("No hay commits en Zit.\n");
        return;
    }
    
    printf("Historial de commits Zit:\n");
    printf("========================\n");
    for (int i = repo.commit_count - 1; i >= 0; i--) {
        Commit *c = &repo.commits[i];
        printf("Commit %d: %s\n", c->id, c->message);
        printf("  Autor: %s\n", c->author);
        printf("  Fecha: %s\n", c->timestamp);
        printf("  Archivos: %d\n", c->file_count);
        if (c->file_count > 0) {
            printf("  ");
            for (int j = 0; j < c->file_count && j < 3; j++) {
                printf("%s ", c->files[j]);
            }
            if (c->file_count > 3) printf("...(+%d más)", c->file_count - 3);
            printf("\n");
        }
        printf("\n");
    }
}

void show_help() {
    printf("Zit - Sistema de control de versiones simplificado\n\n");
    printf("Uso: zit <comando> [argumentos]\n\n");
    printf("Comandos disponibles:\n");
    printf("  init                  Inicializar un nuevo repositorio Zit\n");
    printf("  add <archivo|.>       Agregar archivos al área de preparación\n");
    printf("  commit <mensaje>      Realizar commit con los archivos preparados\n");
    printf("  status                Mostrar estado del repositorio\n");
    printf("  log                   Mostrar historial de commits\n");
    printf("  --help                Mostrar esta ayuda\n\n");
    printf("Ejemplos:\n");
    printf("  zit init              Inicializar repositorio\n");
    printf("  zit add .             Agregar todos los archivos nuevos/modificados\n");
    printf("  zit add archivo.c     Agregar archivo específico\n");
    printf("  zit commit \"mensaje\"  Hacer commit con mensaje\n");
    printf("  zit status            Ver estado actual\n");
    printf("  zit log               Ver historial de commits\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        show_help();
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        init_repo();
    } else if (strcmp(argv[1], "add") == 0) {
        add_files(argc, argv);
    } else if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            printf("Error: especifica un mensaje para el commit.\n");
            return 1;
        }
        save_commit(argv[2]);
    } else if (strcmp(argv[1], "status") == 0) {
        show_status();
    } else if (strcmp(argv[1], "log") == 0) {
        show_log();
    } else if (strcmp(argv[1], "--help") == 0) {
        show_help();
    } else {
        printf("Comando desconocido: %s\n", argv[1]);
        printf("Usa 'zit --help' para ver los comandos disponibles.\n");
        return 1;
    }

    return 0;
}