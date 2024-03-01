#include "systemcalls.h"
#include <stdlib.h>

#include <unistd.h>    // for fork and exec
#include <sys/wait.h> // for wait
#include <fcntl.h>   // for open

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int status = system(cmd);
    if(status != -1) return true;
    else return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    fflush(stdout);
    pid_t pid = fork();
    int status;

    if (pid == 0) // Child process
    {
        printf("Child process\n");
        int out = execv(command[0], command);
        if(out == -1){
            perror("execv failed");
            exit(EXIT_FAILURE);
        }
        exit(1);
    } 
    else if (pid > 0) // Parent process
    { 
        printf ("Parent process pid = %d\n", pid);
        pid = wait(&status);
        printf ("wait_pid = %d\n", pid);

        if (WIFEXITED (status)) printf ("Normal termination with exit status=%d\n", WEXITSTATUS (status));
        if (WIFSIGNALED (status)) printf ("Killed by signal=%d%s\n", WTERMSIG (status), WCOREDUMP (status) ? " (dumped core)" : "");
        if (WIFSTOPPED (status)) printf ("Stopped by signal=%d\n", WSTOPSIG (status));
        if (WIFCONTINUED (status)) printf ("Continued\n");

        if (pid == -1 || WEXITSTATUS(status) != 0) {
            perror ("wait");
            return false;
        }
    } 
    else // Error in fork()  
    {
        perror("fork failed");
        return false;
    }

    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    int kidpid;
    int status;
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }

    switch (kidpid = fork()) {
      case -1: perror("fork"); abort();
      case 0:
        if (dup2(fd, 1) < 0) { perror("dup2"); abort(); }
        close(fd);
        execvp(command[0], command); perror("execvp"); abort();
      default:
        /* do whatever the parent wants to do. */
        printf("Parent process\n");
        wait(NULL);
        kidpid = wait(&status);

        if (kidpid == -1) perror ("wait");

        printf ("pid = %d\n", kidpid);

        if (WIFEXITED (status)) printf ("Normal termination with exit status=%d\n", WEXITSTATUS (status));
        if (WIFSIGNALED (status)) printf ("Killed by signal=%d%s\n", WTERMSIG (status), WCOREDUMP (status) ? " (dumped core)" : "");
        if (WIFSTOPPED (status)) printf ("Stopped by signal=%d\n", WSTOPSIG (status));
        if (WIFCONTINUED (status)) printf ("Continued\n");
        close(fd);
}
    va_end(args);
    return true;
}
