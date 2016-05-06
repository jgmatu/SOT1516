#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	MAXARGS = 10,
};

int
esseparador (char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void 
initokens (char **tokens)
{
	int i;
	for (i = 0 ; i < MAXARGS ; ++i)
		tokens[i] = 0;
}

int
isstartpal (char c , int enpal)
{
	return !esseparador(c) && !enpal;
}
int
isfinpal (char c , int enpal)
{
	return esseparador(c) && enpal;
}

int
mytokenize(char *str , char **args , int maxargs)
{
/*printf("Copia del puntero : %p\n", str);*/
	int numpal , enpal;
	
	/* Inicializacion  y Tokenize */
	enpal = 0;
	numpal = 0;
	while (*str != '\0' && numpal < maxargs){
		if (isstartpal(*str,enpal)){
			args[numpal] = str; 			/* Puntero a principio de la palabra */
			++numpal;
			enpal = !enpal;
		}else if (isfinpal(*str,enpal)){
			*str = '\0'; 					/* Finalizar la palabra */
			enpal = !enpal;
		}	
		++str; /* Pasar a la siguiente dirección de memoria siguiente char*/
	}
/*printf("Numero de palabras de la cadena ... : %i\n", numpal);*/
	return numpal;
} 

void
printok (char *tokens[] , int numpal)
{
	int i;
	for (i = 0 ; i < numpal ; ++i)
		printf("%s\n", tokens[i]);
}

void 
do_string(char *str)
{
	/* Declaracion */
	int numpal;
	char *args[MAXARGS];

	/* Inicializacion */
	numpal = 0;
	initokens(args);

	
	/* Tokenizar strings */
	printf("\n************************ STRING  ****************************\n");
	printf("Cadena inicial : %s\n", str);
/*printf("Copia del puntero : %p\n", str);*/
	numpal = mytokenize(str , args , MAXARGS);


	/* Mostrar por pantalla Tokens */
	printok(args , numpal);
	printf("%s\n", "**********************************************************\n\n");
}

int 
main(int argc, char *argv[])
{
	char str1[] = "El  \nreto es encontrar \n 	una opcion    correcta";
	char str2[] = "Bueno...  ya\n 		esta terminando el tokenize...";
	char str3[] = "Jamas conoci a \tuna chica tan\t guapa como tu";

	do_string(str1);
	do_string(str2);
	do_string(str3);

	exit(EXIT_SUCCESS);
}