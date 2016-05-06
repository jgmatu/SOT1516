#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

	
char*
createpath (char *command)
{
	char *path = malloc(strlen("/usr/bin/") + strlen(command) + 1);
	path = strcat (path , "/usr/bin/");
	path = strcat (path, command);
	return path;
}

void
xperrorfile (char *file)
{
	fprintf(stderr, "Error file %s %s\n" , file, strerror(errno));
}

void
xpfduperr()
{
	fprintf(stderr, "Failed duplicate the file descriptor %s\n", strerror(errno));	
}

void 
xforkerr ()
{
	fprintf(stderr, "Error create child process with fork %s\n", strerror(errno));
}
void
xwaitprocess()
{
	fprintf(stderr, "General fail in one process if grep failed possible not lines found on file\n");
}
void 
xerrorpipe ()
{
	fprintf(stderr, "Error create Pipe : %s\n",  strerror(errno));
}

int
xopenR (char *file)
{
	int fd;
	fd = open(file , O_RDONLY);
	if (fd < 0){
		xperrorfile(file);
		return -1;
	}
	return fd;
}

int
xclose (int fd ,  char *file)
{
	if (close(fd) < 0){
		xperrorfile(file);
		return -1;
	}
	return 0;
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
execgrep (int fd , char *regex)
{
	int pid = 0;

	if (fdtoInput(fd) < 0) /* fd to Standard input for command grep */
		return -1;
	switch (pid = fork()){
		case -1 :
			xforkerr();
			return -1;
		/* Child process to run grep */
		case 0: 
			execl("/bin/grep" , "grep" , regex , NULL);
			fprintf(stderr, "Failed execute grep %s\n" , strerror(errno));
			exit(1);

 		/* Grep Lauched Success */
		default :
			return 0;	
	}
}

int
executecmd (char *path , char **parameters , char *inputFile , int fdout)
{ 
	int fdin = 0;

	fdin = xopenR(inputFile); /* Open file to standard input of command */ 
	if (fdin < 0 || fdout < 0)
		return -1;

	if (fdtoInput(fdin) < 0) /* fd to Standard input for command this is a file*/
		return -1;

	if (fdtoOutput(fdout) < 0) /* fdout is a fdpipe[1] */ 
		return -1;
	
	if (xclose (fdin , inputFile) < 0)
		return -1;

	if (xclose(fdout , "Pipe") < 0)
		return -1;

	++parameters;  /* Drop name of program */ 
	++parameters;  /* Drop parameter regex */
	execv(path , parameters);
	fprintf(stderr, "Execv failed %s\n", strerror(errno));
	return -1;
}

int
launchfilter (char *file , char *command ,  char **parameters , char *regex)
{	
	int pid = 0 , pipefd[2] , status = 0 , errors = 0 , w = 0;
	char *path;

	/* Create pipe in parent process */
	if (pipe(pipefd) == -1){ 	
		xerrorpipe();
		return -1;
	}
	switch (pid = fork()){
		case -1 :
			xforkerr();
			return -1;
			
		/* Excuted Command */
		case  0 :
			if (xclose(pipefd[0] , "Pipe") < 0)  /* Command not read in Pipe */
				exit(1);

			path = createpath(command); /* I cant free this memory command will free the memory */
			if (executecmd(path , parameters , file , pipefd[1]) < 0) /* Command write in pipe */
				exit(1);

		/* Executed Grep */
		default :
			if (xclose(pipefd[1] , "Pipe") < 0) /* Grep not write in pipe */
				errors = -1;

 			if (execgrep(pipefd[0] , regex) < 0) /* Grep read of pipe */	
				errors = -1;
	}
	/* Wait for Process Launched (Commnand and Grep) */
	while ((w = wait(&status)) != pid && w != -1)
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0){
			xwaitprocess();
			errors = -1;
		}
	return errors;	
}

int 
staterrors (int stat)
{
	int errors = 0;

	if (stat < 0){
		fprintf(stderr, "Stat errors  : %s\n", strerror(errno));
		errors = -1;
	}
	return errors;
}


int
isdirectory (char *file , int *errors)
{
	struct stat sb;

	*errors = staterrors(stat(file , &sb));
	switch (sb.st_mode & S_IFMT){
		case S_IFDIR :
			return 1;
		default :
			return 0; 
	}
}
	
int
isfile (char *file , int *errors)
{
	struct stat sb;

	*errors = staterrors(stat(file , &sb));
	switch (sb.st_mode & S_IFMT){
		case S_IFREG :
			return 1;
		default : 
			return 0;
	}
}

int
isforfilter(char *name , char *name_program , int *errors_st)
{
	return !isdirectory(name, errors_st) && isfile(name , errors_st) && (strcmp(name , name_program) != 0);
}

int
lisdir (char *pathdir , char **argv)
{
	DIR *d;
	struct  dirent *de;
	int errors = 0 , errors_st;
	char *regex = argv[1] , *command = argv[2];

	d = opendir(pathdir);
	if (d < 0){
		fprintf(stderr, "Error open directory : %s\n", strerror(errno));
		return -1;
	}
	while ((de = readdir(d)) != NULL)
		if (isforfilter(de->d_name , argv[0] , &errors_st))
			if (launchfilter(de->d_name , command , argv , regex) < 0)
				errors = -1;
	return errors + errors_st;
}

int 
main(int argc, char *argv[])
{	
	char pathdir[] = ".";

	if (argc < 3){
		fprintf(stderr, "failed pass of parameters : %s\n" , argv[0]);
		fprintf(stderr, "Example: %s REGEX COMMAND PARAMETER1 ... PARAMETERN\n" , argv[0]);
		exit(1);
	}
	if (lisdir(pathdir , argv) < 0)
		exit(1);
	exit(EXIT_SUCCESS);
}