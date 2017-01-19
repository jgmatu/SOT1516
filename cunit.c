#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <err.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>


#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <ctype.h>

/* Modificacion  psot.c en SOT1516 : cd ... cd a ... si a no existe pruebo a cambiar a a.dir */

static char *EXTOUT  = ".out";
static char *EXTOK   = ".ok";
static char *EXTTST  = ".tst";
static char *EXTCOND = ".cond";

enum {
	MAXLINE   = 1024,
	MAXPATH   = 1024,
	MAXTOKENS = 1024,
	MAXFILES  = 128,
	MAXNAME   = 128,
	BYTEZERO  = 1,
	MAXTESTS  = 128,
	ALIGN     = 2,
};

typedef struct T_PIDTEST T_PIDTEST;
struct T_PIDTEST  {
	char *test;
	int    pid;
};

typedef struct T_CMDPIPE T_CMDPIPE;
struct T_CMDPIPE {
	char  *cmd;
	char  *parameters[MAXTOKENS];
	int 	numpars;
	int   readpipe;
	int   readLastPipe;
};

typedef struct T_FILES T_FILES;
struct T_FILES {
	char *files[MAXFILES];
	int  numfiles;
};


//typedef struct T_CUNIT T_CUNIT;
//struct T_CUNIT {
//	struct T_CMDPIPE *cmdpipe;
//	char *pwdcunit;
//};


static void
tout(int no)
{
	fprintf(stderr , "Test timeout \n");
	exit(1);
}

static void
opttimeout (int timeout)
{
	// Optional part flag timeout.
	if (timeout){
		signal(SIGALRM, tout);
		siginterrupt(SIGALRM, 0);
		alarm(timeout);
	}
}


static void
xpfduperr()
{
	fprintf(stderr, "Error dup2... %s\n", strerror(errno));
}


static int
isext(char *file , char *ext)
{
	return strcmp(file + strlen(file) - strlen(ext) , ext) == 0;
}

static char*
changeext(char *test , char *ext , char *newext)
{
	char *newtest , *auxext;

    // Memory to new test with new extension memory reserved.
	newtest = strndup(test , strlen(test)  - strlen(ext) + strlen(newext) + BYTEZERO);
	if (newtest == NULL)
		err(1 , "Error newtest strdup : %s" , strerror(errno));


	/* Copy string test to begin of extension to change extension. */
	auxext = newtest + strlen(test) - strlen(ext);

	/* Drop old extension in new string*/
	while(*auxext != 0) {
		*auxext = 0;
		auxext++;
	}

	/* Copy new extension at begin of old extension deleted. */
	auxext = strcpy(newtest + strlen(newtest) , newext);

	return newtest;
}


static int
endpath (char *cmdpath)
{
	return strcmp(cmdpath + strlen(cmdpath) - BYTEZERO , "/") == 0;
}

static int
stdin_to_fd (int fd)
{
	if ((fd = dup2(fd , 0)) < 0){
		xpfduperr();
		return -1;
	}

	return fd;
}

static int
stdout_to_fd (int fd)
{
	if ((fd = dup2(fd , 1)) < 0){
		xpfduperr();
		return -1;
	}

	return fd;
}

static int
stderr_to_fd (int fd)
{
	if ((fd = dup2(fd , 2)) < 0){
		xpfduperr();
		return -1;
	}

	return fd;
}

static int
fd_to_devnull(int fd)
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


static FILE*
openfile(char *file , char *mode)
{
	FILE *fp = fopen(file ,  mode);

	if (fp == NULL)
		fprintf(stderr, "Error open file... %s\n", strerror(errno));

	return fp;
}

static void
initokens (char **tokens , int maxargs)
{
	int i;

	for (i = 0 ; i < maxargs ; ++i)
		tokens[i] = NULL;
}

/*
static void
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
*/

static int
is_carater_sep (char c , char *sep)
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

static int
mytokenize(char *str , char **args , int maxargs , char *sep)
{
	int numtoken , enpal;

	/* Init vector of pointer to char with NULL value*/
	initokens(args , maxargs);
	enpal = 0;
	numtoken = 0;
	while (*str != 0 && numtoken < maxargs) {

		if (!is_carater_sep(*str , sep) && !enpal){
			/* Pointer begin token*/
			args[numtoken] = str;
			++numtoken;
			enpal = !enpal;
		}

		if (is_carater_sep(*str , sep) && enpal){
			/* Finalize Token */
			*str = 0;
			enpal = !enpal;
		}
		++str;
	}

	return numtoken;
}

static int
envname (char **parameters)
{
	int pos = 0 , notfound = -1;

	while (parameters[pos] != NULL) {
		if ( *(parameters[pos]) == '$')
			return pos;
		pos++;
	}

	return notfound;
}

static void
change_env (char **parameters , int found)
{
	char *env = parameters[found];

	env++; // Drop $.
	if (getenv(env) != NULL)  {

		parameters[found] = strdup(getenv(env));
		if (parameters[found] == NULL)
			err (1 , "Error strdup to change the name of envoirment variable : %s" , strerror(errno));

	} else {

		fprintf(stderr, "%s %s\n", "Envoirment name of variable not found : " , env);
		exit(1);

	}

}

static int
exec_cmd (char *cmd , char **parameters)
{
	char *path[MAXPATH] , *cmdpath = NULL , *envpath = NULL;
	int numpath = 0 , i , found = -1;


	if ((found = envname(parameters)) != -1) {
		// Look for $envname in parameters of comand line and change for
		// envoirment variable.
		change_env(parameters , found);
	}

	if (found == 0)
		// envname is the name of command line.
		cmd = parameters[found];

	// Command in work directory.
	execv(cmd , parameters);


	/* Look for in path the command */
	envpath = strdup(getenv("PATH"));
	if (envpath == NULL)
		err(1 , "Error create strdup of envpath %s\n", strerror(errno));

	numpath = mytokenize(envpath , path , MAXPATH , ":");
	for (i = 0 ; i < numpath ; i++){

		cmdpath = malloc(MAXPATH);
		if (cmdpath == NULL)
			err(1, "Error crate name cmdpath : %s" , strerror(errno));

		strcat(cmdpath , path[i]);

		if (!endpath(cmdpath))
			strcat(cmdpath , "/");

		strcat(cmdpath , cmd);

		/* Launch the command from PATH */
		execv(cmdpath , parameters);
	}

	// Other path to find a command ?

	fprintf(stderr, "Error launch command : %s %s\n", cmd , strerror(errno));
	return -1;
}

/*
 * built-in -> Change directory of test not is a process is  a function or
 * command inside the process which launched
 */
static char*
changeDir (char *path)
{
	char *savepath = getenv("PWD");

	if (savepath == NULL) {
		fprintf(stderr, "%s\n", "Error env PWD not exsit");
		return NULL;
	}

	savepath = strdup(savepath);
	if (savepath == NULL) {
		// Save path in memory to comeback initial directory.
		fprintf(stderr, "Not enougth memory to savepath in chagedir... %s\n", strerror(errno));
		return NULL;
	}

	if (chdir(path) < 0) {
		fprintf(stderr, "Not found directory %s\n", strerror(errno));
		return NULL;
	}

	// Return the path saved, changed directory was well.
	return savepath;
}

/*
 * Results of commands  in test.tst Standard ouput and Standard errors to
 * file test.out
 */
static int
outputest(int fdOut , int readLastPipe)
{
	int  nr = 0 , nw = 0 , bytesreads = 0;
	char buffer[64*1024];				// Stack buffer to read 64 KB.

	/* Write on file test.out final results of last pipe.*/
	for (;;) {
		nr = read(readLastPipe , buffer , sizeof buffer);

		if (nr == 0)
			break;

		if (nr < 0){
			fprintf(stderr, "Error read input lastpipe... %s\n", strerror(errno));
			return -1;
		}

		nw = write(fdOut , buffer , nr);
		if (nw != nr) {
			fprintf(stderr, "Error write in file test.out... %s\n", strerror(errno));
			return -1;
		}
		bytesreads += nr;
	}
	return bytesreads;
}

static int
cmd_line_pipe (struct T_CMDPIPE *cmdpipe)
{
	int pid = 0 , fdpipe[2];

	// Create Pipe.
	if (pipe(fdpipe) == -1){
		fprintf(stderr, "Error create pipe... %s\n", strerror(errno));
		return -1;
	}

	switch (pid = fork()){

	// Failed create child process with command and pipe.
	case -1:

		fprintf(stderr, "Error create child process... %s\n", strerror(errno));
		return -1;

	// Child process with command of line and pipe.
	case  0:

		if (stdin_to_fd(cmdpipe->readLastPipe) < 0)
			// Standard input next-to-last-pipe created we have to read last
		  	// output of pipe in this process.
			exit(1);

		if (stdout_to_fd(fdpipe[1]) < 0)
			// Standard Output of process to write pipe.
			exit(1);

		if (close(fdpipe[0]) < 0)
			// Close read pipe in this process necessary to not block the program
			err (1 ,"Error closing files possible a pipe reader... %s\n", strerror(errno));


		if (exec_cmd(cmdpipe->cmd , cmdpipe->parameters) < 0)
			// Launch command line in this process prepare files to launch. */
			exit(1);

	// Parent pid child process return by fork and pipe to write must be closed in parent process.
	default:

		// Save pipe to read. Next command or final output.
		cmdpipe->readpipe = fdpipe[0];
		if (close(fdpipe[1]) < 0) {
			fprintf(stderr, "Error closing write pipe error... %s\n", strerror(errno));
			return -1;
		}

		// PID of command line launched like child process.
		return pid;
	}
}


/*
 * Comeback direcotry cunit dir, if we chage the directory
 * to create in cunit directory, test.out, and compare or
 * create test.ok of test.tst.
 */
static int
chdirpwd  (char *pwdcunit)
{
	if (pwdcunit) {
		if (chdir(pwdcunit) < 0){
			fprintf(stderr, "Error change directory with path... %s error... %s\n" , pwdcunit , strerror(errno));
			// Is possible chage to other directory with extension :).
			free(pwdcunit);
			return -1;
		}
		free(pwdcunit);
	}
	return 0;
}

static int
iscd (char *cmd , int numcmd)
{
	return numcmd == 0 && strcmp(cmd , "cd") == 0;
}

static struct T_CMDPIPE*
init_cmd_pipe()
{
	struct T_CMDPIPE *cmdpipe = NULL;

	cmdpipe = malloc ( sizeof(T_CMDPIPE) );
	if (cmdpipe == NULL){
		fprintf(stderr , "Error allocate memory to cmdpipe : %s" , strerror(errno));
		return NULL;
	}

	/* Command values...  */
	cmdpipe->cmd = NULL;
	initokens(cmdpipe->parameters , MAXTOKENS);
	cmdpipe->numpars = 0;


	/* Read lines of test.tst and put in pipes each command... */
	cmdpipe->readpipe = 0;
	cmdpipe->readLastPipe = 0;

	return cmdpipe;
}

static int
waitResults (int pidLast)
{
	int pid , cmdfailed = 0 , status;

	if ((pid = waitpid(pidLast , &status , 0)) != -1) {
		// Wait for childrens if errors in process are by commands process failed...
		// the last command is taken like cmdfailed != -1 necesary exist pidLast children.
		cmdfailed = 0;
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
				cmdfailed = 1;
	}

	return cmdfailed;
}

static int
exectest (char *test , int fdOut)
{
	FILE *fp = openfile(test , "r");
	char *line ,  *lp = NULL , *pwdcunit = NULL;
	int numLinecmd = 0 , pidLast = -1;
	struct T_CMDPIPE *cmdpipe = init_cmd_pipe();

	if (fp == NULL || cmdpipe == NULL) return -1;

	line = malloc(MAXLINE);
	if (line == NULL) {
		fprintf(stderr, "Failed allocate memory to line : %s\n", strerror(errno));
		return -1;
	}

	// Reading test.tst
	while ((lp = fgets(line , MAXLINE , fp)) != NULL) {

		cmdpipe->numpars = mytokenize(lp , cmdpipe->parameters , MAXTOKENS , " \t\n\r");

		if (cmdpipe->numpars == 0)
			// Empty line.
			continue;

		cmdpipe->cmd = cmdpipe->parameters[0];

		if (!iscd(cmdpipe->cmd , numLinecmd)){

			if((pidLast = cmd_line_pipe(cmdpipe)) < 0)
				// Errors only for launch in process Operating System calls...
				return -1;
			cmdpipe->readLastPipe = cmdpipe->readpipe;

		} else {
			if ((pwdcunit = changeDir(cmdpipe->parameters[1])) == NULL)
				// Errors only for change directory in Operating System calls...
				return -1;
		}
		++numLinecmd;
	}
	free(line);

	if (!numLinecmd) {
		// Case of empty file.
		// No commands failed.
		free(cmdpipe);
		fclose(fp);
		return 0;
	}

	if (chdirpwd(pwdcunit) < 0) return -1;

	// Finals results of test.tst to test.out.
	if (outputest(fdOut , cmdpipe->readLastPipe) < 0) return -1;

	close(cmdpipe->readLastPipe);
	free(cmdpipe);
	fclose(fp);

	// Return results of last command.
	return waitResults(pidLast);;
}

static int
isregfile(char *file)
{
	struct stat sb;

	if (stat(file , &sb) < 0) {
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

static int
is_test_in_files (struct T_FILES *files , char *test)
{
	int pos = 0 , found = 0;

	while (pos < files->numfiles && !found)
		if (strcmp(test , files->files[pos]) == 0)
			found = 1;
		else
			pos++;

	return found;
}

static int
size_aligment (int size)
{
	return (size + size%ALIGN);
}

static int
fileToStr (char *test , char **str)
{
	char *new = NULL;
	char buffer[128*1024];
	int fd = 0, nr = 0 , bytesreads = 0 , size = 0;

      if ((fd = open(test , O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file : %s error... %s\n" , test , strerror(errno));
		return -1;
	}

	// Read file to convert in string on memmory.
	*str = NULL;
      int count = 0;
	for (;;) {

		nr = read(fd , buffer , sizeof(buffer) - BYTEZERO);
		buffer[nr] = 0;	// This is very important buffer must be a string
					// else call to strncat will fail and cunit will
					// undefined behavior occurs.

		if (nr < 0) {
			fprintf(stderr, "Error reading test... %s\n", strerror(errno));
			return -1;
		}

		if (nr == 0)
			break;

		bytesreads += nr;
		size  = size_aligment(bytesreads + BYTEZERO);

		new = realloc(*str , size);
            if (new == NULL) {
			fprintf(stderr, "Failed realloc memory to file to string... %s\n", strerror(errno));
			free(str);
			return -1;
            } else
			*str = new;

		if (strncat(*str , buffer , size) == NULL){
			fprintf(stderr, "Error concat file read in string file to string... %s\n", strerror(errno));
			return -1;
		}
            count++;
	}

	// Not necesary null final byte, strncat and strncpy put null bytes in
	// string, on memory not used if not found one in size of n.
	if (close(fd) < 0) {
		fprintf(stderr, "Error closing file : %s error... %s\n" , test , strerror(errno));
		return -1;
	}

	return 0;
}

static int
comparetest (char *testout , char *testok)
{
	char *strOk = NULL , *strOut = NULL;
	int sames = 0;

	if (fileToStr(testout , &strOut) < 0)
		return -1;

	if (fileToStr(testok , &strOk) < 0)
		return -1;

	if (strcmp(strOk , strOut) == 0)
		sames = 1;

	// Empty files.
	if (strOk == NULL && strOk == NULL)
		sames = 1;

	if (!sames) {
		fprintf(stderr, "%s\n", "**************");
		fprintf(stderr, "%s", strOut);
		fprintf(stderr, "%s", strOk);
		fprintf(stderr, "%s\n", "**************");
	}

	free(strOut);
	free(strOk);

	return sames;
}


static int
createok (char *testok , char *testout)
{
	int fdOut = 0 , fdOk = 0 , nr = 0 , nw = 0;
	char buffer[128*1024];

	/* Create a new file test.ok */
	if ((fdOk = creat(testok , 0600)) < 0) {
		fprintf(stderr, "Error create file %s error... %s\n", testok , strerror(errno));
		return -1;
	}

	/* Open test.out to read and copy in test.ok */
	if ((fdOut = open(testout , O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file %s error... %s\n", testout , strerror(errno));
		return -1;
	}

	for (;;) {

		// Copy file test.out in test.ok .
		nr = read(fdOut , buffer , sizeof buffer);
		if (nr < 0) {
			fprintf(stderr, "Error reading bytes from %s error... %s\n", testout , strerror(errno));
			return -1;
		}

		if (nr == 0)
			// File read.
			break;

		nw = write(fdOk , buffer , nr);
		if (nw != nr) {
			fprintf(stderr, "Error writing in file...%s error...%s\n", testok , strerror(errno));
			return -1;
		}
	}

	if (close(fdOut) < 0 || close(fdOk) < 0) {
		fprintf(stderr, "Error closing files test.ok or test.out%s \n", strerror(errno));
		return -1;
	}

	return 0;
}

static int
checkok (struct T_FILES *files , char *testout)
{
	char *testok =  changeext(testout , EXTOUT , EXTOK);
	int sames = 0;


	if (is_test_in_files(files , testok)) {

		// compare really test.ok with test.out files.
		if ((sames = comparetest(testout , testok)) < 0)
			exit(1);

	} else {

		// create test.ok with test.out content
		if (createok(testok , testout) < 0)
			exit(1);

		// test.ok and test.out are sames.
		// test.ok is create with test.out content.
		sames = 1;
	}

	return sames;
}

static int
launchtest (char *test , struct T_FILES *files , int timeout)
{
	char *testout;
	int pid = 0 , fdOut = 0 , cmdfailed = 0 , sames = 0;


	switch (pid = fork()) {
	case -1:

		fprintf(stderr , "Error launch fork process test... %s error...%s\n" , test , strerror(errno));
		return -1;

	case  0:

		opttimeout(timeout);

		if (fd_to_devnull(0) < 0)
		  // Input test to /dev/null .
			exit(1);

		// Create a new file test.out of test.tst to write Standard output and
		// Standard Errors in file test.out of read test.tst
		testout = malloc(MAXNAME);
		if (testout == NULL)
			err (1 , "Error create file name test.out in launchtest : : %s\n", strerror(errno));

		testout = changeext(test , EXTTST , EXTOUT);

		if ((fdOut = creat(testout , 0600)) < 0)
			err (1 , "Error create file test.out... %s error... %s\n", testout , strerror(errno));

		// Redirect test.tst standard output to test.out
		if ((fdOut = stdout_to_fd(fdOut)) < 0)
			exit(1);

		// Redirect test.tst standard error to test.out
		if ((fdOut = stderr_to_fd(fdOut)) < 0)
			exit(1);

		/* Execute test.tst on directory cmdfailed is of last command */
		if ((cmdfailed = exectest(test , fdOut)) < 0)
			exit(1);

		// Finalize close file with written results of test.tst in test.out.
		if (close(fdOut) < 0)
			err (1 , "Error closing file test.out... %s error... %s\n", testout , strerror(errno));

		// If last command failed, test failed,
		// we have not to compare test.ok and test.out
		if (!cmdfailed)
			sames = checkok(files , testout);

		free(testout);

		// Result of test no errors in operating system calls..
		if (cmdfailed || !sames)
			exit(1);
		exit(0);

	default:

		return pid;
	}
}

/*
static void
printTestsPids (struct T_PIDTEST *pidstests , int numtests)
{
	int i;

	for (i = 0 ; i < numtests ; i++) {
		printf("name test : %s\t" , pidstests[i].test);
		printf("pid test  : %d\n" , pidstests[i].pid);
	}
}
*/

static char*
findtest (struct T_PIDTEST *pidstests , int pid , int numtests)
{
	int pos = 0 , found = 0;
	char *test = NULL;

	while (!found && pos < numtests) {

		if (pidstests[pos].pid == pid) {
			test = pidstests[pos].test;
			found = 1;
		} else
			pos++;
	}

	return test;
}

static void
freefiles (T_FILES *files)
{
	int i;
	for (i = 0; i < files->numfiles ; i++)
		free(files->files[i]);
}

static int
is_ext_basic_test (char *name)
{
	return (isext(name , EXTTST) || isext(name ,  EXTOK));
}

static struct T_FILES*
initfiles()
{
	struct T_FILES *files = malloc ( sizeof (T_FILES) );

	if (files == NULL) {
		fprintf(stderr, "Error allocate memmory to struct files. %s\n", strerror(errno));
		return NULL;
	}

	initokens(files->files , MAXFILES);
	files->numfiles = 0;

	return files;
}

static void
addfile (struct T_FILES *files , char *file)
{
	/* New file name to basic cunit to memory...*/

	files->files[files->numfiles] = strdup(file);
	if (files->files[files->numfiles] == NULL)
		err(1 , "Error strdup in files ... %s\n", strerror(errno));

	files->numfiles++;
}

int
savefiles_basic(char *path , T_FILES *files)
{
	DIR *directory;
	struct dirent *rdirectory;

	if ((directory = opendir(path)) == NULL) {
		fprintf(stderr, "Error open dir... %s error... %s\n", path , strerror(errno));
		return -1;
	}

	// Take files and save to compare test.ok
	// if is there in directory this kind of files.
	while((rdirectory = readdir(directory)) != NULL && files->numfiles < MAXFILES)
		if (is_ext_basic_test(rdirectory->d_name) && isregfile(rdirectory->d_name))
			addfile(files , rdirectory->d_name);

	return 0;
}

static int
cunit_basic_test(char *path , int timeout)
{
	struct T_PIDTEST *pidstests = malloc (sizeof (T_PIDTEST) * MAXTESTS);
	struct T_FILES *files = initfiles();
	int cunitfailed = 0 , pid = 0 , i ,  status , numtests = 0;
	char *test = NULL;

	if (pidstests == NULL || files == NULL) {
		fprintf(stderr, "Error allocate memory to structs pidstests or files %s\n", strerror(errno));
		return -1;
	}

	if (savefiles_basic(path , files) < 0) return -1;

	// Launch test.tst and pass argument files to found test.ok on directory.
	for (i = 0 ; i < files->numfiles ; i++) {
		if (isext(files->files[i] , EXTTST)) {
			test = files->files[i];
			if ((pid = launchtest(test , files , timeout)) < 0) {
				fprintf(stderr, "Error launch test process on system call fork... %s\n" , files->files[i]);
				cunitfailed = -1;  /* Operating system call errors -1 */
			}
			pidstests[numtests].test = files->files[i];
			pidstests[numtests].pid  = pid;
			++numtests;
		}
	}

	/* Wait for childrens tests.tst of cunit.c*/
	while ((pid = wait(&status)) != -1) {
		test = findtest(pidstests , pid , numtests);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			// Results of test failed.
			fprintf(stdout, "TEST FAILED  : %s \n", test);
			cunitfailed = 1;
		} else
			fprintf(stdout, "TEST SUCCESS : %s \n", test);
	}

	// Free memory reserved to names of files in directory of cunit.
	// and results saved in pidstests.
	freefiles(files);
	free(pidstests);
	free(files);
	return cunitfailed;
}

static int
is_ext_clean(char *name)
{
	return (isext(name , EXTOK) || isext(name , EXTOUT));
}

static int
cunitclean (char *path)
{
	DIR *directory = NULL;
	struct dirent *rdirectory = NULL;

	if ((directory = opendir(path)) == NULL){
		fprintf(stderr, "Error open dir... %s error... %s\n", path , strerror(errno));
		return -1;
	}

	while ((rdirectory = readdir(directory)) != NULL) {
		if (is_ext_clean(rdirectory->d_name) && isregfile(rdirectory->d_name)) {
			if (unlink(rdirectory->d_name) < 0) {
				fprintf(stderr, "Error unlink file... %s\n", strerror(errno));
				return -1;
			}
		}
	}
	return 0;
}

static int
cmd_cond (char *testcond , char *cmd , char **parameters , int timeout)
{
	char *testout;
	int pid = 0 , fdOut = 0 , condsuccess = 0 , status = 0;

	switch(pid = fork()){

	case -1:

		fprintf(stderr, "Error create fork %s\n", strerror(errno));
		return -1;

	case 0:

		opttimeout(timeout);

		/* Create file test.out same that basic cunit program */
		testout = changeext(testcond , EXTCOND , EXTOUT);
		if ((fdOut = creat(testout , 0600)) < 0) {
			fprintf(stderr, "Error create file ... %s error... %s\n" , testout , strerror(errno));
			exit(1);
		}

		if (stdout_to_fd(fdOut) < 0)
			exit(1);

		if (stderr_to_fd(fdOut) < 0)
			exit(1);

		if (close(fdOut) < 0)
			err (1 , "Error closing file... %s error... %s\n", testout , strerror(errno));

		if (exec_cmd(cmd , parameters) < 0)
			exit(1);

	default:

		/* Wait command per line in test.cond */
		if ((pid = wait(&status)) != -1) {
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
				condsuccess = 1;
		}
	}
	return condsuccess;
}


static int
testcond (char *testcond , struct T_FILES *files , int timeout)
{
	FILE *fp = openfile(testcond , "r");
	char *lp = NULL , *line , *parameters[MAXTOKENS] , *cmd = NULL;
	int condsuccess = 0 , numline = 0;

 	initokens(parameters , MAXTOKENS);

	if (fp == NULL) return -1;

	if (( line = malloc(MAXLINE) ) == NULL) {
		fprintf(stderr, "Error in line not allocate memory to line : %s\n", strerror(errno));
		return -1;
	}

	/* Read lines command of test.cond */
	while (!condsuccess && (lp = fgets(line , MAXLINE , fp)) != NULL) {

		mytokenize(lp , parameters , MAXTOKENS , " \t\n");
		cmd = parameters[0];

		/* launch line command and wait results before launch the next command*/
		if ((condsuccess = cmd_cond(testcond , cmd , parameters , timeout)) < 0)
			return -1;

		numline++;
	}
	free(line);
	fclose(fp);

	if (condsuccess)
		// The test cond same like basic cunit check ok files.
		condsuccess = checkok(files , changeext(testcond , EXTCOND , EXTOUT));

	/* test.cond results */
	if (!condsuccess)
		fprintf(stdout , "TEST COND FAILED  : %s\n" , testcond);
	else
		fprintf(stdout,  "TEST COND SUCCESS : %s\n" , testcond);

	return condsuccess;
}

static int
is_ext_cond (char *name)
{
	return (isext(name , EXTCOND) || isext(name , EXTOK));
}

static void
savefiles_cond (T_FILES *files , char *path)
{
	DIR *directory = NULL;
	struct  dirent *rdirectory = NULL;

	if ((directory = opendir(path)) == NULL)
		err (1 , "Error open dir... %s error... %s\n", path , strerror(errno));


	/* Take files of directory test.out test.cond */
	while((rdirectory = readdir(directory)) != NULL && files->numfiles < MAXFILES){
		if (is_ext_cond(rdirectory->d_name) && isregfile(rdirectory->d_name)) {
			/* New file name of cunit cond to memory...*/
			addfile(files , rdirectory->d_name);
		}
	}

}

static int
cunitcond(char *path , int timeout)
{
	struct  T_FILES *files = initfiles();
	int i , condsuccess = 1;
	char *test;

	if (files == NULL) return -1;

	savefiles_cond(files , path);

	/* Launch cond and pass files test.out to open or create file test.out */
	for (i = 0 ;  i < files->numfiles ; i++) {
		if (isext(files->files[i] , EXTCOND)) {
			test = files->files[i];
			if ((condsuccess = testcond(test , files , timeout)) < 0)
				return -1;
		}
	}

	/* Free memory of files test.out and test.cond */
	freefiles(files);
	free(files);
	return condsuccess;
}

static int
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
areincompatibles (const char **argv)
{
	int ist = 0 , isc = 0;

	while(*argv != NULL && !(ist && isc)) {
		if (strcmp(*argv , "-c") == 0)
			isc = 1;
		if (strcmp(*argv , "-t") == 0)
			ist = 1;
		argv++;
	}
	return ist && isc;
}

int
main (int argc , char const **argv)
{
	char *path = ".";
	int cunitfailed = 0 , condsuccess = 1 , istimeout = 0 , timeout = 0;

	/* Drop argument program name */
	++argv;
	--argc;

	if (argc > 0) {
		/* Optional Parameters */
		if (areincompatibles(argv)) {
			fprintf(stderr, "Error program -t and -c are incompatibles parameters...\n");
			exit(1);
		}

		if (argc == 1 && strcmp(argv[0] , "-c") == 0){
			/* Optional Clean -c */
			if (cunitclean(path) < 0)
				exit(1);
			exit(0);
		}

		if (argc == 2 && strcmp(argv[0] , "-t") == 0 && aredigits(argv[1])) {
			/* Optional Time  -t */
			timeout = atoi(argv[1]);
			istimeout = 1;
		}

		if (!istimeout) {
			/* Bad parameters */
			fprintf(stderr, "Bad parameters... \n");
			fprintf(stderr, "cunit -c [clean]  \n");
			fprintf(stderr, "cunit -t 10 [timeout cunit] \n");
			fprintf(stderr, "cunit [basic cunit and test.cond]\n");
			exit(1);
		}
	}

	/* Basic cunit */
	if ((cunitfailed = cunit_basic_test(path , timeout)) < 0)
		exit(1);		// Errors in cunit test basic


	/* Optional ext .cond */
	if ((condsuccess = cunitcond(path , timeout)) < 0)
		exit(1);		// Errors in cunit test cond.


  	// Optional cond success and basic cunit. if you wanna execute basic alone
	// part comment cunitcond, optional parameters, and drop variable condsuccess.

	if (cunitfailed  || !condsuccess)
		exit(1);		// Final results of cunit test.
	exit(0);

}
