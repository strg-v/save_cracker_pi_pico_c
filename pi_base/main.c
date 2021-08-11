#include <wiringPi.h>
#include <wiringSerial.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#ifndef __cplusplus
typedef unsigned char bool;
static const bool false = 0;
static const bool true = 1;
#endif

int main(){
    const int fd = serialOpen("/dev/serial0", 115200);
    if(fd == -1)
        return fd;

    time_t rawTime;
    struct tm * timeinfo;
    time(&rawTime);
    timeinfo = localtime(&rawTime);
    

    char filename[16] = {0};
    sprintf(filename, "numbers/%d-%d-%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    printf("Starting Tresor at ");
    printf(filename);
    printf("\n");
    fflush(stdout);

    FILE *file = fopen(filename, "a+");
    bool running = true;
    char buffer[5] = {0};
    

int o = 0;

    while (running){

        char c = serialGetchar(fd);
        
        if (c > 0 && c < 128){
            printf("%c", c);
            fputc(c, file);
            fflush(stdout);
            fflush(file);

            buffer[0] = buffer[1];
            buffer[1] = buffer[2];
            buffer[2] = buffer[3];
            buffer[3] = c;

            if(strcmp(buffer, "stop") == 0){
                running = false;
            }
        }
    }
    printf("\nStopping program\n");
    serialClose(fd);
    fclose(file);

}
