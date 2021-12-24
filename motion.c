#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PIN 24  
#define POUT 23 

#define VALUE_MAX 30
#define BUFFER_MAX 3
#define DIRECTION_MAX 45

double distance_first = 0;

void error_handling(char *message) {
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

static int GPIOExport(int pin) {

    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open export for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIODirection(int pin, int dir) {
    static const char s_directions_str[] = "in\0out";

    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);

    if (-1 == fd) {
        fprintf(stderr, "Failed to open direction for writing!\n");
        return(-1);
    }

    if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2: 3)) {
        fprintf(stderr, "Failed to set direction!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);

}

static int GPIORead(int pin) {
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return(-1);
    }

    if (-1 == read(fd, value_str, 3)) {
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(atoi(value_str));

}

static int GPIOWrite(int pin, int value) {
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return(-1);
    }

    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
        fprintf(stderr, "Failed to write value!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
    
}

static int GPIOUnexport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

void* ultra_first_thd() {
    clock_t start_t, end_t;
   double time;

    if(-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN)){
        printf("gpio export err\n");
        exit(0);
    }

    usleep(100000);

    if(-1 == GPIODirection(POUT, OUT) || -1 == GPIODirection(PIN, IN)){
        printf("gpio direction err\n");
        exit(0);
    }

    GPIOWrite(POUT, 0);

    while(1) {

        if(-1 == GPIOWrite(POUT, 1)) {
            printf("gpio write/trigger err\n");
            exit(0);
        }

        usleep(10);
        GPIOWrite(POUT, 0);

        while(GPIORead(PIN) == 0){
            start_t = clock();
        }

        while(GPIORead(PIN) == 1){
            end_t = clock();
        }

        time = (double)(end_t-start_t)/CLOCKS_PER_SEC;
        distance_first = time/2*34000;
        if(distance_first > 900) distance_first = 900;

    }

    if(-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(PIN)) exit(0);

}


void* socket_thd() {

    int sock;
    struct sockaddr_in serv_addr;
    char msg[1024];

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handling("socket() error");

    char* host = "192.168.0.38";
    int port = 8080;
        
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(host);
    serv_addr.sin_port = htons(port); 
        
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
        error_handling("connect() error");

    while(1) {

        snprintf(msg, 100, "%d", (int)distance_first);
        write(sock, msg, sizeof(msg));
            
        printf("sending message to Master : %s\n",msg);
        usleep(100000);

    }

}

int main(int argc, char *argv[]) {

    pthread_t p_thread[2];
   int thr_id;
   int status;
   char p1[] = "ultrafirst_thd";
   char p2[] = "socket_thd";

   //make thread
    thr_id = pthread_create(&p_thread[0], NULL, ultra_first_thd , (void *)p1);
      if(thr_id <0)
    {
           perror("thread create error : ");
           exit(0);
   }

    thr_id = pthread_create(&p_thread[1], NULL, socket_thd(argv), (void *)p2);
     if(thr_id <0)
     {
           perror("thread create error : ");
           exit(0);
   }

    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);
   
    if(-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(PIN)) return(-1);
   return 0;
 
}