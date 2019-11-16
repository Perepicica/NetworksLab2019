#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>
#include<time.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

#include <termios.h>

#define _XOPEN_SOURCE 700


void sigHandlerOut(int sig);

void *reading(void *sockfd);

void sendMessage(char *message);

void closeClient();

bool toRead = true;
int sockfd;


int main(int argc, char *argv[]) {
    //объявление переменных
    int nameLength = (int) strlen(argv[3]) + 5;
    char name[nameLength];
    char timeToSend[8];
    char buffer[256];
    char length[5];
    int len;

    pthread_t tid;
    uint16_t portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    //проверка, что все аргументы введены
    if (argc < 4) {
        fprintf(stderr, "usage %s hostname port nickname\n", argv[0]);
        exit(0);
    }
    //номер порта
    portno = (uint16_t) atoi(argv[2]);

    //создание сокета и проверка
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    //обработчик закрытия клиента
    signal(SIGINT, sigHandlerOut);

    //нахожу мой сервер и проверяю
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    //соединение с сервером
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    //каждому клиенты отдельный поток чтения
    if (pthread_create(&tid, NULL, reading, (void *) (intptr_t) sockfd) < 0) {
        perror("ERROR on create phread");
        exit(2);
    }

    //подготовила имя клиента для отправки
    snprintf(name, sizeof name, "[%s]", argv[3]);
    while (1) {
        if (toRead) { // true - пока не нажата esc, то есть нельзя писать сообщение
            int character;
            struct termios orig_term_attr;
            struct termios new_term_attr;

            // что-то там для терминала, чтобы считать нажатие клавиши
            tcgetattr(fileno(stdin), &orig_term_attr);
            memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
            new_term_attr.c_lflag &= ~(ECHO | ICANON);
            new_term_attr.c_cc[VTIME] = 0;
            new_term_attr.c_cc[VMIN] = 0;
            tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

            //считываю нажатие кливаши
            character = fgetc(stdin);
            tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

            //если нажата esc
            if (character == 0x1B) {
                //не вывожу сообщения в терминал и могу печатать своё сообщение
                toRead = false;
                printf("\nВведите сообщение:\n");
            }
        } else {
            //обнуляю буфер и считываю новое сообщение
            bzero(buffer, 256);
            fgets(buffer, 255, stdin);

            //чсто-то там для узнавания текущего времени
            time_t my_time;
            struct tm *timeinfo;
            time(&my_time);
            timeinfo = localtime(&my_time);

            //длина моего сообщения, включая время и имя отправителя
            len = strlen(name) + strlen(timeToSend) + strlen(buffer);

            //подготовила согласно формату время и длину
            snprintf(timeToSend, sizeof timeToSend, "<%d:%d>", timeinfo->tm_hour, timeinfo->tm_min);
            snprintf(length, sizeof length, "%d", len);

            //отправляю данные согласно протоколу: длинуб имя, время, сообщение
            sendMessage(length);
            sendMessage(name);
            sendMessage(timeToSend);

            //возвращаюсь в режим принятия сообщений, сама снова не могу писать пока не нажму esc
            toRead = true;
        }
    }
}

//поток чтения сообщений с сервера
void *reading(void *sockfd) {
    char buffer[256];
    while (1) {
        while (toRead) {
            bzero(buffer, 256);
            if (read((uintptr_t) sockfd, buffer, 256) <= 0) {
                closeClient();
            }
            printf("%s\n", buffer);
        }
    }
}

//отправка сообщения
void sendMessage(char *message) {
    if (write(sockfd, message, strlen(message)) <= 0) {
        closeClient();
    }
}

//обработчик закрытия клиента
void sigHandlerOut(int sig) {
    if (sig != SIGINT) return;
    else {
        closeClient();
    }
}

//закрытие клиента
void closeClient() {
    printf("\nВыход из чата\n");
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    exit(1);
}



