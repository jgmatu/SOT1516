#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum  {
	HOME = 0,
	PID = 1,
	LOGIN = 2,
	PWD = 3,
	Noptions = 4,
};
char *opciones[] = {
	[HOME] = "CASA",
	[PID] = "*",
	[LOGIN] = "USUARIO",
	[PWD] = "DIRECTORIO",
};

int
esN(char *argv)
{
	return strcmp(argv, "-n") == 0;
}

int opcion (char *argv)
{
	int pos, encontrado;
	encontrado = pos = 0;
	while(pos < Noptions && !encontrado)
		if (strcmp(opciones[pos] , argv) == 0) encontrado = 1; else ++pos;
	return pos;
}

void
printarg (char *argv , int argc , int numarg)
{
	switch (opcion(argv)){
		case HOME:
			printf("%s" , getenv("HOME"));
			break;
		case PID:
			printf("%i" , getpid());
			break;
		case LOGIN:
			printf("%s" , getlogin());
			break;
		case PWD:
			printf("%s" , getenv("PWD"));
			break;
		default:	
			printf("%s", argv);
	}
	/* Espacio entre argumentos sin incluir espacio en el ultimo argumento*/
	if (numarg < argc){
		printf(" ");
	}
}

int 
main(int argc, char *argv[])
{
	/* Quitar el nombre del programa */
	--argc;
	++argv; 

	int n , numarg;
	n = 0; /* Supongo no me han puesto -n */
	/*Si el primer argumento es -n lo quito y lo digo*/
	if (argc != 0 && esN(argv[0])){
		--argc;
		++argv;
		n = 1;
	}
	/* Empiezo a imprimir argumentos */
	int i;
	numarg = 1;
	for(i = 0 ; i < argc ; ++i){
		printarg(argv[i] , argc , numarg);
		++numarg;
	}

	/* Salto de linea si no he metido -n */
	if (argc > 0 && n != 1){
		printf("\n");
	}

	/*Imprime \n si no le meto argumentos al echo */
	if (argc == 0 && n != 1){
		printf("\n");
	}
	exit(0);
}

/* Quitar el nombre del programa al principio, --argc ++argv */
/* si es -n el primer argumento quitarlo de los argumentos --argc ++argv */
/* Hacer el array y quitar todo lo que sea NOMBREPRO */ 

/* Backtracking laberinto y volver para atras para ver todos los caminos*/
/* Advance data structures peter brass (1,2)*/
/* Algoritms in C sedgewick (primeros capítulos no leer intro C)*/
/* Practice of programming */
/* Indexes del libro de C mirar los includes */
