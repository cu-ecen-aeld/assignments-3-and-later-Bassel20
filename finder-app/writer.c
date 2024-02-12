#include <stdio.h>
#include <stdlib.h>
// for logging
#include <syslog.h>
// for reporting error conditions
#include <errno.h>

int main(int argc, char *argv[]) {

    // Connect to syslog service
    openlog(NULL, 0, LOG_USER);

    // Check for wrong number of arguments 
    if(argc != 3){
        syslog(LOG_ERR, "Error: please enter 3 arguments");
        syslog(LOG_INFO, "USAGE: \n %s <file_path> <text_to_write>", argv[0]);
        closelog();
        return 1;
    }

    // Extract arguments
    const char * file_path = argv[1];
    const char * text_to_write = argv[2];

    // Open file
    FILE *file = fopen(file_path, "w");

    // Check if file opened successfully
    if (file == NULL){
        syslog(LOG_ERR, "Error: unable to open file \n %s", strerror(errno));
        closelog();
        return 1;
    }

    // Write string to file with error handling
    if(fprintf(file, "%s", text_to_write) < 0){
        syslog(LOG_ERR, " Error writing to file \n %s", strerror(errno) );
        fclose(file);
        closelog();
        return 1;
    }
    else {  // Confirm operation success
        syslog(LOG_DEBUG, "Writing %s to %s", text_to_write, file_path);
    }
    // Close file
    fclose(file);
    // Close syslog service
    closelog();

    return 0;
}