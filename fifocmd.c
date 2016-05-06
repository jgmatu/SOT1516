#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

enum {
	MAXLINE = 8*1024,
	MAXPATH = 1024,
	MAXCMD = 100,
};

void 
initokens (char **tokens , int maxargs)
{
	int i;
	for (i = 0 ; i < maxargs ; ++i)
		tokens[i] = NULL;
}

void
printoks (char *tokens[] , int numpal)
{
	int i;
	for (i = 0 ; i < numpal ; ++i)
		printf("%s\n", tokens[i]);
}


int
isseparador (char c , char *sep)
{
	int found = 0;
	while (*sep != 0 && !found)
		if (*sep == c) 
			found = 1;
		else
			++sep;
	return found;
}

int
mytokenize(char *str , char **args , int maxargs , char *sep)
{
	int numtoken , enpal;

	/* Inicializacion  y Tokenize String*/
	enpal = 0;
	numtoken = 0;
	while (*str != 0 && numtoken < maxargs){		
		if (!isseparador(*str , sep) && !enpal){
			args[numtoken] = str;	 				/* Puntero a principio del Token*/
			++numtoken;
			enpal = !enpal;
		}
		if (isseparador(*str , sep) && enpal){
			*str = 0; 							    /* Finalizar Token */
			enpal = !enpal;
		}	
		++str; 
	}
	return numtoken;
} 

int
endpath (char *cmdpath)
{
	return strcmp(cmdpath + strlen(cmdpath) - 1 , "/") == 0;
}

void
xpfduperr()
{
	fprintf(stderr, "Failed duplicate the file descriptor %s\n", strerror(errno));	
}

int 
fdtoInput (int fd)
{
	if (dup2(fd , 0) < 0){ 
		xpfduperr();
		return -1; 
	}
	return 0;
}

int
fdtoOutput (int fd)
{
	if (dup2(fd , 1) < 0){
		xpfduperr();
		return -1;
	}
	return 0;
}

int
redicterrors ()
{
	int fderr;
	
	/* Open file /dev/null */
	fderr  = open("/dev/null" , O_WRONLY);
	if (fderr < 0){
		fprintf(stderr, "Error open file /dev/null %s\n",strerror(errno));
		return -1;
	}
	/* Redirect Standard error to file /dev/null */
	if (dup2(fderr , 2) < 0){
		fprintf(stderr, "Error redirect standar output to /dev/null %s\n", strerror(errno));
		return -1;
	}	
	close(fderr);
	return 0;
}


int
execcmd (char *cmd , char **parameters)
{
	char *path[MAXPATH] , *cmdpath = NULL , *envpath = NULL;
	int numpath = 0 , i;

	/* Redirect standard erros to /dev/null */
	if (redicterrors() < 0){
		fprintf(stderr, "Error to redirect errors\n");
		exit(1);
	}
	envpath = strdup(getenv("PATH"));
	initokens(path , MAXPATH);
	numpath = mytokenize(envpath , path , MAXPATH , ":");

	for (i = 0 ; i < numpath ; i++){
		cmdpath = malloc(MAXPATH);
		strcat(cmdpath , path[i]);
			if (!endpath(cmdpath))
				strcat(cmdpath , "/");		
		strcat(cmdpath , cmd);
		execv(cmdpath , parameters);
	}
	fprintf(stderr, "Error launch command : %s %s\n", cmd , strerror(errno));
	return -1;
}

int
cmdargs (char *cmd , char **parameters , int fdinput , int fdOut)
{
	int pid;

	switch (pid = fork()){		
	case -1 :

		fprintf(stderr, "Error create child process %s\n", strerror(errno));
		return -1;

	case 0 :

		if (fdtoOutput(fdOut) < 0)  /* Standard output to fifocmd.out */
			exit(1);

		if (fdtoInput(fdinput) < 0)  /* Standar input to read pipe */
			exit(1);
		
		if (close(fdinput) < 0){
			fprintf(stderr, "Error close fdinput (pipe read descriptor) to process args : %s\n", strerror(errno));
			exit(1);
		}

		execcmd(cmd , parameters);
	default :
		return 0;
	}
}

int
launchcommands (char *line , char *cmd , char **parameters , int fdOut)
{
	char *cmdline[MAXCMD];
	int pid = 0 , fdpipe[2] , errors = 0 , w , status = 0;

	if (pipe(fdpipe) < 0){
		fprintf(stderr, "Error create pipe %s\n", strerror(errno));
		return -1;
	}

	switch (pid = fork()){
	case -1:
	
		fprintf(stderr, "Error create process child  with fork : %s\n", strerror(errno));
		return -1;	
	
	/* Launch cmd line */
	case 0 :

		if (close(fdpipe[0]) < 0) /* Command line not read on the pipe */
			exit(1);
			
		if (fdtoOutput(fdpipe[1]) < 0) /* Pipe to Standard output of process */
			exit(1);
		
		if (close(fdpipe[1]) < 0){ /* Close pipe */
			fprintf(stderr, "Error close Pipe %s\n", strerror(errno));
			exit(1);
		}

		initokens(cmdline , MAXCMD); /* Needed to end-pointer to null for execv*/
		mytokenize(line , cmdline , MAXCMD , " \n");
			
		if (execcmd(cmdline[0] , cmdline) < 0)
			exit(1);

	/* Launch cmd parameters */
	default :

		if(close(fdpipe[1]) < 0){ /* Command not write on the pipe */
			fprintf(stderr, "Error close Pipe %s\n", strerror(errno));
			errors = -1; 
		}
		++parameters; /* Drop name program */
		++parameters; /* Drop fifo path */
		if (cmdargs(cmd , parameters , fdpipe[0] , fdOut) < 0) /* Command read of pipe and write in fifocmd.out */	
			errors = -1;	
	}

	/* Wait for results of child process */
	while ((w = wait(&status)) != pid && w != -1)
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0){
			fprintf(stderr, "Child process failed \n");
			errors = -1;
		}
	return errors;	
}

int
fifocommands(char *pathfifo , char *cmd , char **parameters)
{
	FILE *fp;
	char *line , *lp;
	int fdOut;	

	/* Create file fificmd.out */
	fdOut = creat("fifocmd.out" , 0600);
	if (fdOut < 0){
		fprintf(stderr, "Error creat file fifocmd.out %s\n" , strerror(errno));
		return 1;
	}					
	
	if (mkfifo(pathfifo , 0600)){
		fprintf(stderr, "Error to create fifo : %s\n", pathfifo);
		return -1;
	}	
	
	for (;;){
		fp = fopen(pathfifo , "r"); 
		if (fp == NULL){
			fprintf(stderr, "Error Open fifo : %s %s\n" , pathfifo , strerror(errno));
			free(line);
			return -1;
		}
		/* Read lines of fifo */
		line = malloc(MAXLINE);
		while ((lp = fgets(line , MAXLINE , fp)) != NULL)
			launchcommands(lp , cmd , parameters , fdOut);
		free(line);
		fclose(fp);
	}
	return 0;
}

int 
main(int argc, char *argv[])
{
	
	/* Control parameters */
	if (argc < 3){
		fprintf(stderr, "Error in pass of parameters to %s\n", argv[0]);
		fprintf(stderr, "First Par is name of fifo second parameters and follow are the command and its parameters\n");
		exit(1);
	}
	
	/* Execute fifocmd */
	if (fifocommands(argv[1] , argv[2] , argv) < 0)
		exit(1);
	exit(0);
}