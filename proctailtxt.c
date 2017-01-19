#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
		MAXFILES = 20,
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

char*
extensionout (char *file)
{
	char *endout = ".out";
	char *exitfile = malloc(sizeof(file) + strlen(endout)); 

	strcpy(exitfile , file); 
	strcat(exitfile , endout); 
	return exitfile;
}

int
freeresources (int fd , int fdout , char *file , char *outfile)
{
	int errors = 0;

	if (!(fd < 0) && close(fd) < 0){
		fprintf(stderr, "error close file : %s : %s\n", file , strerror(errno));
		errors = -1;
	}
	
	if (!(fdout < 0) && close(fdout) < 0){
		fprintf(stderr, "error close file : %s : %s\n", outfile , strerror(errno));
		errors = -1;
	}
	
	free(file);
	free(outfile);
	return errors;
}

/*
Duda de abstracción cualquier fichero evitarme el control de errores al abrir el fichero.
int
openfile (char *file , int *fd)
{
	*fd = open(file , O_RDONLY);
	if (fd < 0){
		fprintf(stderr, "Error  open file : %s : %s \n", file ,strerror(errno));
		return -1;
	}
	return 0;
}
*/

int
tailfile (char *file)
{
	char buffer[8*1024]; /* Buffer 8 KB*/
	int fd = 0 , nr = 0 , errors = 0 , offset = 0 , fdout = 0 , nw = 0;
	char *outfile;

	outfile = extensionout(file);
	/* File to read .txt*/
	fd = open(file , O_RDONLY); 
	if (fd < 0){
		fprintf(stderr, "failed to open file : %s : %s\n" , file , strerror(errno));
		freeresources(fd , fdout , file , outfile);
		return -1;
	}
	/* File to write .txt.out */
	fdout = creat(outfile , 0600); 
	if (fdout < 0){
		fprintf(stderr, "failed to create file : %s : %s\n",outfile,strerror(errno));
		freeresources(fd , fdout , file , outfile);
		return -1;
	}
	/* Read the text from the end if N is not equals to zero*/
	if (N != 0){
		offset = lseek(fd , -N , SEEK_END);
	}
	/* Output error but print the full text Program with errors*/
	if (offset < 0){
		fprintf(stderr, "failed to lseek file : %s : %s\n", file, strerror(errno));
		lseek(fd , 0 , SEEK_SET);
		errors = -1;
	}
	/* Write the tail in file.txt.out read of file.txt*/
	for(;;){
		/* Read file.txt */
		nr = read(fd , buffer , sizeof(buffer));
		if (nr < 0){
			fprintf(stderr, "fail read file : %s : %s\n", file , strerror(errno));
			freeresources(fd , fdout , file , outfile);
			return -1;
		}
		if (nr == 0)
			break; /* I have finish read file.txt*/
		
		/* Write the tail in file.txt.out */
		nw = write(fdout , buffer , nr); 
		if (nr != nw){
			fprintf(stderr, "failed write file %s %s\n", outfile , strerror(errno));
			freeresources(fd , fdout , file , outfile);
			return -1;
		}
	}
	errors = freeresources(fd , fdout , file , outfile);	
	return errors;
}

void
initfiles (char **file)
{
	int i;
	
	for (i = 0 ; i < MAXFILES ; i++) {
		file[i] = NULL;
	}
}

void
printfiles (char **file , int numfiles)
{
	int i;
	
	for (i = 0 ; i < numfiles  ; i++) {
		printf("%s\n", file[i]);
	}
}

int
createchilds (char **file , int numfiles)
{
	int pid = 0 , errors = 0 , status = 0 , i;
	/* Launch Childrens */
	for (i = 0 ; i < numfiles ; i++) {
		switch (pid = fork()) {
			case -1: /* Error launch children (clone) */
				fprintf(stderr, "failed create child %s\n", strerror(errno));
				errors = -1;
				break;
			case 0: /* Work of childs process */
				errors = tailfile(file[i]);
				(errors < 0) ? exit(1) : exit(0);
				break;
			default :
				free(file[i]);
		}
	}
	/* Wait for childrens */
	for (i = 0 ; i < numfiles ; i++) {
		pid = wait(&status);
		if (pid < 0){
			fprintf(stderr, "failed called to wait %s\n", strerror(errno));
			errors = -1;
		}
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0){
			fprintf(stderr, "child process %i failed\n", pid);
			errors = -1;
		}
	}	
	return errors;
}

int
listdir(char *path)
{
	DIR *dir;
	struct dirent *inputdir;
	int errors = 0 , numfile = 0;
	char *file[MAXFILES];

	dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "Error open dir %s\n", strerror(errno));
		exit(1);
	}
	/* List dir and get name files .txt */
	while ((inputdir = readdir(dir)) != NULL && numfile <= MAXFILES) {
		if (esfichero(inputdir->d_name) && esfintxt(inputdir->d_name)){
			file[numfile] = strdup(inputdir->d_name);
			++numfile;
		}
	}
		
	/* Launch chils for each .txt else return -1 if MAXFILES */
	if (numfile > MAXFILES) {
		fprintf(stderr, "Error program maxfiles\n");
		return -1;
	} else {
		errors = createchilds(file , numfile);
	}
	closedir(dir);
	return errors;
}

int
notdigits (char *digits)
{
	int result = 0;
	while (*digits != 0 && !result) {
		if (isdigit(*digits)) { 
			++digits;
		} else {
			result = 1;
		}
	}
	return result;
}

int 
main(int argc, char *argv[])
{
	char *path = ".";
	int list = 0;

	if (argc > 1 && ((N = atoi(argv[1])) < 0 || notdigits(argv[1]))){
		fprintf(stderr, "error parameter N\n");
		fprintf(stderr, "./%s N (N positive number to tail TXT)\n" , argv[0]);
		exit(1);
	}

	list = listdir(path);
	if (list < 0){
		fprintf(stderr, "Program finished with errors \n");
		exit(1);
	}
	exit(0);
}
