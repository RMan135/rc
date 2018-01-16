#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

#define PORT 2643

char * startMessage = " === myRPC Client v.0.46 ===\n";

enum Command{
    CMD_QUIT = 0,
    CMD_UNDEFINED = -1
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
    if(StartsWith(buffer, "quit") && iswspace(buffer[4]))
        return CMD_QUIT;

    return CMD_UNDEFINED;
}

int CleanEndingWhitespace(char * buffer)
{
    int i = strlen(buffer);
    while(iswspace(buffer[i]))
    {
        buffer[i] = 0;
        i--;
    }
}

int SendMessage(int sd, char * msg)
{
    int len = strlen(msg);
    write(sd, &len, sizeof(int));
    write(sd, msg, len);
}

int main (int argc, char *argv[])
{
    printf("%s\n", startMessage);
    int sd;
    sockaddr_in server;

    int nr=0;
    char buf[10];

    if (argc != 2)
    {
        printf ("%s <adresa_server>\n", argv[0]);
        return -1;
    }

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf ("Eroare la creare socket\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (PORT);

    if (connect (sd, (sockaddr *) &server, sizeof(sockaddr)) == -1)
    {
        printf ("Eroare la connect()\n");
        return errno;
    }

    int quit = 0;

    int commandBufferSize = 256;
    char * commandBuffer = new char[commandBufferSize];
    int commandSize;
    int answerSize;
    char * answerBuffer;

    Command command;

    read(sd, &answerSize, sizeof(int));

    if(answerSize == 0)
    {
        printf("Server-ul nu a oferit un mesaj initial\n");
    }
    else
    {
        answerBuffer = new char[answerSize];
        read(sd, answerBuffer, answerSize);
        printf("\n%s\n", answerBuffer);
        delete [] answerBuffer;
    }

    while(!quit)
    {
        printf(">");
        fgets(commandBuffer, commandBufferSize, stdin);
        CleanEndingWhitespace(commandBuffer);
        command = GetCommand(commandBuffer);

        if(command == CMD_QUIT)
        {
            quit = 1;
            printf("Exiting...\n");
        }

        SendMessage(sd, commandBuffer);

        if(!quit)
        {
            read(sd, &answerSize, sizeof(int));
            //printf("Received %d bytes\n", answerSize);
            if(answerSize == 0)
            {
                printf("Null response\n");
            }
            else
            {
                answerBuffer = new char[answerSize];
                bzero(answerBuffer, answerSize);
                read(sd, answerBuffer, answerSize);
                answerBuffer[answerSize] = 0;
                printf("%s\n", answerBuffer);
                delete [] answerBuffer;
                answerBuffer = NULL;
            }
        }
    }
    close (sd);
}
