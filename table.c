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

#define menu "menu.txt"
#define BUFF_SIZE 400

// Struct to hold shared data
typedef struct {
    int flag; 
    int data;
    int all_orders[50];
} SharedData;

// Function to read menu from file and display
void read_menu() {
    FILE *menu_file = fopen(menu, "r");
    if (menu_file == NULL) {
        perror("Error opening menu file");
        exit(EXIT_FAILURE);
    }
    char buffer[1000];
    while (fgets(buffer, sizeof(buffer), menu_file) != NULL) {
        int len = strlen(buffer);
        if (buffer[len - 1] != '\0')
            buffer[len - 1] = '\0';
        printf("%s\n", buffer);
    }
    fclose(menu_file);
}

// Function to take customer orders
int take_customer_order(int customer_order[]) {
    int ind = 0;
    while (1) {
        int inp = 0;
        scanf("%d", &inp);
        if(inp == -1) break;
        FILE *file = fopen(menu, "r");
        char buffer[1000];
        int cur = 0;
        while (fgets(buffer, sizeof(buffer), file) != NULL) cur++;
        if(inp > cur || inp == 0) return -1;
        customer_order[ind++] = inp;
    }
    return 0;
}

int main() {
    int tableNo, noofCustomers, billAmount;
    printf("Enter Table Number: ");
    scanf("%d", &tableNo);

    while (1) {
        printf("Enter Number Of Customers: ");
        scanf("%d", &noofCustomers);
        if(noofCustomers != -1) // Display menu if not closing
            read_menu();
        
        // Creating key for table and table manager
        key_t tableid = ftok("waiter.c", tableNo);
        if (tableid == -1) {
            printf("Error in executing ftok\n");
            exit(-1);
        }
        key_t table_manager = ftok("table.c", tableNo);
        if (table_manager == -1) {
            printf("Error in executing ftok\n");
            exit(-1);
        }

        // Getting shmid of already created shm for table manager
        int table_mn_shmid = shmget(table_manager, 10, 0666 | IPC_CREAT);
        if (table_mn_shmid == -1) {
            printf("Error in executing shmget\n");
            exit(-2);
        }

        // Attaching table manager to shared memory segment
        int *table_mn_shmptr = (int*)shmat(table_mn_shmid, NULL, 0);
        if(table_mn_shmptr==(void*)-1) {
            perror("Error in shmPtr in attaching the memory segment\n");
            exit(-1);
        }
        table_mn_shmptr[0] = 0;

        // Initializing all_orders array
        int all_orders[50];
        memset(all_orders, 0, sizeof(all_orders));

        // Creating N pipes for communication between table and customers
        for (int i = 0; i < noofCustomers; i++) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(-1);
            }
            
            // Forking to create customer processes
            pid_t pid = fork();
            if (pid == -1) {
                perror("Error in creating fork");
                exit(-1);
            }
            else if (pid == 0) { // Child process (customer)
                int valid_order = -1;
                int customer_order[10] = {};
                printf("Enter the serial number(s) of the item(s) to order from the menu. Enter -1 when done: ");
                valid_order = take_customer_order(customer_order);
                while(valid_order == -1) {
                    printf("Please Enter a Valid Order.\n");
                    printf("Enter the serial number(s) of the item(s) to order from the menu. Enter -1 when done: ");
                    valid_order = take_customer_order(customer_order);
                }
                close(pipefd[0]); // Close read end of the pipe
                write(pipefd[1], customer_order, 40);
                close(pipefd[1]); // Close write end of the pipe
                exit(-1);
            }
            else { // Parent process (table)
                close(pipefd[1]); // Close write end of the pipe
                int customer_order[10] = {};
                read(pipefd[0], customer_order, 40);
                close(pipefd[0]); // Close read end of the pipe
                for(int j = 0; j < 10; j++) {
                    all_orders[i * 10 + j] = customer_order[j];
                }
            }
        }
        
        // Getting shmid of already created shm for table
        int shmid = shmget(tableid, sizeof(SharedData), 0666 | IPC_CREAT);
        if (shmid == -1) {
            printf("Error in executing shmget\n");
            exit(-2);
        }

        // Attaching table to shared memory segment
        SharedData *shmptr = (SharedData *)shmat(shmid, NULL, 0); 
        
        while(shmptr->flag != 0); // Wait until table is ready for orders
        
        // Copying all_orders to shared memory
        for(int j = 0; j < 50; j++) {
            shmptr->all_orders[j] = all_orders[j];
        }
        
        if(noofCustomers == -1) {
            shmptr->data = -1; // Set data to -1 to indicate closing
            shmptr->flag = 1;  // Set flag to 1 to indicate orders are ready
        }
        else {
            shmptr->data = 1;  // Set data to 1 to indicate orders are ready
            shmptr->flag = 1;  // Set flag to 1 to indicate orders are ready
        
            while(shmptr->flag != 3); // Wait until bill is ready
            
            printf("The total bill amount is %d INR\n", shmptr->data);
            shmptr->flag = 0; // Reset flag to 0
        }

        table_mn_shmptr[0] = 1; // Set table manager flag to 1
        
        if (shmdt(shmptr) == -1) {
            perror("Error in shmdt\n");
            exit(-1);
        }
        if (shmdt(table_mn_shmptr) == -1) {
            perror("Error in shmdt\n");
            exit(-1);
        }
        
        if(noofCustomers == -1) break; // Break if closing
    }

    return 0;
}
