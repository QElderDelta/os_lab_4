#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define FILE_NAME_LENGTH 129


void readLine(char* result) {
    char c;
    int byteCounter = 0;
    while(byteCounter < FILE_NAME_LENGTH) {
        if(read(STDIN_FILENO, &c, 1) == -1) {
            perror("Reading failed");
        }
        if(c == '\n') {
            break;
        } else {
            result[byteCounter++] = c;
        }
    }
    result[byteCounter] = '\0';
}

int main() {
    pid_t pid;
    char c;
    int numberOfProcesses = 1;
    char buf[FILE_NAME_LENGTH];
    readLine(buf);
    char* inputSemaphoreName = strdup("/semaphore?input");
    char* outputSemaphoreName = strdup("/semaphore?output");
    if(atoi(buf) > 0) {
        numberOfProcesses = atoi(buf);
    } else {
        printf("Invalid argment - must be a positive number\n");
        exit(-1);
    }
    char* map;
    char fileNames[numberOfProcesses][FILE_NAME_LENGTH];
    int processNumber = 0;
    sem_t* inputSemaphores[numberOfProcesses];
    sem_t* outputSemaphores[numberOfProcesses];
    for(int i = 0; i < numberOfProcesses; ++i) {
        readLine(buf);
        strcpy(fileNames[i], buf);
    }
    for(int i = 0; i < numberOfProcesses; ++i) {
        inputSemaphoreName[10] = '0' + i;
        outputSemaphoreName[10] = '0' + i;
        inputSemaphores[i] = sem_open(inputSemaphoreName, O_CREAT, 777, 0);
        outputSemaphores[i] = sem_open(outputSemaphoreName, O_CREAT, 777, 1);
        if(inputSemaphores[i] == SEM_FAILED || 
                outputSemaphores[i] == SEM_FAILED) {
            perror("Fault during semaphore init");
            return -1;
        }
        sem_unlink(inputSemaphoreName);
        sem_unlink(outputSemaphoreName);
    }
    char* tempFileName = strdup("/tmp/tmp_file.XXXXXX");
    int tempFileDescriptor = mkstemp(tempFileName);
    free(tempFileName);
    int fileSize = numberOfProcesses + 1;
    char temp[fileSize];
    for(int i = 0; i < fileSize; ++i) {
        temp[i] = ' ';
    }
    write(tempFileDescriptor, temp, fileSize);
    if((map = (char*)mmap(NULL, fileSize, PROT_WRITE | PROT_READ, MAP_SHARED, 
                    tempFileDescriptor, 0)) == MAP_FAILED) {
        perror("Mapping failed");
        return -1;
    }
    for(int i = 0; i < numberOfProcesses; ++i) {
       pid = fork(); 
       if(pid == 0) {
            processNumber = i;
            break;
       } else if (pid == -1) {
            perror("Fork failed");
            return -1;
       }
    }
    if(pid > 0) {
        while(read(STDIN_FILENO, &c, 1) > 0) {
            sem_wait(outputSemaphores[processNumber]);
            map[processNumber] = c; 
            sem_post(inputSemaphores[processNumber]);
            processNumber++;
            processNumber %= numberOfProcesses;
        }
        for(int i = 0; i < numberOfProcesses; ++i) {
            map[i] = '\0';
            sem_post(inputSemaphores[i]);
        }
    } else if(pid == 0) {
        int fd = open(fileNames[processNumber], O_CREAT | O_WRONLY, S_IREAD | 
                S_IWRITE);
        if(ftruncate(fd, 0) == -1) {
            perror("truncation failed");
        }
        while(1) {
            sem_wait(inputSemaphores[processNumber]);
            char c;
            c = map[processNumber];
            sem_post(outputSemaphores[processNumber]);
            if(c == '\0') {
                break;
            } else {
                if(write(fd, &c, 1) == -1) {
                    perror("Writing failed");
                }
            }
        }
        close(fd);
        exit(0);
    }
    close(tempFileDescriptor);
    munmap(map, fileSize);
    return 0;
}
