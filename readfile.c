//License: Apache License 2.0
//See LICENSE for details
//Authors (in alphabet order of last name):
// - Qijiang Fan <fqj1994@gmail.com>
// - Bin He <binhe22@gmail.com>
// - Cheng Zhang <chengzhang@hustunique.com>
// - Shiwei Zhou <shwzhou@hustunique.com>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct fileStruct {
	char fileName[256];
	long int fileSize;
	int fileMode;
	struct fileStruct *next;
} fileStruct;
extern int verbose_mode;

void SwitchMemory(int *currentMemory, void **writeMemory, void *memory1,
		  void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr)
{
	*currentMemory = -(*currentMemory);	//可以在这个函数中应用读写锁
	switch (*currentMemory) {
	case -1:
		(*(int*)memory2) = 2;
		pthread_mutex_unlock(m2lockr);
		pthread_mutex_lock(m1lockw);
		pthread_mutex_lock(m1lockr);
		*writeMemory = memory1 + sizeof(int);
		break;
	case 1:
		(*(int*)memory1) = 2;
		pthread_mutex_unlock(m1lockr);
		pthread_mutex_lock(m2lockw);
		pthread_mutex_lock(m2lockr);
		*writeMemory = memory2 + sizeof(int);
		break;
	default:
		fprintf(stderr, "switch mem error");
		exit(1);
		break;
	}
	//printf("Read Switch %d\n", *currentMemory);
}


void ReadFile(struct fileStruct * list, long bufSize, void *memory1, void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr){
	bufSize -= sizeof(int);
	FILE *fp;
	long int remainMemory = bufSize;
	//void *memory1=malloc(bufSize);            // malloc first 128MB memory
	//void *memory2=malloc(bufSize);            // malloc second 128MB memory

	//void **storeMemory=malloc(2*sizeof(void *));
	//storeMemory[0]=memory1;                       //将要已分配好的内存地址存入以供返回
	//storeMemory[1]=memory2;

	int currentMemory = -1;	//标志当前内存
	void *writeMemory = memory1 + sizeof(int);

	fileStruct *ergodic = list;
	while (ergodic != NULL) {
		//printf("Read remain %d\n", remainMemory);
		if (verbose_mode) {
			printf("read file:%s\n",ergodic->fileName);
		}
		if (sizeof(fileStruct) < remainMemory) {
			memcpy(writeMemory, ergodic, sizeof(fileStruct));	//写文件信息
			remainMemory = remainMemory - sizeof(fileStruct);
			writeMemory = writeMemory + sizeof(fileStruct);
			fp = fopen(ergodic->fileName, "r");
			if (ergodic->fileSize < remainMemory) {
				fread(writeMemory, ergodic->fileSize, 1, fp);
				remainMemory = remainMemory - ergodic->fileSize;
				writeMemory = writeMemory + ergodic->fileSize;
				ergodic = ergodic->next;
				fclose(fp);
				continue;
			} else if (ergodic->fileSize == remainMemory) {
				fread(writeMemory, ergodic->fileSize, 1, fp);
				remainMemory = bufSize;
				SwitchMemory(&currentMemory, &writeMemory,
					     memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
				ergodic = ergodic->next;
				fclose(fp);
				continue;
			}
			fread(writeMemory, remainMemory, 1, fp);
			long int hasRead = remainMemory;
			SwitchMemory(&currentMemory, &writeMemory, memory1,
				     memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			remainMemory = bufSize;
			//int writeTimes=0;
			int writeTimes = ceil((ergodic->fileSize - hasRead) / (float)bufSize);	//改编译参数
			int i;
			for (i = 1; i < writeTimes;i++) {
				fseek(fp, hasRead + bufSize * (i - 1),
				      SEEK_SET);
				fread(writeMemory, bufSize, 1, fp);
				SwitchMemory(&currentMemory, &writeMemory,
					     memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			}
			fseek(fp, hasRead + bufSize * (writeTimes - 1),
			      SEEK_SET);
			fread(writeMemory,
			      ergodic->fileSize - (hasRead +
						   bufSize * (writeTimes - 1)),
			      1, fp);
			writeMemory += ergodic->fileSize - (hasRead + bufSize * (writeTimes -1));
			remainMemory =
			    bufSize - (ergodic->fileSize -
				       (hasRead + bufSize * (writeTimes - 1)));
			if (remainMemory == 0) {
				remainMemory = bufSize;
				SwitchMemory(&currentMemory, &writeMemory,
					     memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
				ergodic = ergodic->next;
				fclose(fp);
				continue;
			}
			ergodic = ergodic->next;
			fclose(fp);
		} else {
			remainMemory = bufSize;
			SwitchMemory(&currentMemory, &writeMemory, memory1,
				     memory2, m1lockw, m1lockr, m2lockw, m2lockr);
		}
	}
	if (currentMemory == -1) { pthread_mutex_unlock(m1lockr); *(int*)memory1 = 1;}
	else { pthread_mutex_unlock(m2lockr); *(int*)memory2 = 1;}
}
