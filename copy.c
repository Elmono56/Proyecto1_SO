#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PROCESS 4

void copiar_archivo(const char *rutaOrigen, const char *rutaDestino) {
    FILE *origen = fopen(rutaOrigen, "rb");
    if (!origen) {
        perror("Error al abrir el archivo de origen");
        exit(EXIT_FAILURE);
    }

    FILE *destino = fopen(rutaDestino, "wb");
    if (!destino) {
        fclose(origen);
        perror("Error al crear el archivo de destino");
        exit(EXIT_FAILURE);
    }

    // Obtener tamaño del archivo
    fseek(origen, 0, SEEK_END);
    long filesize = ftell(origen);
    fseek(origen, 0, SEEK_SET);

    printf("Copiando archivo: %s (%ld bytes)\n", rutaOrigen, filesize);

    char buffer[4096];
    size_t bytes_leidos;
    while ((bytes_leidos = fread(buffer, 1, sizeof(buffer), origen)) > 0) {
        if (fwrite(buffer, 1, bytes_leidos, destino) != bytes_leidos) {
            perror("Error al escribir en el archivo de destino");
            fclose(origen);
            fclose(destino);
            exit(EXIT_FAILURE);
        }
    }

    fclose(origen);
    fclose(destino);
}

void copiar_directorio(const char *rutaOrigen, const char *rutaDestino) {
    DIR *dir = opendir(rutaOrigen);
    if (!dir) {
        perror("Error al abrir el directorio de origen");
        exit(EXIT_FAILURE);
    }

    if (mkdir(rutaDestino, 0777) == -1 && errno != EEXIST) {
        perror("Error al crear el directorio de destino");
        exit(EXIT_FAILURE);
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        char rutaOrigenCompleta[PATH_MAX];
        snprintf(rutaOrigenCompleta, PATH_MAX, "%s/%s", rutaOrigen, dp->d_name);

        char rutaDestinoCompleta[PATH_MAX];
        snprintf(rutaDestinoCompleta, PATH_MAX, "%s/%s", rutaDestino, dp->d_name);

        struct stat info;
        if (stat(rutaOrigenCompleta, &info) == -1) {
            perror("Error al obtener información del archivo/directorio de origen");
            exit(EXIT_FAILURE);
        }

        if (S_ISDIR(info.st_mode)) {
            copiar_directorio(rutaOrigenCompleta, rutaDestinoCompleta);
        } else if (S_ISREG(info.st_mode)) {
            copiar_archivo(rutaOrigenCompleta, rutaDestinoCompleta);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s ruta_origen ruta_destino\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *rutaOrigen = argv[1];
    char *rutaDestino = argv[2];

    // Crear pool de procesos estático
    pid_t pids[MAX_PROCESS];
    for (int i = 0; i < MAX_PROCESS; ++i) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("Error al crear el proceso hijo");
            exit(EXIT_FAILURE);
        } else if (pids[i] == 0) {
            // Proceso hijo

            // Asignar trabajo al proceso hijo
            DIR *dir = opendir(rutaOrigen);
            if (!dir) {
                perror("Error al abrir el directorio de origen");
                exit(EXIT_FAILURE);
            }

            struct dirent *dp;
            while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
                    continue;
                }

                char rutaOrigenCompleta[PATH_MAX];
                snprintf(rutaOrigenCompleta, PATH_MAX, "%s/%s", rutaOrigen, dp->d_name);

                char rutaDestinoCompleta[PATH_MAX];
                snprintf(rutaDestinoCompleta, PATH_MAX, "%s/%s", rutaDestino, dp->d_name);

                struct stat info;
                if (stat(rutaOrigenCompleta, &info) == -1) {
                    perror("Error al obtener información del archivo/directorio de origen");
                    exit(EXIT_FAILURE);
                }

                if (S_ISDIR(info.st_mode)) {
                    // Si es un directorio
                    copiar_directorio(rutaOrigenCompleta, rutaDestinoCompleta);
                    continue;
                } else if (S_ISREG(info.st_mode)) {
                    // Si es un archivo, copiarlo
                    copiar_archivo(rutaOrigenCompleta, rutaDestinoCompleta);
                }
            }

            // Cerrar el directorio de origen
            closedir(dir);

            // Terminar el proceso hijo después de copiar los archivos
            exit(EXIT_SUCCESS);
        }
    }

    // Cerrar los directorios de los procesos padre
    for (int i = 0; i < MAX_PROCESS; ++i) {
        if (pids[i] != 0) {
            waitpid(pids[i], NULL, 0);
        }
    }

    printf("Copiado completado!\n");

    return 0;
}