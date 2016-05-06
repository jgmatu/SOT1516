#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>

/* Variables Globales */
	long N = 0;
	int debug = 0;

/* Constantes Universales */
	enum {
		LEN = 4, /* Strlen de .txt */ 
	};

int
esfintxt (char *cadena)
{
	char *txt = ".txt";
	return strcmp(txt , cadena + strlen(cadena) - LEN) == 0;
}

int
esfichero (char *d_name)
{
	struct stat sb;
	stat(d_name , &sb);
	switch (sb.st_mode & S_IFMT){
		case S_IFREG :
			return 1;
		default :
			return 0;
	}
}

int
tailfich (char *fichero)
{
	char buffer[8*1024]; /* Buffer 8 KB*/
	int fd = 0;
	int nr = 0;
	int nw = 0;
	int errors = 0;
	int offset = 0;
	
	fd = open(fichero , O_RDONLY);
	if (fd < 0){
		warn("ERROR OPEN FICH");
		return -1;
	}

	/* read the text from the end if N is not equals to zero*/
	if (N != 0)
		offset = lseek(fd , -N , SEEK_END);

	/* Output error but print the full text */
	if (offset < 0){
		warn("ERROR LSEEK :");
		lseek(fd , 0 , SEEK_SET);
		errors = -1;
	}

	for(;;){
		nr = read(fd , buffer , sizeof(buffer));

		if (nr < 0){
			warn ("READ FICH : ");
			close(fd);
			return -1;
		}

		if (nr == 0)
			break; /* I have finish read */
		
		nw = write(1 , buffer , nr);	
		if (nw != nr){
			warn ("WRITE FICH : ");
			close(fd);
			return -1;
		}
	}
	close(fd);
	return errors;
}

int
listdir(char *path)
{
	DIR *dir;
	struct dirent *inputdir;
	int errors = 0;

	dir = opendir(path);
	if (dir == NULL){
		warn("ERROR OPENDIR : ");
		exit(1);
	}
	while ((inputdir = readdir(dir)) != NULL)
		if (esfichero(inputdir->d_name) && esfintxt(inputdir->d_name)){
			if(debug == 1)
				printf("fichero : %s\n", inputdir->d_name);

			if (tailfich(inputdir->d_name) < 0)
				errors = -1;
		}
	closedir(dir);
	return errors;
}

int
notdigits (char *digits)
{
	int result = 0;
	while (*digits != '\0' && !result)
		if (isdigit(*digits)) ++digits; else result = 1;
	return result;
}

int 
main(int argc, char *argv[])
{
	char *path = ".";
	int list = 0;

	/*Quitarme Nombre del programa */
	--argc;	
	++argv;

	if (argc != 0 && ((N = atoi(argv[0])) < 0 || notdigits(argv[0]))){
		write(2, "ERROR PAREMETER N\n" , strlen("ERROR PAREMETER N\n"));
		exit(1);
	}

	list = listdir(path);
	if (list < 0)
		err(1 , "PROGRAM ERRORS");

	exit(0);
}