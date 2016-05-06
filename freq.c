#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

/* Constantes Universales */
enum{
	ASCII = 256,
};

/* Variables globales */
int pari = 0;

int
esI (char *argv)
{
	return strcmp(argv , "-i") == 0;
}

void	
initCounter (int counter[])
{
	int i;
	for (i = 0 ; i < ASCII ; ++i)
		counter[i] = 0;
}

void
printCounter(int counter [])
{
	int i;
	for (i = 0 ; i < ASCII ; ++i)
		if (counter[i] != 0)
			printf("%c:%3i\n", i , counter[i]);
}

int
rangeminus (char a)
{
	return (a >= 'a' && a <= 'z');
}

int
rangemayus (char a)
{
	return (a >= 'A' && a <= 'Z');
}

void
prints (int c,double prob100,int countBeginPal,int countEndPal)
{ 
	if (prob100 >= 10.0){
		printf("%c : %2.2f%% ", c , prob100); 
		printf ("%3i %3i\n",countBeginPal , countEndPal);
	}else{
		printf("%c : %2.2f%% ", c ,prob100); 
		printf(" %3i %3i\n" ,countBeginPal, countEndPal);
	}
}

void
printOut(char initC , int freqcars[] , int countBeginPal[] , int countEndPal[] , int numcars)
{
	int caracter , last;
	double prob100;
	prob100 = 0.0;
	caracter = initC;
	last = 0;
	do{
		if (freqcars[caracter] != 0){
			prob100 = (freqcars[caracter]/((double)numcars))*100.0;	
			prints(caracter , prob100 , countBeginPal[caracter] , countEndPal[caracter]);
		}
		last = caracter++;
	}while (caracter < ASCII && last != 'z' && last != 'Z');
}


int
contfreqbuff (int freqcars[] , char *buffer , int nr)
{
	int i , numcars;
	numcars = 0;
	for (i = 0 ; i < nr ; i++){
		/* Sin tener en cuenta Mayus */
		if (pari != 1 && isalpha(*buffer)){ 
			if(isupper(*buffer))
				++freqcars[(int)tolower(*buffer)];
			if(islower(*buffer))
				++freqcars[(int)*buffer];
			++numcars;
		}
		/* Tengo en cuenta Mayus */
		if (pari == 1 && isalpha(*buffer)){ 
			if(islower(*buffer))
				++freqcars[(int)*buffer];
			if(isupper(*buffer))
				++freqcars[(int)*buffer];
			++numcars;
		}	
		++buffer;
	}
	return numcars;
}

void
contEndBeginPalBuff (int numBegin[] , int numEnd[] , char *buffer, int nr)
{
	int enpal , i;
	char *last; /* Puntero a ultimo caracter para finales de palabra */ 
	last = NULL; 
	enpal = 0; /* Empiezo fuera de palabra */
	for (i = 0 ; i < nr ; i++){
		/* Principio de palabra sin  -i no distingo Minus de Mayus*/
		if (pari != 1 && isalpha(*buffer) && !enpal){
			isupper(*buffer) ? ++numBegin[(int)(tolower(*buffer))] : ++numBegin[(int)*buffer];
			enpal = !enpal;
		}

		/* Final de palabra sin -i no distingo Minus de Mayus*/
		if (pari != 1 && !isalpha(*buffer) && enpal){		
			isupper(*last) ? ++numEnd[(int)(tolower(*last))] : ++numEnd[(int)*last];
			enpal = !enpal;
		}

		/* Principio de palabra con -i distingo Minus de Mayus */
		if (pari == 1 && isalpha(*buffer) && !enpal){ 
			enpal = !enpal;   
			++numBegin[(int)*buffer];
		}

		/* Final de palabra con -i distingo Minus de Mayus */
		if (pari == 1 && !isalpha(*buffer) && enpal){ 
			enpal = !enpal;
			++numEnd[(int)*last];
		}	
		last = buffer++;
	}
}

/* Mostrar por la salida estandar el contenido de un fichero */
int
fich (char *argv , int countfreqcars[] , int countBeginPal[] , int countEndPal[] , int input)
{
	int fdp , nr , nw , numcars;
	char buffer[8*1024]; /* Buffer de 8 KB */
	nr = nw = 0; /* Supongo que tengo fichero vacio y que no he escrito nada */
	numcars = 0; /* Numero de caracteres alphanumericos del fichero empiezo a 0 */
	fdp = 0; /* Inicializo a leer de la entrada estandar */

	if (input != 0) /* Compruebo que no se quiere leer de la entrada estandar */
		fdp = open (argv ,O_RDONLY);
	
	if (fdp < 0) 
		err(1 , "ERROR AL ABRIR EL FICHERO :");

	/* Leer el fichero */
	for (;;){ 
		nr = read(fdp , buffer , sizeof(buffer));
		if (nr == 0)
			break; /* no hay mas que leer */

		if (nr < 0)
			err(1 , "ERROR EN LA LECTURA DEL FICHERO");

		/*nw = write(1 , buffer , nr); Escribir Buffer Salida estandar, comentada */
		if (nw < 0)
			err(1, "ERROR EN LA ESCRITURA POR PANTALLA");

		numcars += contfreqbuff(countfreqcars, buffer, nr);
		contEndBeginPalBuff(countBeginPal, countEndPal, buffer, nr);
	}
	return numcars;
}

int 
main(int argc, char *argv[])
{
	int countfreqcars[ASCII];
	int countBeginPal[ASCII];
	int countEndPal[ASCII];
	int numcars;
	int i;

	initCounter(countfreqcars);
	initCounter(countBeginPal);
	initCounter(countEndPal);
	numcars = 0;

	/* Quitarme el nombre del programa */
	--argc;
	++argv;

	/* Control de parámetros paso del -i */
	pari = 0;
	if (argc > 0 && esI(argv[0])){
		--argc;
		++argv;
		pari = 1;
	}
	/* Paso de ficheros al programa y contar la freq del total de ficheros*/
	for (i = 0 ; i < argc ; i++)
		numcars += fich(argv[i] , countfreqcars , countBeginPal , countEndPal , argc); 

	/* Leer de la entrada estandar */
	if (argc == 0)
		numcars = fich(argv[0], countfreqcars, countBeginPal, countEndPal, argc);

	/* Imprimir en pantalla la solucion*/
	if (pari == 1){
		printOut('a' , countfreqcars , countBeginPal , countEndPal , numcars);
		printOut('A', countfreqcars, countBeginPal, countEndPal, numcars);
	}else
		printOut('a' , countfreqcars , countBeginPal , countEndPal , numcars);
	
	exit(0);
}
