#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Globals  Variables */
int flagP = 0;
char *pixmap;
pthread_mutex_t lock;

enum{
	N = 128,
	MAXTHREADS = 1000,
	MAXVALUE = 255,
	SPACEMAP = 4*4*1024, /* Size of pixmap 128*128 blocks of 4KBs to mmap*/
	MAXSTRERR = 1024,
	POSITIVE = 128,
	WRITEBYTE = 1,
};


void
addpix (char row , char field)
{
	unsigned char result;

	pthread_mutex_lock(&lock);
	result = pixmap[N*row + field]; 
	if (result < MAXVALUE)	
		++pixmap[N*row +field];
	pthread_mutex_unlock(&lock);
}

int
readcontrol (char *file , FILE *fp , int nr)
{
	if (feof(fp) != 0){
		clearerr(fp);
		return 0;
	}

	if (ferror(fp) != 0){
		printf("Error on file inside thread reading file: %s \n" , file);
		clearerr(fp);
		return -1;
	}
	return 0;
}

static void*
tmain (void *args)
{
	int nr;
	char last , nextToLast , *file;
	FILE *fp;
	char *errorthread = "Error thread";

	file = args; /* Conversion type from void* to char* */
	
	fp = fopen(file , "r");
	if (fp == NULL){
		fprintf(stderr , "Error open file inside thread %s : %s\n", file , strerror(errno));
		return errorthread;
	}

	/* First byte */
	nr = fread(&nextToLast , 1 , 1 , fp);
	if (nr == 0){
		if (readcontrol(file , fp , nr) == 0){
			fprintf(stderr , "End of file not enought inside thread bytes : %s\n" , file);
			return errorthread;
		}
		if (readcontrol(file , fp , nr) == -1){
			fprintf(stderr , "Error on file inside thread \n");
			return errorthread;
		}
	}

	/* Follows bytes */
	for (;;){
		nr = fread(&last , 1 , 1 , fp);
	
		if (nr == 0){
			if (readcontrol(file , fp , nr) == 0)
				break;

			if (readcontrol(file , fp , nr) == -1)
				return errorthread;
		}
		if (last != '\n'){
			addpix(nextToLast , last);
			nextToLast = last;
		}
	}
	fclose(fp);
	free(file);
	return NULL;
}

void
showmap ()	
{ 	
	int row = 0, field = 0;
	unsigned char result;

	for (row = 0 ; row < N ; row++){
		for (field = 0; field < N ; field++){
			printf("(%i" , row);
			printf(",%i) : ", field);
			result = pixmap[row*N + field];
			printf("%u\n", result);
		}
	}
}


int 
launchthreads (char **argv , int argc)
{
	char *file , *errth;
	int i , errors = 0 , numthreads = 0;
	void *sts[MAXTHREADS];     /* VECTOR POINTER OF ARGUMETNS RETURNS OF THREADS WITHOUT TYPE */
	pthread_t thr[MAXTHREADS]; /* POINTERS TO THREAD. ID THREAD */
	
	/* Create threads for each file */
	for (i = 0 ; i < argc && i < MAXTHREADS ; i++){

		file = strdup(argv[i]);
		if (pthread_create(thr + i , NULL , tmain , file) != 0){
			fprintf(stderr, "Error create thread : %s\n", strerror(errno));
			errors = -1;
		}else
			++numthreads;
	}

	/* Wait for Threads launched */
	for (i = 0 ; i < argc && i < MAXTHREADS ; i++){	
		if (pthread_join(thr[i] , sts + i) != 0){
				fprintf(stderr, "Errors END THREAD WITH JOIN \n");
				errors = -1;
			}

		/* Check errors in thread */
		errth = sts[i];
		if (errth)
			errors = -1;
	}	
	return errors;
}


int
createmap (char *pixfile)
{
	int fd;
	void *addr;

	fd = creat(pixfile , 0600);
	if (fd < 0){
		fprintf(stderr, "Error create file %s %s\n" , pixfile , strerror(errno));
		return -1;
	}
	lseek(fd , SPACEMAP - WRITEBYTE , SEEK_SET);
	if (write(fd , "" , 1) != 1){
		fprintf(stderr, "Error write in file %s %s\n" , pixfile , strerror(errno));
		return -1;
	}
	close(fd);

	fd = open(pixfile , O_RDWR);
	if (fd < 0){
		fprintf(stderr, "Error open file %s %s\n", pixfile , strerror(errno));
		return -1;
	}

	addr = mmap(NULL , SPACEMAP , PROT_READ | PROT_WRITE , MAP_SHARED | MAP_FILE , fd , 0);
	if (addr == MAP_FAILED){
		fprintf(stderr, "Error create map %s\n", strerror(errno));
		return -1;
	}
	close(fd);
	pixmap = addr;	
	return 0;
}

void
controlparameters (int *argc , char ***argv)
{
	/* Control Parameters */
	if (*argc < 2){
		fprintf(stderr, "Error too few parameters for program %s\n" , *argv[0]);
		fprintf(stderr, "Example : %s pixmap file1 file2 ...\n", *argv[0]);
		exit(1);
	}

	/* Drop program name */
	--*argc;
	++*argv;
	
	/* Activated flagP */
	if (strcmp(*argv[0] , "-p") == 0){
		flagP = 1;
		++*argv;
		--*argc;
	}

	/* Check the file map*/
	if (*argc < 2){
		fprintf(stderr, "Error you must especifed a file to pixmap and a file for pixmap\n");
		fprintf(stderr, "Example : %s pixmap file1 file2 ...\n", *argv[0]);
		exit(1);
	}
}

int 
main(int argc, char *argv[])
{

	/* Check parameters and Drop Name program */
	controlparameters (&argc , &argv);

	/* Create Globbing MAP */
	if (createmap(argv[0]) < 0)
		err(1 , "Error create map");

	/* Drop pixfile name */
	--argc;
	++argv;

	/* Launch trhead per file */
	if (launchthreads(argv , argc) < 0)
		exit(1);

	if (flagP == 1)
		showmap();

	/* Drop memory reserved for globbing  map */
	if (munmap(pixmap , SPACEMAP) < 0)
		err(1 , "Error munmap");

	exit(0);
}