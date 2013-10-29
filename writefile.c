#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
typedef struct fileStruct  {
	char fileName[256];
	long int fileSize;
	int fileMode;
	struct fileStruct *next;
} fileStruct;


void switchMemory(int *currentMemory, void **writeMemory, void *memory1,
		    void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr) 
{
	*currentMemory = -(*currentMemory);	//可以在这个函数中应用读写锁
	switch (*currentMemory)
		 {
	case -1:
		pthread_mutex_unlock(m2lockr);
		pthread_mutex_unlock(m2lockw);
		pthread_mutex_lock(m1lockr);
		*writeMemory = memory1 + sizeof(int);
		break;
	case 1:
		pthread_mutex_unlock(m1lockr);
		pthread_mutex_unlock(m1lockw);
		pthread_mutex_lock(m2lockr);
		*writeMemory = memory2 + sizeof(int);
		break;
	default:
		fprintf(stderr, "switch mem error");
		exit(1);
		break;
		}
	//printf("Write Switch %d\n", *currentMemory);
}



int mkpath(char* file_path, mode_t mode) {
   assert(file_path && *file_path);
   char* p;
   for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
     *p='\0';
     if (mkdir(file_path, mode)==-1) {
       if (errno!=EEXIST) { *p='/'; return -1; }
     }
     *p='/';
   }
   return 0;
 }



void WriteFile(void *memory1, void *memory2, int bufSize, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr) 
{
	bufSize -= sizeof(int);
	long int remainMemory = bufSize;
	int currentMemory = -1;	//标志当前内存
	void *writeMemory = memory1 + sizeof(int);
	void *p = writeMemory;
	pthread_mutex_lock(m1lockr);
	while (*(int*)memory1 == 0) { pthread_mutex_unlock(m1lockr); usleep(100); pthread_mutex_lock(m1lockr); }
	while (1) {
		//printf("Write remain %d\n", remainMemory);
		if (remainMemory < sizeof(fileStruct)) {
			switchMemory(&currentMemory, &p, memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			remainMemory = bufSize;
		}
		fileStruct * nowfile = (fileStruct *) p;
		char filename[256]="";
		strcpy(filename,nowfile->fileName);
		printf("will write %s",filename);
		mkpath(filename,0755);	
		int fd = open(filename , O_CREAT, nowfile->fileMode);
		if(!fd){
			printf("create file error");
			exit(1);
		}
		close(fd);
		int f=open(filename,O_WRONLY);
		printf("size:%dmode:%d",nowfile->fileSize,nowfile->fileMode);
		fileStruct * endWrite = nowfile->next;
		remainMemory -= sizeof(fileStruct);
		if (nowfile->fileSize < remainMemory) {
			write(f, nowfile + 1, nowfile->fileSize);
			remainMemory -= nowfile->fileSize;
			p += sizeof(fileStruct);
			p += nowfile->fileSize;
			close(f);
		}
		
		else if (nowfile->fileSize == remainMemory) {
			write(f, nowfile + 1, nowfile->fileSize);
			remainMemory = bufSize;
			switchMemory(&currentMemory, &p, memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			close(f);
		}
		
		else if (nowfile->fileSize > remainMemory) {
			write(f, nowfile + 1, remainMemory);
			int fileSize = nowfile->fileSize;
			switchMemory(&currentMemory, &p, memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			int writeTimes = ceil((nowfile->fileSize - remainMemory) / (float)bufSize);	//改编译参数
			int i = 1;
			for (i = 1; i < writeTimes;i++)
				 {
				write(f, p, bufSize);
				switchMemory(&currentMemory, &p, memory1,
					      memory2, m1lockw, m1lockr, m2lockw, m2lockr);
				}
			int lastSize =
			    fileSize - remainMemory - (writeTimes -
						       1) * bufSize;
			write(f, p, lastSize);
			p += lastSize;
			remainMemory = bufSize - lastSize;
			if (!remainMemory) {
				remainMemory = bufSize;
				switchMemory(&currentMemory, &p, memory1,
					      memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			}
			close(f);
		}
		if (!endWrite)
			break;
	}
	if (currentMemory == -1) { pthread_mutex_unlock(m1lockr); pthread_mutex_unlock(m1lockw); }
	else { pthread_mutex_unlock(m2lockr); pthread_mutex_unlock(m2lockw);}
	
	    /*while(((fileStruct *)readMemory)->next!=NULL)
	       {
	       currentFile=(fileStruct *)readMemory;
	       readMemory=readMemory+sizeof(fileStruct);
	       remainMemory=bufferSize-sizeof(fileStruct);
	       fp=fopen(ergodic->fileName,"r");
	       if(currentFile->fileSize<=remainMemory)
	       {
	       
	       }
	       
	       //} */ 
}


