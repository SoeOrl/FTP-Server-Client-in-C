#include <sys/socket.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#define MAX_LINE 100
#define SERVER_PORT 1588
#define BACKLOG 10
#define MAX_OPEN_CONNECTIONS 20
void ServerInteract(int);

int main(int argc, char *argv[])
{
    int currentOpenSockets = 0;
    int welcomeSocket, connectionSocket, pid;
    char clientSentence[MAX_LINE] = "0";
    struct sockaddr_in servaddr;

    welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (welcomeSocket == -1)
    {
        printf("Socket Create Failed");
        exit(1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT);

    if (bind(welcomeSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        printf("Binding Failed");
        exit(1);
    }

    listen(welcomeSocket, BACKLOG);

    while (connectionSocket = accept(welcomeSocket, (struct sockaddr *)NULL, NULL))
    {
        printf("Accepted Connection\n");
        pid = fork();

        if (pid > 0)
        {
            currentOpenSockets += 1;
            printf("Resuming with Server\n");
        }
        else
        {
            printf("Interacting with Server\n");
            ServerInteract(connectionSocket);
            currentOpenSockets -= 1;
            exit(0);
        }
    }
}

void ServerInteract(int connectionSocket)
{
    char command[50];
    FILE *fp;
    char test[50];
    int bytesRead = 1;
    char clientSentence[MAX_LINE];
    bytesRead = read(connectionSocket, clientSentence, MAX_LINE);
    printf(clientSentence);
    while (strcmp(clientSentence, "bye\n") != 0 && bytesRead > 0)
    {
        // If we want server ls, run the ls command on the server
        if (strcmp(clientSentence, "catalog\n") == 0)
        {

            fp = popen("ls", "r");
            if (fp == NULL)
            {
                printf("Failed to run Command");
                write(connectionSocket, "Failed to run Command: ls", MAX_LINE);
            }
            else
            {
                // read each line and send to client
                while (fgets(clientSentence, MAX_LINE, fp) != NULL)
                {
                    write(connectionSocket, clientSentence, MAX_LINE);
                    printf("%s", clientSentence);
                }

                pclose(fp);
            }
        }
        else if (strstr(clientSentence, "download") != NULL)
        {

            // Prnt what Server received, set delim, and file stats
            printf(clientSentence);
            char delim[] = " ";
            struct stat file_stat;

            // Split the string
            char *ptr = strtok(clientSentence, delim);
            printf("Action: %s\n", ptr);
            char *fileName = strtok(NULL, delim);
            printf("Source: %s\n", fileName);
            char *destFileName = strtok(NULL, delim);
            printf("Destination: %s\n", destFileName);

            // If the file names are good and accessable send to client
            if (fileName != NULL && destFileName != NULL && access(fileName, F_OK) != -1)
            {
                FILE *file = fopen(fileName, "r");
                int fd = fileno(file);
                if (fstat(fd, &file_stat) < 0)
                {
                    printf("Error gettings stats\n");
                }
                char fileLength[256];

                // Send file length followed by the actual file
                sprintf(fileLength, "%d", file_stat.st_size);
                write(connectionSocket, fileLength, MAX_LINE);
                sendfile(connectionSocket, fd, NULL, file_stat.st_size);
            }
            else
            {
                if (fileName == NULL || destFileName == NULL)
                {
                    write(connectionSocket, "USAGE: download fileNameOnServer fileNameToBeOnClient\n", MAX_LINE);
                }
                else
                {
                    write(connectionSocket, "The file you requested does not exist\n", MAX_LINE);
                }
            }
        }
        else if (strstr(clientSentence, "upload") != NULL)
        {
            // Set up delimeter, file size, and delimeter
            char delim[] = " ";
            struct stat file_stat;
            char *ptr = strtok(clientSentence, delim);
            int messageLength = 0;

            // Parse the message
            printf("Action: %s\n", ptr);
            char *fileName = strtok(NULL, delim);
            printf("Source: %s\n", fileName);
            char *destFileName = strtok(NULL, delim);
            printf("Destination: %s\n", destFileName);

            // If we can access the file, it already exists
            if (access(destFileName, F_OK) == 0)
            {
                write(connectionSocket, "File with the specified name already exists, overwritting the file\n", MAX_LINE);
            }

            destFileName[strlen(destFileName) - 1] = '\0';
            // Open the file for writing and get the file length from client
            FILE *received_file = fopen(destFileName, "w");
            read(connectionSocket, clientSentence, MAX_LINE);
            int remainingData = atoi(clientSentence);
            printf("The length of the file is %d bytes\n", remainingData);

            //Receive the file
            while ((remainingData > 0) && ((messageLength = read(connectionSocket, clientSentence, MAX_LINE)) > 0))
            {
                fwrite(clientSentence, sizeof(char), messageLength, received_file);
                remainingData -= messageLength;
                if (remainingData < 100)
                {
                    messageLength = read(connectionSocket, clientSentence, remainingData);
                    fwrite(clientSentence, sizeof(char), messageLength, received_file);
                    remainingData -= messageLength;
                }
            }
            fclose(received_file);
        }

        else if (strcmp(clientSentence, "spwd\n") == 0)
        {
            // Run the pwd command and send its output to the client
            fp = popen("pwd", "r");
            if (fp == NULL)
            {
                printf("Failed to run Command");
            }
            while (fgets(clientSentence, MAX_LINE, fp) != NULL)
            {
                write(connectionSocket, clientSentence, MAX_LINE);
                printf("%s", clientSentence);
            }
            pclose(fp);
        }
        else
        {
            write(connectionSocket, "ERROR Unknown Command: Make sure there is no extra characters\n", MAX_LINE);
            write(connectionSocket, clientSentence, MAX_LINE);
        }
        write(connectionSocket, "\0\0", MAX_LINE);
        fflush(stdout);
        bytesRead = read(connectionSocket, clientSentence, MAX_LINE);
        printf(clientSentence);
    }

    printf("Closing Connection\n");
    fflush(stdout);
    // Send bye to the client and close the connection
    write(connectionSocket, "bye\n", MAX_LINE);
    write(connectionSocket, "\0\0", MAX_LINE);
    close(connectionSocket);
}