#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>

#define MAX_PROCESS 9
#define MAX_PATH_LEN 512

struct message {
    long mtype;
    char rutaOrigen[MAX_PATH_LEN];
    char rutaDestino[MAX_PATH_LEN];
};

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

void copiar_directorio(const char *rutaOrigen, const char *rutaDestino, int qid) {
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
            copiar_directorio(rutaOrigenCompleta, rutaDestinoCompleta, qid);
        } else if (S_ISREG(info.st_mode)) {
            struct message msg;
            msg.mtype = 1;
            strcpy(msg.rutaOrigen, rutaOrigenCompleta);
            strcpy(msg.rutaDestino, rutaDestinoCompleta);
            msgsnd(qid, &msg, sizeof(struct message) - sizeof(long), 0);
        }
    }

    closedir(dir);
}

// Cada proceso hijo copia el archivo respectivo
void procesar_archivo(int qid) {
    struct message msg;
    while (1) {
        if (msgrcv(qid, &msg, sizeof(struct message) - sizeof(long), 1, 0) == -1) {
            perror("No se pudo obtener el mensaje");
            exit(1);
        }

        // Se recibio el mensaje de terminacion
        if (strlen(msg.rutaOrigen) == 0 && strlen(msg.rutaDestino) == 0) {
            break; 
        }

        copiar_archivo(msg.rutaOrigen, msg.rutaDestino);
    }
}

int main(int argc, char *argv[]) {
    
    //iniciar contador
    time_t inicio = time(NULL);	

    if (argc != 3) {
        fprintf(stderr, "Uso: %s ruta_origen ruta_destino\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Crear la cola de mensajes
    key_t key = ftok(".", 'q');
    int qid = msgget(key, IPC_CREAT | 0666);

    char *rutaOrigen = argv[1];
    char *rutaDestino = argv[2];

    // Crear pool de procesos estático
    pid_t pids[MAX_PROCESS];
    for (int i = 0; i < MAX_PROCESS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            procesar_archivo(qid);
            exit(0);
        } else if (pid < 0) {
            printf("Failed to fork child process\n");
            return 1;
        } else {
            pids[i] = pid;
        }
    }

    copiar_directorio(rutaOrigen, rutaDestino, qid);

    // Se envia el indicador "" para la rutaOrigen y rutaDestino del mensaje
    // para indicarle terminacion al proceso
    struct message msg;
    msg.mtype = 1;
    strcpy(msg.rutaOrigen, "");
    strcpy(msg.rutaDestino, "");
    for (int i = 0; i < MAX_PROCESS; i++) {
        msgsnd(qid, &msg, sizeof(struct message) - sizeof(long), 0);
    }

    // Esperar a que el proceso hijo termine
    for (int i = 0; i < MAX_PROCESS; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Eliminar el mensaje de la cola
    msgctl(qid, IPC_RMID, NULL);
    
    // terminar contador
    time_t fin = time(NULL);
    
    // medir diferenciap
    printf("\n Con %i procesos se necesitaron %d segundos para copiar todo el directorio", MAX_PROCESS, (fin - inicio));

    return 0;
}
