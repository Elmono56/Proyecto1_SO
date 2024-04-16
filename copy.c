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
#include <time.h>
#include <sys/shm.h>
#include <sys/wait.h>

// cantidad maxima de procesos hijos
#define MAX_PROCESS 10

// struct para almacenar las rutas para cada archivo
struct rutas {
    char* ruta1;
    char* ruta2;
};

// funcio de copiar un archivo
void copiar_archivo(int pCola, int pKey, int pPPID) {

	struct rutas msg;
	// mensaje contiene ruta de archivo
   	if (msgrcv(pCola, &msg, sizeof(struct rutas), pPPID, 0) == -1) {
    	perror("Error al recibir el mensaje\n");
		exit(EXIT_FAILURE);
    };

	char *rutaOrigen = msg.ruta1;
	char *rutaDestino = msg.ruta2;

    FILE *origen = fopen(rutaOrigen, "rb");
    if (!origen) {
        perror("Error al abrir el archivo de origen");
        exit(EXIT_FAILURE);
    };
    FILE *destino = fopen(rutaDestino, "wb");
    if (!destino) {
        fclose(origen);
        perror("Error al crear el archivo de destino");
        exit(EXIT_FAILURE);
    };

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
        };
    };
	printf("Archivo copiado exitosamente");

    fclose(origen);
    fclose(destino);
}

// funcion auxiliar para extraer el nombre de archivos en ruta
void leerRuta(const char *rutaOrigen, const char *rutaDestino, int pCola, int pKey, int pPPID){
	
	// validaciones de rutas
    DIR *dir = opendir(rutaOrigen);
    if (!dir) {
        perror("Error al abrir el directorio de origen");
        exit(EXIT_FAILURE);
    };
    if (mkdir(rutaDestino, 0777) == -1 && errno != EEXIST) {
        perror("Error al crear el directorio de destino");
        exit(EXIT_FAILURE);
    };

    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        };

		// crear ruta de origen para el archivo
        char rutaOrigenCompleta[PATH_MAX];
        snprintf(rutaOrigenCompleta, PATH_MAX, "%s/%s", rutaOrigen, dp->d_name);

		// crear ruta de destino para el archivo
        char rutaDestinoCompleta[PATH_MAX];
        snprintf(rutaDestinoCompleta, PATH_MAX, "%s/%s", rutaDestino, dp->d_name);

        struct stat info;
        if (stat(rutaOrigenCompleta, &info) == -1) {
            perror("Error al obtener información del archivo/directorio de origen");
            exit(EXIT_FAILURE);
        };

	// si es subdirectorio se llama nuevamente esta función
        if (S_ISDIR(info.st_mode)) {
            leerRuta(rutaOrigenCompleta, rutaDestinoCompleta, pCola, pKey, pPPID);
        }
	// si es archivo se envía un mensaje a la cola
	else if(S_ISREG(info.st_mode)) {

		// crear ruta del archivo
		struct rutas msg = {*rutaOrigenCompleta, *rutaDestinoCompleta};
		// enviar ruta a la cola y validar si hay fallo
		if (msgsnd(pCola, &msg, sizeof(struct rutas), pPPID) == -1) {
			perror("Error al enviar el mensaje\n");
			exit(EXIT_FAILURE);
		};
        };
    };

    closedir(dir);
    
};

// funcion inicial: llama auxiliares
void iniciarCopy(int pCantProcesos, char* pRutaO, char* pRutaD){

	key_t msg_key = (pCantProcesos*10);
	int cola = msgget(msg_key, 0666 | IPC_CREAT);

	leerRuta(pRutaO, pRutaD, cola, msg_key, getppid());
	
	// Crear pool de procesos estático
    pid_t pids[MAX_PROCESS];
    for (int i = 0; i < MAX_PROCESS; ++i) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("Error al crear el proceso hijo");
            exit(EXIT_FAILURE);
        }
	else if (pids[i] == 0) {
            copiar_archivo(cola, msg_key, getppid());
        };
    };

    // Cerrar los directorios de los procesos padre
    for (int i = 0; i < MAX_PROCESS; ++i) {
        if (pids[i] != 0) {
            waitpid(pids[i], NULL, 0);
        };
    };
	// esperar a que los hilos terminen
	for (int i = 0; i < MAX_PROCESS; i++) wait(NULL);
	// Liberar recursos compartidos
	shmctl(cola, IPC_RMID, NULL);

	printf("Copiado completado!\n");
};


// funcion auxiliar: medir tiempo de ejecucion
void medirTiempo(int pCantProcesos, char* pRutaO, char* pRutaD){
	//iniciar contador
	time_t inicio = time(NULL);
	 
	iniciarCopy(pCantProcesos, pRutaO, pRutaD);
 	
 	// terminar contador
	time_t fin = time(NULL);
	
	// medir diferencia
	printf("\n Con %i procesos se necesitaron %d segundos", MAX_PROCESS, (fin - inicio));
};


int main(int argc, char *argv[]) {
	if (argc != 3) {
        fprintf(stderr, "Uso: %s ruta_origen ruta_destino\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *rutaOrigen = argv[1];
    char *rutaDestino = argv[2];
  
  //prueba unitaria (funcionamiento)
  iniciarCopy(MAX_PROCESS, rutaOrigen, rutaDestino);
  
  return 0;
};
