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


#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1

#define PIN 20   // Button
#define POUT 21 // Button
#define PIN2 24 // ultra
#define POUT2 23 // ultra

#define VALUE_MAX   256
#define BUFFER_MAX 3
#define DIRECTION_MAX 45    

int motion;
int degree;
int distance_first;
int distance_second;
int prev_degree;

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

static int PWMExport(int pwmnum) {

    char buffer[BUFFER_MAX];
    int bytes_written;
    int fd;

    fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in export!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
    write(fd, buffer, bytes_written);

    close(fd);   
    sleep(1);
    return(0);

}

static int PWMUnexport(int pwmnum) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in unexport!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
    write(fd, buffer, bytes_written);

    close(fd); 
    sleep(1);
    return(0);

}

static int PWMEnable(int pwmnum) {
    static const char s_unenable_str[] = "0";
    static const char s_enable_str[] = "1";

    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd, s_unenable_str, strlen(s_unenable_str));
    close(fd);

    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd, s_enable_str, strlen(s_enable_str));
    close(fd);
    return 0;

}

static int PWMUnable(int pwmnum) {
    static const char s_unable_str[] = "0";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd, s_unable_str, strlen(s_unable_str));
    close(fd);

    return 0;

}

static int PWMWritePeriod(int pwmnum, int value) {
    char s_values_str[VALUE_MAX];
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in period!\n");
        return -1;
    }

    byte = snprintf(s_values_str, 10, "%d", value);

    if (-1 == write(fd, s_values_str, byte)) {
        fprintf(stderr, "Failed to write value in period!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

}

static int PWMWriteDutyCycle(int pwmnum, int value) {
    char path[VALUE_MAX];
    char s_values_str[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pwmnum);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in duty_cycle!\n");
        return -1;
    }

    byte = snprintf(s_values_str, 10, "%d", value);

    if (-1 == write(fd, s_values_str, byte)) {
        fprintf(stderr, "Failed to write value in duty_cycle!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

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

void play_gtts(int temp, int code) {

    int ret;
    char buffer[200];

    if(code){

        snprintf(buffer, 200, "gtts en \"The current temperature is %d degrees. \"", temp);

        ret = system(buffer);

        if (ret == -1) {
          printf("Failed to gtts\n");
        }

    } else {

        if(temp == 1) {

            ret = system("sudo aplay /home/pi/Downloads/beep-18.wav");

            if(ret == -1){
                printf("Failed to beep\n");
            }

            return;
        }

        if(temp == -1) {

            ret = system("sudo aplay /home/pi/Downloads/beep-17.wav");

            if(ret == -1){
                printf("Failed to beep\n");
            }

            return;

        }

        while(1){

            ret = system("sudo aplay /home/pi/Downloads/beep-05.wav");

            if(ret == -1){
                printf("Failed to beep\n");
            }

            if(motion == 1) {
                return;
            }

        }
    }

    return;
    
}

void* temp_socket_thd() {

    int serv_sock, clnt_sock = -1;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    char msg[1024];

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(8888);

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    if(listen(serv_sock,5) == -1)
        error_handling("listen() error");

    if(clnt_sock < 0) {    

        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

        if(clnt_sock == -1) {
            error_handling("accept() error");   
        }

    }

    while(1) {

        int str_len = read(clnt_sock, msg, sizeof(msg));

        if(str_len == -1) error_handling("read() error");

        if(str_len > 0) {
            degree = atoi(msg);
        }

        printf("degree: %d\n", degree);

    }

    close(clnt_sock);
    close(serv_sock);

}

void *temp_thd(){

    while(1) {

        if(motion == 0) {
            if(degree > 60) {
                printf("Dangerous! Watch out FIRE!!!\n");
                play_gtts(motion, 0);
            }
        } 

        else {
            if(degree - prev_degree > 10) {

                printf("Changed more than 10 degrees!\n");
                play_gtts(degree, 1); 
                prev_degree = degree;

            }
        }
        
        usleep(100000);
    
    }

}

void* motion_socket_thd() {

    int serv_sock, clnt_sock = -1;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    char msg[1024];

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(8080);

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    if(listen(serv_sock,5) == -1)
        error_handling("listen() error");

    if(clnt_sock < 0) {    

        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

        if(clnt_sock == -1) {
            error_handling("accept() error");   
        }

    }

    while(1) {

        int str_len = read(clnt_sock, msg, sizeof(msg));
        
        if(str_len == -1) error_handling("read() error");

        if(str_len > 0) {

            distance_first = atoi(msg);
            printf("distance_first: %d\n", distance_first);

            if(distance_first <= 70) {
                motion = 1;
            } else {
                motion = 0;
            }

        }
    }

    close(clnt_sock);
    close(serv_sock);

}



void *distance_thd(){

    clock_t start_t, end_t;
    double time;
    double distance;

    if(-1 == GPIOExport(POUT2) || -1 == GPIOExport(PIN2)){
        printf("gpio export err\n");
        exit(0);
    }

    if(-1 == GPIODirection(POUT2, OUT) || -1 == GPIODirection(PIN2, IN)){
        printf("gpio direction err\n");
        exit(0);
    }

    while(1) {

        if(-1 == GPIOWrite(POUT2, 1)) {
            printf("gpio write/trigger err\n");
            exit(0);
        }

        usleep(10);
        GPIOWrite(POUT2, 0);

        while(GPIORead(PIN2) == 0){
            start_t = clock();
        }

        while(GPIORead(PIN2)==1){
            end_t = clock();
        }

        time = (double)(end_t-start_t)/CLOCKS_PER_SEC;
        distance = time/2*34000;
        if(distance > 900) distance = 900;

        distance_second = (int)distance;

        printf("distance_second: %d\n", distance_second);

        if(distance_second < 10) {
            printf("Dangerous! Move Back!\n");
            play_gtts(1, 0);
        } else if(distance_second < 15){
            printf("Back!\n");
            play_gtts(-1, 0);
        }
        
        usleep(500000);

    }

    if(-1 == GPIOUnexport(POUT2) || -1 == GPIOUnexport(PIN2)) exit(0);

}

void *button_thd() {
    
    int state = 1;
    int prev_state = 1;
    
    //Enable GPIO pins
    if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
        exit(0);

    //Set GPIO directions
    if (-1 == GPIODirection(POUT, OUT))
        exit(0);

    //Set GPIO directions
    if (-1 == GPIODirection(PIN, IN))
        exit(0);
        
    
    while(1) { 

        if (-1 == GPIOWrite(POUT, 1)) {
            printf("Failed to write BUTTON_IN\n");
            exit(0);
        }

        state = GPIORead(PIN);
        // printf("state : %d\n", state);

        if (state == -1) {
            printf("Failed to read BUTTON_IN\n");
            exit(0);
        }
        
        if(prev_state == 1 && state == 0) {
            printf("Button clicked\n");
            play_gtts(degree, 1);
        }

        prev_state = state;
        usleep(500 * 100);

    }

    //Disable GPIO pins
    if (-1 == GPIOUnexport(POUT))
        exit(0);
    
    
}


int main(int argc, char *argv[]) {

    pthread_t p_thread[4];
    int thr_id;
    int status;
    char p1[] = "thread_1"; 
    char p2[] = "thread_2";
    char p3[] = "thread_3";
    char p4[] = "thread_4";
    char p5[] = "thread_5";

    thr_id = pthread_create(&p_thread[0], NULL, distance_thd, (void *)p1); 
    if (thr_id < 0) {
        perror("thread create error: "); 
        exit(0);
    }

    thr_id = pthread_create(&p_thread[1], NULL, temp_thd, (void *)p2); 
    if (thr_id < 0) {
        perror("thread create error: "); 
        exit(0);
    }
    
    
    thr_id = pthread_create(&p_thread[2], NULL, button_thd, (void *)p3); 
    if (thr_id < 0) {
        perror("thread create error: "); 
        exit(0);
    }

    thr_id = pthread_create(&p_thread[3], NULL, temp_socket_thd, (void *)p4); 
    if (thr_id < 0) {
        perror("thread create error: "); 
        exit(0);
    }

    thr_id = pthread_create(&p_thread[4], NULL, motion_socket_thd, (void *)p5); 
    if (thr_id < 0) {
        perror("thread create error: "); 
        exit(0);
    }

    pthread_join(p_thread[0], (void **)&status); 
    pthread_join(p_thread[1], (void **)&status);
    pthread_join(p_thread[2], (void **)&status);
    pthread_join(p_thread[3], (void **)&status);
    pthread_join(p_thread[4], (void **)&status);

    return(0);
}