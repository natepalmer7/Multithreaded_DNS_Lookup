#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>      // Provides the O_CREAT, O_RDWR constants
#include <sys/stat.h>   // Provides the S_IRUSR, S_IWUSR constants
#include <stdlib.h>     // Provides the exit() function
#include "util.h"
#include <semaphore.h>
#include <pthread.h>

#define MAX_DOMAIN_LENGTH 256
#define BUFFER_CAPACITY 8

char domainBuffer[BUFFER_CAPACITY][MAX_DOMAIN_LENGTH];
int recordsPerThread = MAX_DOMAIN_LENGTH / BUFFER_CAPACITY;

int inputIndex = 0;
int outputIndex = 0;
int remainingFiles;
int recordCount = 0;

pthread_mutex_t producerLock;
pthread_mutex_t consumerLock;
pthread_cond_t producerCond;
pthread_cond_t consumerCond;

sem_t consumerSemaphore;
sem_t producerSemaphore;

typedef struct {
    FILE **sourceFiles;
    FILE *destFile;
    FILE *logFile;

    int currentFile;
    int totalFiles;
    int remainingFiles;
} FileHandler;

typedef struct {
    FileHandler *fileDetails;
    int *indices;
} FileReaderArgs;

typedef struct {
    FILE *destination;
    int threadId;
} FileOutputArgs;

int fetchRecord(FileHandler *fileDetails, char *record){
    char *status = fgets(record, MAX_DOMAIN_LENGTH, fileDetails->sourceFiles[fileDetails->currentFile]);

    if (status == NULL){
        remainingFiles--;
        fileDetails->currentFile++;
        return 0;
    }

    return 1;
}

int isBufferEmpty(int buffer[]) {
  for (int i = 0; i < BUFFER_CAPACITY; i++){
    if (buffer[i] != 0 ) {
      return 0;
    }
  }
  return 1;
}

void *processFile(void *argPointers) {
    FileReaderArgs *params = (FileReaderArgs *) argPointers;
    FileHandler *fileDetails = params->fileDetails;
    int *bufferIndices = params->indices;

    char *record = (char *) malloc(MAX_DOMAIN_LENGTH);
    pthread_mutex_lock(&producerLock);
    while(remainingFiles){
        pthread_mutex_unlock(&producerLock);

        pthread_mutex_lock(&producerLock);
        while (fetchRecord(fileDetails, record)) {
            if (inputIndex == 0){
            }
            while (inputIndex >= BUFFER_CAPACITY){
                sem_post(&consumerSemaphore);
                sem_wait(&producerSemaphore);
            }
            strcpy(domainBuffer[inputIndex], record);
            domainBuffer[inputIndex][strcspn(domainBuffer[inputIndex], "\n")] = 0;
            inputIndex++;
            recordCount++;
            fprintf(fileDetails->logFile, "Thread %lu serviced file %d\n", (unsigned long)pthread_self(), fileDetails->currentFile);
        }
        pthread_mutex_unlock(&producerLock);
        usleep(1);
        pthread_mutex_lock(&producerLock);
    }
    sem_post(&consumerSemaphore);
    pthread_mutex_unlock(&producerLock);
    return;
}

void *transformToOutput(void *argPointers){
    FileOutputArgs *params = (FileOutputArgs *)argPointers;
    FILE *destination = (FILE *)params->destination;
    int threadId = params->threadId;
    
    pthread_mutex_lock(&consumerLock); 
    sem_wait(&consumerSemaphore);
    pthread_mutex_unlock(&consumerLock);

    while (remainingFiles){
        pthread_mutex_lock(&consumerLock);
        while(inputIndex != BUFFER_CAPACITY && remainingFiles){
            sem_wait(&consumerSemaphore);
        }

        while (outputIndex < BUFFER_CAPACITY && domainBuffer[outputIndex][0] != '\0') {
            char *address = get_ipv4_address(domainBuffer[outputIndex]);
            fprintf(destination, "%s, %s\n", domainBuffer[outputIndex], address);
            outputIndex++;
        }

        memset(domainBuffer, '\0', sizeof(domainBuffer));
        outputIndex = 0;
        inputIndex = 0;
        sem_post(&producerSemaphore);
        
        pthread_mutex_unlock(&consumerLock);
        usleep(1);
    }
    return;
}

int main(int argc, char *argv[]) {
    int producerCount = 3;
    int consumerCount = 3;
    pthread_t producers[producerCount], consumers[consumerCount];

    pthread_mutex_init(&producerLock, NULL);
    pthread_mutex_init(&consumerLock, NULL);
    pthread_cond_init(&producerCond, NULL);
    pthread_cond_init(&consumerCond, NULL);
    sem_init(&producerSemaphore, 0, 0);
    sem_init(&consumerSemaphore, 0, 1);

    memset(domainBuffer, '\0', sizeof(domainBuffer));
    FILE *destination = fopen("consumer.txt", "w");
    if (destination == NULL) {
        printf("Failed to open consumer file\n"); 
        return 1;
    }
    
    FILE *log = fopen("p_log.txt", "w"); 
    if (log == NULL) {
        printf("Failed to open parser file\n"); 
        return 1;
    }
    

    FileHandler* fileDetails = (FileHandler *)malloc(sizeof(FileHandler));

    fileDetails->sourceFiles = (FILE **)malloc(sizeof(FILE *) * argc - 1);
    fileDetails->logFile = log;

    fileDetails->totalFiles = argc - 1;
    remainingFiles = argc - 1;
    fileDetails->currentFile = 0;

    for (int i = 1; i < argc; i++) {
        FILE *source = fopen(argv[i], "r");
        if (source == NULL) {
            printf("Error opening input_file: %s\n", argv[i]);
            return 1;
        }
        fileDetails->sourceFiles[i-1] = source; 
    }

    int currentIndex = 0;
    for (int i = 0; i < producerCount; i++) {
        FileReaderArgs *params = malloc(sizeof(FileReaderArgs));
        params->fileDetails = fileDetails;
        params->indices = (int *)malloc(sizeof(int) * recordsPerThread);
        int perThread = BUFFER_CAPACITY / producerCount;
        for (int j = 0; j < perThread; j++) {
            params->indices[j] = currentIndex;
            currentIndex++;
        }

        pthread_create(&producers[i], NULL, processFile, (void *)params);
    }
    for (int i = 0; i < consumerCount; i++) {
        FileOutputArgs *params = malloc(sizeof(FileOutputArgs));
        params->destination = destination;

        pthread_create(&consumers[i], NULL, transformToOutput, (void *)params);
    }
    
    for (int i = 0; i < producerCount; i++) {
        pthread_join(producers[i], NULL);
    }

    for (int i = 0; i < consumerCount; i++) {
        pthread_join(consumers[i], NULL);
    }

    for (int i = 0; i < fileDetails->totalFiles; i++) {
        fclose(fileDetails->sourceFiles[i]);
    }

    fclose(destination);
    fclose(log);
    free(fileDetails->sourceFiles);
    free(fileDetails);

    pthread_mutex_destroy(&producerLock);
    pthread_mutex_destroy(&consumerLock);
    pthread_cond_destroy(&producerCond);
    pthread_cond_destroy(&consumerCond);
    sem_destroy(&producerSemaphore);
    sem_destroy(&consumerSemaphore);

    return 0;
}






