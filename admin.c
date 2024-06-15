#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#define BUFF_SIZE 200

// Function to display a message prompting for closure decision
void displayMessage()
{
    printf("Do you want to close the hotel? Enter Y for Yes and N for No.\n");
}

int main() {
    int adm_shmid, hm_shmid; // Shared memory IDs for admin and hotel manager
    char *shmptr, buff[BUFF_SIZE]; // Shared memory pointer and buffer

    // Creating key for shared memory segment
    key_t key = ftok("admin.c", 0);

    // Error handling for key creation
    if (key == -1) {
        printf("Error in executing ftok\n");
        exit(-1);
    }
    
    // Creating shared memory segment between admin and hotel manager
    adm_shmid = shmget(key, BUFF_SIZE, 0666 | IPC_CREAT);

    // Error handling for shared memory creation
    if (adm_shmid == -1) {
        printf("Error in executing shmget\n");
        exit(-2);
    }
    
    // Attaching admin to the shared memory segment
    shmptr = (char*)shmat(adm_shmid, NULL, 0);

    // Error handling for shared memory attachment
    if (shmptr == (void*)-1) {
        perror("Error in shmPtr in attaching the memory segment\n");
        return 1;
    }

    displayMessage();
    
    // Loop to continue until hotel closure
    while (1) {
        // Prompting user for closure decision
        char decision;
        scanf("%c",&decision);
        
        // Handling closure decision
        if(decision == 'Y' || decision == 'y') {
            // Notifying hotel manager to close
            strcpy(buff,"close");
            int len = strlen(buff);
            if(buff[len - 1] == '\n')
                buff[len - 1] = '\0';
            strcpy(shmptr, buff);

            // Cleanup
            // Detaching admin from shared memory segment
            if (shmdt(shmptr) == -1) {
                perror("Error in shmdt\n");
                exit(-1);
            }
            
            // Freeing shared memory after detaching all processes
            if(shmctl(adm_shmid, IPC_RMID, NULL) == -1) {
                perror("Error in executing shmctl\n");
                exit(-1);
            }
            break;
        }
        // Continuing loop if closure decision is 'N'
        else if(decision == 'N' || decision == 'n') {
            displayMessage();
        }
    }

    return 0;
}
