#include  <stdio.h>
#include  <unistd.h>
#include  <sys/wait.h>



int  main()
{
    printf("Parent: Process started\n");
    printf("Parent: Forking a child.\n");
    
    if (fork() != 0) {
        // Parent
        int status;
        printf("Parent: Wait for child to complete.\n")    ;
        // waitpid(pid, &status, options)
        // pid == 0 means wait for child whose 
        // group-id = its caller group-id
	   // pid == -1 means wiat for child process
        // whose group-id == |pid|
        waitpid(-1, &status, 0);   // equivalent to wait(&status)
        printf("Parent: Terminating.\n");
    }
    else {
        // Child
        printf("Child: Process started.\n");
        printf("Child: Start 10 second idle:");
        
        int i;
        for (i = 10; i >= 0; i--) {
            printf("%3d", i); fflush(stdout);
            sleep(1);
        }
        printf(" done!\n");
        printf("Child: Terminating.\n");
    }
}
