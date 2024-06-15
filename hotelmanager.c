#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>

#define Earnings "earnings.txt"
#define BUFF_SIZE 200
#define MAX_TABLES 10

// Function for cleanup by detaching shared memory
void cleanup(char *shmptr) {
    if (shmdt(shmptr) == -1) {
        perror("Error in shmdt\n");
        exit(-1);
    }
}

int main(){
    char *hmadm_shmptr, hmadmbuff[BUFF_SIZE]; // Shared memory pointer and buffer for admin and hotel manager
    int hmwbuff[2];   // Buffer for waiter and hotel manager communication
    int hmadm_shmid;  // Shared memory ID for admin and hotel manager
    
    // Creating key to connect to admin and hotel manager
    key_t key = ftok("admin.c", 0);
    if (key == -1) {
        printf("Error in executing ftok1\n");
        exit(-1);
    }
    
    // Getting the shmid of the already created shared memory segment between admin and hotel manager
    hmadm_shmid = shmget(key, BUFF_SIZE, 0666 | IPC_CREAT);
    if (hmadm_shmid == -1) {
        printf("Error in executing shmget\n");
        exit(-2);
    }
    
    // Attaching hotel manager to the shared memory segment between admin and hotel manager
    hmadm_shmptr = (char*)shmat(hmadm_shmid, NULL, 0);
    if(hmadm_shmptr == (void*)-1) {
        perror("Error in shmPtr in attaching the memory segment\n");
        return 1;
    }

    int totalbill = 0, waiter_wages = 0, profit = 0;
    int noofWaiters;
    printf("Enter Total Number Of Tables at the Hotel\n");
    scanf("%d", &noofWaiters);
    
    // Storing the keys, shmid's and shmptr's between all waiters and Hotel manager
    key_t waiter[noofWaiters + 1];
    int hmw_shmid[noofWaiters + 1];
    int* hmw_shmptr[noofWaiters + 1];

    key_t table[noofWaiters + 1];
    int hmt_shmid[noofWaiters + 1];
    int* hmt_shmptr[noofWaiters + 1];
    
    // Clearing the earnings file
    if(1){
        FILE *fp = fopen(Earnings, "w");
        fclose(fp);
    }

    for(int i = 1; i <= noofWaiters; i++) {
        // Creating key to connect to waiter[i] and hotel manager
        waiter[i] = ftok("manager.c", i);
        if(waiter[i] == -1) {
            printf("Error in executing ftok2\n");
            exit(-1);
        }
        
        // Creating a shared memory segment between waiter[i] and hotel manager
        hmw_shmid[i] = shmget(waiter[i], 10, 0666 | IPC_CREAT);
        if (hmw_shmid[i] == -1) {
            printf("Error in executing shmget\n");
            exit(-2);
        }
        
        // Attaching hotel manager to the shared memory segment between waiter[i] and hotel manager
        hmw_shmptr[i] = (int*)shmat(hmw_shmid[i], NULL, 0);
        if(hmw_shmptr[i] == (void*)-1) {
            perror("Error in shmPtr in attaching the memory segment\n");
            exit(-1);
        }

        // Similar operations for table communication
        table[i] = ftok("table.c", i);
        if(table[i] == -1) {
            printf("Error in executing ftok3\n");
            exit(-1);
        }
        
        hmt_shmid[i] = shmget(table[i], 10, 0666 | IPC_CREAT);
        if (hmt_shmid[i] == -1) {
            printf("Error in executing shmget\n");
            exit(-2);
        }
        
        hmt_shmptr[i] = (int*)shmat(hmt_shmid[i], NULL, 0);
        if(hmt_shmptr[i] == (void*)-1) {
            perror("Error in shmPtr in attaching the memory segment\n");
            exit(-1);
        }

        hmt_shmptr[i][0] = 0;
        hmw_shmptr[i][0] = -1; // Initializing tableno/waiterno to -1 to indicate no communication
    }
    
    // Loop to read from shared memories of hotel manager and waiters
    while(1) {
        if(hmadm_shmptr[0] != '\0') { // Admin has issued a command
            wait((int*)50);
            strcpy(hmadmbuff, hmadm_shmptr);
            printf("Admin has issued command: %s\n", hmadmbuff);
            memset(hmadm_shmptr, '\0', BUFF_SIZE);

            if(strcmp(hmadmbuff, "close") == 0) { // Closing the hotel
                // Wait until all tables are free
                while(1) {
                    int res = 0;
                    for(int i = 1; i <= noofWaiters; i++) {
                        struct shmid_ds shminfo;
                        if (shmctl(hmt_shmid[i], IPC_STAT, &shminfo) == -1) {
                            perror("shmctl");
                            exit(EXIT_FAILURE);
                        }
                        res += shminfo.shm_nattch;
                    }
                    if(res == noofWaiters) break;
                }

                // Calculating total earnings and processing earnings for each table
                for(int i = 1; i <= noofWaiters; i++) {
                    if(hmw_shmptr[i][0] != -1) { // Waiter has communicated
                        int table_number = hmw_shmptr[i][0];
                        int table_bill = hmw_shmptr[i][1];
                        totalbill += table_bill;
                        memset(hmw_shmptr[i], -1, 2 * sizeof(int)); // Clearing shared memory
                        FILE *fp = fopen(Earnings, "a"); // Appending earnings to file
                        if(fp == NULL) {
                            printf("Error opening the earnings file\n");
                            return -1;
                        }
                        fprintf(fp, "Earnings from Table %d: %d\n", table_number, table_bill);
                        fclose(fp);
                    }
                }
                
                // Calculating waiter wages and net profit
                waiter_wages = 0.4 * totalbill;
                profit = totalbill - waiter_wages;
                
                // Cleaning up shared memory and exiting
                for(int i = 1; i <= noofWaiters; i++) {
                    if (shmdt(hmw_shmptr[i]) == -1 || shmdt(hmt_shmptr[i]) == -1) {
                        perror("Error in shmdt\n");
                        exit(-1);
                    }
                    if(shmctl(hmw_shmid[i], IPC_RMID, NULL) == -1 || shmctl(hmt_shmid[i], IPC_RMID, NULL) == -1) {
                        perror("Error in executing shmctl\n");
                        exit(-1);
                    }
                }
                
                // Appending total earnings, waiter wages, and net profit to earnings file
                FILE *fp = fopen(Earnings, "a");
                if(fp == NULL) {
                    printf("Error opening the earnings file\n");
                    return -1;
                }
                fprintf(fp, "Total Earnings: %d\n", totalbill);
                fprintf(fp, "Waiter Wages: %d\n", waiter_wages);
                fprintf(fp, "Net Profit: %d\n", profit);
                fclose(fp); 
                
                // Cleaning up shared memory and exiting
                cleanup(hmadm_shmptr);
                exit(0);
            }
        }
        else { // Checking if any waiter has communicated with hotel manager
            for(int i = 1; i <= noofWaiters; i++) {
                if(hmw_shmptr[i][0] != -1) { // Waiter has communicated
                    int table_number = hmw_shmptr[i][0];
                    int table_bill = hmw_shmptr[i][1];
                    totalbill += table_bill;
                    memset(hmw_shmptr[i], -1, 2 * sizeof(int)); // Clearing shared memory
                    FILE *fp = fopen(Earnings, "a"); // Appending earnings to file
                    if(fp == NULL) {
                        printf("Error opening the earnings file\n");
                        return -1;
                    }
                    fprintf(fp, "Earnings from Table %d: %d\n", table_number, table_bill);
                    fclose(fp);
                }
            }
        }
    }
}
