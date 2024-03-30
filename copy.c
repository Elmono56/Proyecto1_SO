#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

// cantidad maxima de procesos hijos
#define CMAXP 15
// cantidad maxima del nombre de archivos
#define MAX_MSG_LEN 64

// struct para almacenar las rutas
struct rutas {
    int cantP;
    String ruta1;
    String ruta2;
    long padrePID;
    int cola;
    int key;
};
// struct para manejo de mensajes
struct mensaje{
    char msg[MAX_MSG_LEN];
};


// funcion auxiliar para extraer el nombre de archivos en ruta
void leerRuta1(struct rutas *pRutas){
    	key_t msg_key = (rutaP.cantP*10);
    	int cola = msgget(msg_key, 0666 | IPC_CREAT);
    	pRutas->cola = cola;
    	pRutas->key = msg_key;
    	
    	// hacer ls de ruta1
    	
    	// cantidad de archivos en ruta
    	int cantArchivos = 1;
    	
    	// enviar mensajes a la cola
	for (int i = 0; i < cantArchivos; i++) {
		// crear mensaje
		struct mensaje mess;
		char mensaje[MAX_MSG_LEN] = "archivo.txt";
		
		// Copiar mensaje el mensaje al struct correspondiente
		strcpy(mess.msg, mensaje);
		
		// enviar mensaje a la cola y validar si hay fallo
		if (msgsnd(cola, &mess, sizeof(struct mensaje), 0) == -1) {
		    perror("Error al enviar el mensaje\n");
		    exit(EXIT_FAILURE);
		};
	};
};


// funcion auxiliar: un proceso mueve un archivo
void copy(struct rutas pRutas){
	struct mensaje msg;
    
   	if (msgrcv(pIdCola, &msg, sizeof(struct mensaje), pRutas->padrePID, 0) == -1) {
        	perror("Error al recibir el mensaje\n");
    		exit(EXIT_FAILURE);
    	};
    
	// copy msg(archivo) ruta2

	exit(EXIT_SUCCESS);
};


// funcion principal: crea procesos para que muevan todos los archivos
void iniciarCopy(struct rutas *pRutas){

	leerRuta1(pRutas);
	
	for (int i = 0; i < cantP; i++){
	  	//crear nuevo proceso
		int pid = fork();
		//pid es padre
		if(pid<0) exit(0);
		
		//proceso es hijo
		else if(pid != 0){
		   //invocar funcion auxiliar para copiar archivo (proceso individual)
		   copy(pRutas);
		   
		   break;
		};
	  };
	  
	// esperar a que los hilos terminen
	for (int i = 0; i < cantP; i++) wait(NULL);
	// Liberar recursos compartidos
    	shmctl(cola, IPC_RMID, NULL);
};


// funcion auxiliar: medir tiempo de ejecucion
void medirTiempo(struct rutas* pRutas){
	//iniciar contador
	time_t inicio = time(NULL);
	 
	iniciarCopy(pRutas);
 	
 	// terminar contador
       	time_t fin = time(NULL);
       	
    	// medir diferencia
    	printf("Con %i se necesitaron %d segundos", pRutas->cantP, (fin - inicio));
};


int main () {
  String ruta1 = "";
  String ruta2 = "";
  
  //prueba unitaria (funcionamiento)
  struct rutaP = {5, ruta1, ruta2, getpid(),0,0};
  iniciarCopy(*rutaP);
  
  // prueba masiva (rendimiento)
  for (int i = 1; i<CMAXP; i++){
  	struct rutaCic = {i, ruta1, ruta2, getpid(),0,0};
  	medirTiempo(*rutaCic);
  
  };
  
  return 0;
};
