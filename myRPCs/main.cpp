#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

#define PORT 2643

char * startMessage = " === myRPC Server v.0.46 ===\n";

char * welcomeMessage = "Te-ai conectat la Server-ul standard de myRPC\n"
"Foloseste comanda 'help' pentru a primi o lista de comenzi disponibile.\n";

char * errorMessage = "Something went wrong and the server needs to abort the connection.\n";

char * helpMessage = "Available commands:\n"
        "   help            - Show available commands\n"
        "   time            - Show server's local time\n"
        "   myip            - Show your IP address\n"
        "   ping <address>  - Ping an address. Use 'myself' instead of the address to ping your machine\n"
        "   quit            - Quit session\n";

struct thData{
    bool used;
    int idThread;
    int cl;
    sockaddr_in clData;
};

int commandCount = 6;

enum Command{
    CMD_UNDEFINED = 0,
    CMD_HELP = 1,
    CMD_QUIT = 2,
    CMD_TIME = 3,
    CMD_MYIP = 4,
    CMD_PING = 5,
};

char * commandStr[]{
        "undefined",
        "help",
        "quit",
        "time",
        "myip",
        "ping"
};

bool StartsWith(char * toStart, char * with)
{
    int s = strlen(toStart);
    int w = strlen(with);

    if (s < w) return false;

    for(int i = 0; i < w; i++)
    {
        if((toStart[i]& ~(32)) != (with[i]& ~(32)))
            return false;
    }
    return true;
}

char * GetArgument(char * buffer)
{
    int i = 0;
    while(buffer[i] > 32) i++;

    if(buffer[i] == ' ')
    {
        return buffer + i + 1;
    }
    return NULL;
}

Command GetCommand(char * buffer)
{
    Command command = CMD_UNDEFINED;

    for(int i = 1; i < commandCount; i++)
        if(StartsWith(buffer, commandStr[i]))// && iswspace(strlen(commandStr[i])))
            return (Command)i;

    return command;
}

int SendMessage(int sd, char * msg)
{
    int len = strlen(msg);
    write(sd, &len, sizeof(int));
    write(sd, msg, len);
}

int PingCommand(char * arg, char * answer, char * clAddr)
{
    char * cmd = new char[128];

    if (arg == NULL)
    {
        sprintf(answer, "You need to specify an address\n");
        return 0;
    }

    if(StartsWith(arg, "myself") && iswspace(arg[6]))                    {
        sprintf(cmd, "ping -c 1 %s", clAddr);
    }
    else                    {
        sprintf(cmd, "ping -c 1 %s", arg);
    }
    FILE * p = popen(cmd, "r");

    char * line = new char[128];

    while(fgets(line, 512, p))
    {
        strcat(answer, line);
        //strcat(answer, "\n");
    }
    delete [] cmd;
    delete [] line;
    cmd = NULL;
    line = NULL;
    return 0;
}

void ThreadWork(void *arg)
{
    int nr, i = 0;
    thData tdL;
    tdL = *(thData*)arg;
    char clAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(tdL.clData.sin_addr), clAddr, INET_ADDRSTRLEN);

    SendMessage(tdL.cl, welcomeMessage);

    int quit = 0;

    int commandLength;
    char * commandBuffer;

    while(!quit)
    {
        printf("[Thread #%d][%s] Waiting for user input\n", tdL.idThread, clAddr);
        read(tdL.cl, &commandLength, sizeof(int));
        if(commandLength == 0)
        {
            printf("[Thread #%d][%s] Received null message from client. Closing connection.\n", tdL.idThread, clAddr);
            quit = 1;
        }
        else
        {
            printf("[Thread #%d][%s] Received %d bytes\n", tdL.idThread, clAddr, commandLength);

            commandBuffer = new char[commandLength];
            bzero(commandBuffer, commandLength);
            read(tdL.cl, commandBuffer, commandLength);

            Command command = GetCommand(commandBuffer);

            printf("[Thread #%d][%s] User issued '%s' command\n", tdL.idThread, clAddr, commandStr[command]);

            switch (command) {
                case CMD_QUIT: {
                    printf("[Thread #%d][%s] User disconnecting\n", tdL.idThread, clAddr);
                    quit = 1;
                    close(tdL.cl);
                    break;
                }
                case CMD_HELP: {
                    SendMessage(tdL.cl, helpMessage);
                    break;
                }
                case CMD_TIME: {
                    time_t t = time(NULL);
                    struct tm *currentTime = localtime(&t);
                    SendMessage(tdL.cl, asctime(currentTime));
                    break;
                }
                case CMD_MYIP: {
                    char * answer = new char[128];
                    sprintf(answer, "Your IP address is '%s'\n", clAddr);
                    SendMessage(tdL.cl, answer);
                    delete [] answer;
                    answer = NULL;
                    break;
                }
                case CMD_PING:
                {
                    char * answer = new char[512];
                    PingCommand(GetArgument(commandBuffer), answer, clAddr);
                    SendMessage(tdL.cl, answer);
                    delete [] answer;
                    answer = NULL;
                    break;
                }
                default: {
                    char *c = strtok(commandBuffer, " \t\n\v\f\r");
                    if (c == NULL) {
                        printf("[Thread #%d][%s] Lost connection with user.\n", tdL.idThread, clAddr);
                        quit = 1;
                    }
                    else
                    {
                        printf("[Thread #%d][%s] Unrecognized command: %s\n", tdL.idThread, clAddr, c);
                        SendMessage(tdL.cl,
                                    "Unrecognized command. Use the 'help' command to view the available commands.\n");
                    }
                    break;
                }
            }
        }

        //printf("[Thread #%d][%s] Deleting alocated space\n", tdL.idThread, clAddr);
        delete [] commandBuffer;
        commandBuffer = NULL;
        //printf("[Thread #%d][%s] Finished deleting alocated space\n", tdL.idThread, clAddr);
    }
    return;
}

static void * StartThread(void * arg)
{
    thData tdL;
    tdL = *(thData*)arg;
    printf("[Thread #%d] Started\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    ThreadWork((thData*) arg);
    printf("[Thread #%d] Finished\n", tdL.idThread);
    pthread_exit(NULL);
    return NULL;
}

int main() {

    printf("%s\n", startMessage);

    sockaddr_in server;
    sockaddr_in from;

    int nr;
    int sd;

    int pid;
    pthread_t th[100];

    int i = 0;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Error socket() [%d]\n", errno);
        return errno;
    }

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if( bind(sd, (sockaddr*) &server, sizeof(sockaddr)) == -1)
    {
        printf("Error bind() [%d]\n", errno);
        return errno;
    }

    if(listen(sd, 2) == -1)
    {
        printf("Error listen() [%d]\n", errno);
        return errno;
    }

    printf("Listening port %d\n", PORT);
    fflush(stdout);

    srand(clock());

    while(1)
    {
        int client;
        thData * td;
        socklen_t  length = sizeof(from);

        if((client = accept(sd, (sockaddr*)&from, &length)) < 0)
        {
            printf("Error accept() [%d]\n", errno);
            continue;
        }

        char clAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(from.sin_addr), clAddr, INET_ADDRSTRLEN);

        printf("User connected from %s\n", clAddr);


        td = new thData;
        td->cl = client;
        td->idThread = i;
        td->clData = from;

        pthread_create(&th[i], NULL, &StartThread, td);

        i++;
    }

    return 0;
}