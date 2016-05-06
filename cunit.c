#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>


char *envbash = "$nombre";
char *env  		= "nombre";
char *out  		= ".out";
char *ok   		= ".ok";
char *tst  		= ".tst";
char *cond 		= ".cond";

enum{
	MAXLINE   = 1024,
	MAXPATH   = 1024,
	MAXTOKENS = 1024,
	MAXFILES  = 128,
	MAXNAME   = 128,
	BYTEZERO  = 1,
	MAXTESTS  = 128,
	ALIGN32   = 4,
};

typedef struct t_pidtest t_pidtest;
struct t_pidtest  {
	char *test;
	int    pid;
};

static void
tout(int no)
{
	fprintf(stderr , "Test timeout \n");
	exit(1);
}


void
xpfduperr()
{
	fprintf(stderr, "Error dup2... %s\n", strerror(errno));
}


int
isext(char *file , char *ext)
{
	return strcmp(file + strlen(file) - strlen(ext) , ext) == 0;
}

char*
changeext(char *test , char *ext , char *newext)
{
	char *newtest , *auxext;

	newtest = strndup(test , strlen(test)  - strlen(ext) + strlen(newext) + BYTEZERO);  /* Memory to new test with new extension memory reserved */
	auxext = newtest + strlen(test) - strlen(ext);																			/* Copy string test to begin of extension to change extension */

	/* Drop old extension in new string*/
	while(*auxext != 0){
		*auxext = 0;
		auxext++;
	}
	auxext = strcpy(newtest + strlen(newtest) , newext);																/* Copy new extension at begin of old extension deleted */
	return newtest;
}


int
endpath (char *cmdpath)
{
	return strcmp(cmdpath + strlen(cmdpath) - 1 , "/") == 0;
}

int
stdInputToFd (int fd)
{
	if ((fd = dup2(fd , 0)) < 0){
		xpfduperr();
		return -1;
	}
	return fd;
}

int
stdOutputToFd (int fd)
{
	if ((fd = dup2(fd , 1)) < 0){
		xpfduperr();
		return -1;
	}
	return fd;
}

int
stdErrToFd (int fd)
{
	if ((fd = dup2(fd , 2)) < 0){
		xpfduperr();
		return -1;
	}
	return fd;
}

int
fdtoDevNull(int fd)
{
	int fdnull = 0;

	fdnull = open("/dev/null" , O_RDWR);
	if (fdnull < 0){
		fprintf(stderr, "Error open /dev/null... %s\n", strerror(errno));
		return -1;
	}
	if ((fdnull = dup2(fd , fdnull)) < 0){
		xpfduperr();
		return -1;
	}
	return fdnull;
}

void
initokens (char **tokens , int maxargs)
{
	int i;

	for (i = 0 ; i < maxargs ; ++i)
		tokens[i] = NULL;
}

void
printoks (char *tokens[] , int numtoks)
{
	int i;

	for (i = 0 ; i < numtoks ; ++i){
		if (tokens[i] != NULL)
			printf(" %s", tokens[i]);
		else
			printf(" %p" , tokens[i]);
	}
}


int
isseparador (char c , char *sep)
{
	int found = 0;

	while (*sep != 0 && !found){
		if (*sep == c)
			found = 1;
		else
			++sep;
	}
	return found;
}

int
mytokenize(char *str , char **args , int maxargs , char *sep)
{
	int numtoken , enpal;

	/* Init tokens and Tokenize String */
	initokens(args , maxargs);																										/* Init vector of pointer to char with NULL value*/
	enpal = 0;
	numtoken = 0;
	while (*str != 0 && numtoken < maxargs){
		if (!isseparador(*str , sep) && !enpal){
			args[numtoken] = str;	 																										/* Pointer begin token*/
			++numtoken;
			enpal = !enpal;
		}
		if (isseparador(*str , sep) && enpal){
			*str = 0; 							   																								 /* Finalize Token */
			enpal = !enpal;
		}
		++str;
	}
	return numtoken;
}

int
envname (char *parameters[])
{
	int notfound = -1 , pos = 0;

	while (parameters[pos] != NULL){
		if (strcmp(parameters[pos] , envbash) == 0)
			return pos;
		else
			pos++;
	}
	return notfound;
}

int
execmd (char *cmd , char **parameters)
{
	char *path[MAXPATH] , *cmdpath = NULL , *envpath = NULL;
	int numpath = 0 , i , found = -1;

	if ((found = envname(parameters)) != -1)																			/* Look for $nombre in parameters of comand line and change for envoirment variable */
		parameters[found] = strdup(getenv(env));

	if (found == 0)																																/* $nombre is the name of command line */
		cmd = parameters[found];

	execv(cmd , parameters);					 																						/* Command in work directory */
	/* Look for in var PATH cmd */
	envpath = strdup(getenv("PATH"));																							/* Look for in path the command */
	numpath = mytokenize(envpath , path , MAXPATH , ":");
	for (i = 0 ; i < numpath ; i++){
		cmdpath = malloc(MAXPATH);
		strcat(cmdpath , path[i]);
			if (!endpath(cmdpath))
				strcat(cmdpath , "/");
		strcat(cmdpath , cmd);
		execv(cmdpath , parameters);																									/* Launch the command from PATH */
	}
	fprintf(stderr, "Error launch command : %s %s\n", cmd , strerror(errno));
	return -1;
}



int
changeDir (char *path , char **savepath) 							/* built-in Change directory of test not is a process is  a function or command inside the process which launched */
{
	int cd = 0;
	*savepath = getenv("PWD");

	if ((*savepath = strdup(*savepath)) == NULL){																	/* Save path in memory to comeback */
		fprintf(stderr, "Not enougth memory... %s\n", strerror(errno));
		return -1;
	}

	if (chdir(path) < 0){																													/* Change directory of path in command line 0 */
		fprintf(stderr, "Error change directory with path... %s error... %s\n" , path , strerror(errno));
		return -1;
	}
	cd = 1;																																				/* Return the directory chaneged was well */
	return cd;
}

int
outputest(int fdOut , int readLastPipe)																					/* Results of commands  in test.tst Standard ouput and Standard errors to test.out */
{
	int  nr = 0 , nw = 0 , bytesreads = 0;
	char buffer[64*1024];																													/* 32 KB buffer to read of pipe results*/

	/* Write on file out map*/
	for (;;){
		nr = read(readLastPipe , buffer , sizeof buffer);

		if (nr == 0)
			break;

		if (nr < 0){
			fprintf(stderr, "Error read input lastpipe... %s\n", strerror(errno));
			return -1;
		}

		nw = write(fdOut , buffer , nr);
		if (nw != nr){
			fprintf(stderr, "Error write in file test.out... %s\n", strerror(errno));
			return -1;
		}
		bytesreads += nr;
	}
	return bytesreads;
}

int
cmdLinePipe (char *cmd , char **parameters , int numcmd , FILE *fptest , int *readpipe , int readLastPipe)
{
	int pid = 0 , fdpipe[2];

	/* Create Pipe */
	if (pipe(fdpipe) == -1){
		fprintf(stderr, "Error create pipe... %s\n", strerror(errno));
		return -1;
	}

	switch (pid = fork()){
	/* Failed create child process with command and pipe */
	case -1:

		fprintf(stderr, "Error create child process... %s\n", strerror(errno));
		return -1;

	/* Child process with command of line and pipe*/
	case  0:

		if (stdInputToFd(readLastPipe) < 0)														/* Standard input to next-to-last pipe created we have to read last output of that pipe in this process*/
			exit(1);

		if (stdOutputToFd(fdpipe[1]) < 0)															/*Standard Output of process to write pipe */
			exit(1);

		if (close(fdpipe[0]) < 0 || fclose(fptest) != 0)							/* Close read pipe in this process necessary. And close test.tst not important */
			err (1 ,"Error closing files possible a pipe reader... %s\n", strerror(errno));

		if (execmd(cmd , parameters) < 0)															/* Launch command line in this process prepare files to launch. */
			exit(1);

	/* Parent pid child process return by fork and pipe to write must be closed in parent process */
	default:

		*readpipe = fdpipe[0];																				/* Save pipe to read. Next command or final output*/
		if (close(fdpipe[1]) < 0){
			fprintf(stderr, "Error closing write pipe error... %s\n", strerror(errno));
			return -1;
		}
		return pid;																										/* PID of command line launched like child process */
	}
}

int
chdirpwd  (int cd , char *pwdcunit)	/* Comeback direcotry cunit dir. if we chage the directory. To create in cunit directory, test.out, and compare or create test.ok of test.tst. */
{
	if (cd == 1){
		if (chdir(pwdcunit) < 0){
			fprintf(stderr, "Error change directory with path... %s error... %s\n" , pwdcunit , strerror(errno));
			free(pwdcunit);
			return -1;
		}
		free(pwdcunit);
	}
	return 0;
}

int
iscd (char *cmd , int numcmd)
{
	return numcmd == 0 && strcmp(cmd , "cd") == 0;
}


int
exectest (char *test , int fdOut)
{
	FILE *fp = NULL;
	char *line , *lp = NULL , *tokens[MAXTOKENS] , *cmd = NULL , *pwdcunit = NULL;
	int numcmd = 0  , pid  , status , cd = 0 , cmdfailed = 0 , pidLast , numtoks = 0;
	int readLastPipe = 0 , readpipe = 0; /* First pipe Standard output of test.tst initialize */


	if ((fp = fopen(test ,  "r")) == NULL){
		fprintf(stderr, "Error open file... %s\n", strerror(errno));
		return -1;
	}

	/* Read lines of test.tst and put in pipes each command... */
	line = malloc(MAXLINE);
	readLastPipe = 0;
	while ((lp = fgets(line , MAXLINE , fp)) != NULL){

		numtoks = mytokenize(lp , tokens , MAXTOKENS , " \t\n\r");
		/* Empty line */
		if (numtoks == 0)
			continue;

		cmd = tokens[0];
		if (!iscd(cmd , numcmd)){
			if((pidLast = cmdLinePipe(cmd , tokens , numcmd , fp , &readpipe , readLastPipe)) < 0)   /* Errors only for launch process Operating System calls... */
				return -1;
			readLastPipe = readpipe;
		}else{
			if ((cd = changeDir(tokens[1], &pwdcunit)) < 0)	 /* Errors only for change directory Operating System calls... */
				return -1;
		}
		++numcmd;
	}
	free(line);

	if (!numcmd)		/* Case of empty file like error in test.tst.*/
		return 0;			/* No commands failed */

	if (chdirpwd(cd , pwdcunit) < 0)
		return -1;

	if (outputest(fdOut , readLastPipe) < 0)	/* Finals results of test.tst to test.out */
		return -1;
	close(readLastPipe);

	/* Wait for childrens if errors in process are by commands process failed... the last command is taken like cmdfailed != -1 necesary exist pidLast children */
	if ((pid = waitpid(pidLast , &status , 0)) != -1){
		cmdfailed = 0;
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
				cmdfailed = 1;
	}
	fclose(fp);
	return cmdfailed;																				  /* Return cmdfailed of last command */
}

int
isregfile(char *file)
{
	struct stat sb;

	if (stat(file , &sb) < 0){
		fprintf(stderr, "Error stat file... %s error... %s\n" , file , strerror(errno));
		return -1;
	}

	switch (sb.st_mode & S_IFMT){

	case S_IFREG:

		return 1;

	default:

		return 0;

	}
}

int
istheretest (char *test , char *files[] , int numfiles)
{
	int pos = 0 , found = 0;

	while (pos < numfiles && !found)
		if (strcmp(test , files[pos]) == 0)
			found = 1;
		else
			pos++;

	return found;
}

int
sizeAligment (int size)
{
		return (size + ALIGN32 - size%ALIGN32)*ALIGN32;
}

int
fileToStr (char *test , char **str)
{
	char buffer[64*1024];
	int fd = 0, nr = 0 , bytesreads = 0 , size = 0;

	*str = NULL;
	if ((fd = open(test , O_RDONLY)) < 0){
		fprintf(stderr, "Error opening file : %s error... %s\n" , test , strerror(errno));
		return -1;
	}

	/* Read file to convert in string on memmory */
	for (;;){
		nr = read(fd , buffer , sizeof buffer);
		if (nr < 0){
			fprintf(stderr, "Error reading test... %s\n", strerror(errno));
			return -1;
		}

		if (nr == 0)
			break;

		bytesreads += nr;
		size  = sizeAligment(bytesreads + BYTEZERO);
		if (*str == NULL){
			if ((*str = malloc(size)) == NULL){
				fprintf(stderr, "Error malloc function file to string... %s\n", strerror(errno));
				return -1;
			}
			if ((*str = strncpy(*str , buffer , nr)) == NULL){
				fprintf(stderr, "Error copy test function file to string... %s\n", strerror(errno));
				return -1;
			}
		}else{
			if ((*str = realloc(*str , size)) == NULL){
				fprintf(stderr, "Failed realloc memory to file to string... %s\n", strerror(errno));
				return -1;
			}
			if (strncat(*str , buffer , nr) == NULL){
				fprintf(stderr, "Error concat file read in string file to string... %s\n", strerror(errno));
				return -1;
			}
		}
	}
	/* Not necesary null byte, strncat and strncpy put null bytes in string, on memory not used.*/
	close(fd);
	return 0;
}


int
comparetest (char *testout , char *testok)
{
	char *strOk , *strOut;
	int sames = 0;

	if (fileToStr(testok , &strOk) < 0)
		return -1;

	if (fileToStr(testout , &strOut) < 0)
		return -1;

	if (strcmp(strOk , strOut) == 0)
		sames = 1;

	if (strOk == NULL && strOk == NULL)			/* Empty files */
		sames = 1;

	free(strOut);
	free(strOk);

	return sames;
}


int
createOk (char *testok , char *testout)
{
	int fdOut = 0 , fdOk = 0 , nr = 0 , nw = 0;
	char buffer[64*1024];

	/* Create a new file test.ok */
	if ((fdOk = creat(testok , 0600)) < 0){
		fprintf(stderr, "Error create file %s error... %s\n", testok , strerror(errno));
		return -1;
	}

	/* Open test.out to read and copy in test.ok */
	if ((fdOut = open(testout , O_RDONLY)) < 0){
		fprintf(stderr, "Error opening file %s error... %s\n", testout , strerror(errno));
		return -1;
	}

	/* Copy file test.out in test.ok */
	for (;;){
		nr = read(fdOut , buffer , sizeof buffer);
		if (nr < 0){
			fprintf(stderr, "Error reading bytes from %s error... %s\n", testout , strerror(errno));
			return -1;
		}
		if (nr == 0)
			break;			/* File read */

		nw = write(fdOk , buffer , nr);
		if (nw != nr){
			fprintf(stderr, "Error writing in file...%s error...%s\n", testok , strerror(errno));
			return -1;
		}
	}
	close(fdOut);
	close(fdOk);
	return 0;
}

void
opttimeout (int timeout)
{
		/* Optional part flag timeout */
		if (timeout){
			signal(SIGALRM, tout);
			siginterrupt(SIGALRM, 0);
			alarm(timeout);
		}
}

int
launchtest (char *test , char *files[] , int  numfiles , int timeout)
{
	char *testok = NULL , *testout = NULL;
	int pid = 0 , fdOut = 0 , cmdfailed = 0 , sames = 0;


	switch (pid = fork()){
	case -1:

		fprintf(stderr , "Error launch fork process test... %s error...%s\n" , test, strerror(errno));
		return -1;

	case  0:

		opttimeout(timeout);

		if (fdtoDevNull(0) < 0)														/* Input test to /dev/null */
			exit(1);

		/* Create a new file test.out of test.tst to write Standard output and Standard Errors in file test.out of read test.tst */
		testout = malloc(MAXNAME);
		testout = changeext(test , tst , out);

		if ((fdOut = creat(testout , 0600)) < 0)
			err (1 , "Error create file test.out... %s error... %s\n", testout , strerror(errno));


		/* Redirect test.tst standard output to test.out */
		if ((fdOut = stdOutputToFd(fdOut)) < 0)
			exit(1);

		/* Redirect test.tst standard error to test.out */
		if ((fdOut = stdErrToFd(fdOut)) < 0)
			exit(1);

		/* Execute test.tst on directory cmdfailed is of last command */
		if ((cmdfailed = exectest(test , fdOut)) < 0)
			exit(1);

		/* Test ext test.ok open file... or create file with output test only if last command not failed */
		if (!cmdfailed){	/* If last command failed, test failed, we have not to compare test.ok and test.out */
			testok = changeext(test , tst , ok);
			if (istheretest(testok , files , numfiles)){
				if ((sames = comparetest(testout , testok)) < 0)
					exit(1);
			}else{
				if (createOk(testok , testout) < 0)
					exit(1);
				sames = 1;	/* test.ok and test.out are sames. test.ok is create with test.out content. */
			}
		}

		/* Final write results of test.tst in test.out */
		if (close(fdOut) < 0){
			fprintf(stderr, "Error closing test.out error... %s\n", strerror(errno));
			exit(1);
		}
		free(testout);

		/* Result of test no errors in operating system calls... */
		if (cmdfailed || !sames)
			exit(1);
		exit(0);
	default:
		return pid;
	}
}

void
printTestsPids (struct t_pidtest pidstests[] , int numtests)
{
	int i;

	for (i = 0 ; i < numtests ; i++){
		printf("name test : %s\t" , pidstests[i].test);
		printf("pid test  : %d\n" , pidstests[i].pid);
	}
}

char*
findtest (struct t_pidtest pidstests[] , int pid , int numtests)
{
	int pos = 0 , found = 0;
	char *test = NULL;

	while (!found && pos < numtests){
		if (pidstests[pos].pid == pid){
			test = pidstests[pos].test;
			found = 1;
		}else
			pos++;
	}
	return test;
}

int
cunitbasictestdir(char *path , int timeout)
{
	DIR *directory;
	struct dirent *rdirectory;
	struct t_pidtest pidstests[MAXTESTS];	/* struct static array inside stack of function cunitbasictestdir */
	int cunitfailed = 0 , pid = 0 , i , numfiles = 0 ,  status , numtests = 0;
	char *files[MAXFILES] , *test = NULL;

	if ((directory = opendir(path)) == NULL){
		fprintf(stderr, "Error open dir... %s error... %s\n", path , strerror(errno));
		return -1;
	}

	/* Take files and save for compare test.ok if is there in directory */
	initokens(files , MAXFILES);
	while((rdirectory = readdir(directory)) != NULL && numfiles < MAXFILES){
		if ((isext(rdirectory->d_name , tst) || isext(rdirectory->d_name ,  ok)) && isregfile(rdirectory->d_name)){
			/* New file name to basic cunit to memory...*/
			if ((files[numfiles] = strdup(rdirectory->d_name)) == NULL){
				fprintf(stderr, "Error strdup ... %s\n", strerror(errno));
				return -1;
			}
			numfiles++;
		}
	}

	/* Launch test.tst and pass argument files to found test.ok on directory */
	for (i = 0 ; i < numfiles ; i++){
		if (isext(files[i] , tst)){
			if ((pid = launchtest(files[i] , files , numfiles , timeout)) < 0){
				fprintf(stderr, "Error launch test process on system call fork... %s\n" , files[i]);
				cunitfailed = -1;  /* Operating system call errors -1 */
			}
			pidstests[numtests].test = files[i];
			pidstests[numtests].pid  = pid;
			++numtests;
		}
	}

	/* Wait for childrens tests.tst of cunit.c*/
	while ((pid = wait(&status)) != -1){
		test = findtest(pidstests , pid , numtests);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0){
			fprintf(stdout, "TEST FAILED  : %s \n", test);
			cunitfailed = 1;	/* Results of test failed 1 */
		}else
			fprintf(stdout, "TEST SUCCESS : %s \n", test);
	}

	/* Free memory reserved to names of files in directory of cunit.*/
	for (i = 0; i < numfiles ; i++)
		free(files[i]);
	return cunitfailed;
}

int
cunitclean (char *path)
{
	DIR *directory = NULL;
	struct dirent *rdirectory = NULL;

	if ((directory = opendir(path)) == NULL){
		fprintf(stderr, "Error open dir... %s error... %s\n", path , strerror(errno));
		return -1;
	}
	while ((rdirectory = readdir(directory)) != NULL){
		if ((isext(rdirectory->d_name , ok) || isext(rdirectory->d_name , out)) && isregfile(rdirectory->d_name))
			if (unlink(rdirectory->d_name) < 0){
				fprintf(stderr, "Error unlink file... %s\n", strerror(errno));
				return -1;
			}
	}
	return 0;
}

int
cmdCond (char *testcond , char *files[] , int numfiles , char *cmd , char **parameters , int timeout)
{
	char *testout;
	int pid = 0 , fdOut = 0 , condsuccess = 0 , status = 0;

	switch(pid = fork()){

	case -1:

		fprintf(stderr, "Error create fork %s\n", strerror(errno));
		return -1;

	case 0:

		opttimeout(timeout);
		testout = changeext(testcond , cond , out);													/* Create file test.out same that basic cunit program */
		if ((fdOut = creat(testout , 0600)) < 0){
			fprintf(stderr, "Error create file ... %s error... %s\n" , testout , strerror(errno));
			exit(1);
		}

		if (stdOutputToFd(fdOut) < 0)
			exit(1);

		if (stdErrToFd(fdOut) < 0)
			exit(1);

		if (close(fdOut) < 0){
			fprintf(stderr, "Error closing file... %s error... %s\n", testout , strerror(errno));
			exit(1);
		}


		if (execmd(cmd , parameters) < 0)
			exit(1);

	default:

		/* Wait command per line in test.cond */
		if ((pid = wait(&status)) != -1)
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
				condsuccess = 1;
		return condsuccess;
	}
}


int
testcond (char *testcond , char *files[] , int numfiles , int timeout)
{
	FILE *fp = NULL;
	char *lp = NULL , *line = NULL , *tokens[MAXTOKENS] , *cmd = NULL;
	int condsuccess = 0 , numline = 0;

	if ((fp = fopen(testcond , "r")) == NULL){
		fprintf(stderr, "Error open file... %s error...%s\n" , testcond , strerror(errno));
		return -1;
	}

	/* Read lines command of test.cond */
	line = malloc(MAXLINE);
	while (!condsuccess && (lp = fgets(line , MAXLINE , fp)) != NULL){

		mytokenize(lp , tokens , MAXTOKENS , " \t\n");
		cmd = tokens[0];

		/* launch line command and wait results before launch the next command*/
		if ((condsuccess = cmdCond(testcond , files , numfiles , cmd , tokens , timeout)) < 0)
			return -1;

		numline++;
	}
	free(line);
	fclose(fp);

	/* test.cond results */
	if (!condsuccess)
		fprintf(stdout , "TEST COND FAILED  : %s\n" , testcond);
	else
		fprintf(stdout,  "TEST COND SUCCESS : %s\n" , testcond);

	return condsuccess;
}

int
cunitcond(char *path , int timeout)
{
	DIR *directory = NULL;
	struct  dirent *rdirectory = NULL;
	char *files[MAXFILES];
	int numfiles = 0 , i , condsuccess = 0;

	if ((directory = opendir(path)) == NULL){
		fprintf(stderr, "Error open dir... %s error... %s\n", path , strerror(errno));
		return -1;
	}

	/* Take files of directory test.out test.cond */
	numfiles = 0;
	initokens(files , MAXFILES);
	while((rdirectory = readdir(directory)) != NULL && numfiles < MAXFILES){
		if ((isext(rdirectory->d_name , cond) || isext(rdirectory->d_name , out)) && isregfile(rdirectory->d_name)){
			/* New file name of cunit cond to memory...*/
			if ((files[numfiles] = strdup(rdirectory->d_name)) == NULL){
				fprintf(stderr, "Error strdup ... %s\n", strerror(errno));
				return -1;
			}
			numfiles++;
		}
	}

	/* Launch cond and pass files test.out to open or create file test.out */
	for (i = 0 ;  i < numfiles ; i++){
		if (isext(files[i] , cond))
			if ((condsuccess = testcond(files[i] , files , numfiles , timeout)) < 0)
				return -1;
	}

	/* Free memory of files test.out and test.cond */
	for (i = 0 ; i < numfiles ; i++)
		free(files[i]);

	return condsuccess;
}

int
aredigits (const char *text)
{
	int digit = 1;

	while (*text != 0 && digit)
		if (!isdigit(*text))
			digit = 0;
		else
			text++;

	return digit;
}

int
areincompatibles (const char *argv[])
{
	int ist = 0 , isc = 0;

	while(*argv != NULL && !(ist && isc)){
		if (strcmp(*argv , "-c") == 0)
			isc = 1;
		if (strcmp(*argv , "-t") == 0)
			ist = 1;
		argv++;
	}
	return ist && isc;
}

int
main (int argc , char const *argv[])
{
	char *path = ".";
	int cunitfailed = 0 ,/* condsuccess = 0 , istimeout = 0 , */timeout = 0;

	/* Drop argument program name */
	++argv;
	--argc;

	/* Optional Parameters */
//	if (argc > 0){

//		if (areincompatibles(argv)){
//			fprintf(stderr, "Error program -t and -c are incompatibles parameters...\n");
//			exit(1);
//		}

		/* Optional Clean -c */
//		if (argc == 1 && strcmp(argv[0] , "-c") == 0){
//			if (cunitclean(path) < 0)
//				exit(1);
//			exit(0);
//		}

		/* Optional Time  -t */
//		if (argc == 2 && strcmp(argv[0] , "-t") == 0 && aredigits(argv[1])){
//			timeout = atoi(argv[1]);
//			istimeout = 1;
//		}

		/* Bad parameters */
//		if (!istimeout){
//			fprintf(stderr, "Bad parameters... \n");
//			fprintf(stderr, "cunit -c [clean]  \n");
//			fprintf(stderr, "cunit -t 10 [timeout cunit] \n");
//			fprintf(stderr, "cunit [basic cunit and test.cond]\n");
//			exit(1);
//		}
//	}

	/* Basic cunit */
	if ((cunitfailed = cunitbasictestdir(path , timeout)) < 0)
		exit(1);

	/* Optional ext .cond */
	/*if ((condsuccess = cunitcond(path , timeout)) < 0)
		exit(1);
	*/

	/* Optional cond sucess and basic cunit. if you wanna execute basic part comment cunitcond, optional parameters, and drop variable condsuccess. */
	if (cunitfailed  /*|| !condsuccess*/)
		exit(1);
	exit(0);
}
