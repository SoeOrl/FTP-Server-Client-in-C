#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#define MAX_LINE 100
#define SERVER_PORT 1588
#define FILENAME "a.txt"
#define SERVER_IP "127.0.0.1"
#define h_addr h_addr_list[0]
int main(int argc, char *argv[])
{
    char input[MAX_LINE];
    char reply_msg[MAX_LINE];
    reply_msg[0] = '1';
    int connectionSocket;
    char sentence[MAX_LINE];
    char modifiedSentence[MAX_LINE];
    struct hostent *hp;
    struct sockaddr_in servaddr;

    connectionSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (argc != 2)
    {
        printf("Error, please pass in the name of the host, if it is local type './Client localhost'");
        return 0;
    }

    hp = gethostbyname(argv[1]);
    //hp = gethostbyname("localhost");
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    bcopy(hp->h_addr, (char *)&servaddr.sin_addr, hp->h_length);
    //servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(connectionSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("Connection failed\n");
        return 1;
    }

    while (strcmp(input, "bye\n") != 0) //Check if user wants to exit
    {
        fflush(stdout);
        input[0] = '0';
        printf("What do you want to do?\n");
        fgets(input, MAX_LINE, stdin);

        if (strcmp(input, "ls\n") == 0 || strstr(input, "ls ") != 0)
        {
            reply_msg[0] = '\0';
            reply_msg[1] = '\0';
            int returnStatus = system(input);

            if (returnStatus < 0)
            {
                perror("The following error occurred");
            }
        }
        else if (strcmp(input, "pwd\n") == 0)
        {
            reply_msg[0] = '\0';
            reply_msg[1] = '\0';
            int returnStatus = system("pwd");

            if (returnStatus < 0)
            {
                perror("The following error occurred");
            }
        }
        else if (strstr(input, "download") != NULL)
        {

            // Copy of user input
            char inputCopy[MAX_LINE];
            strncpy(inputCopy, input, 99);
            // Delimeter and file stats
            char delim[] = " ";
            struct stat file_stat;
            // Brake string down into components
            char *ptr = strtok(input, delim);
            int messageLength = 0;
            printf("Action: %s\n", ptr);
            char *fileName = strtok(NULL, delim);
            printf("Source: %s\n", fileName);
            char *destFileName = strtok(NULL, delim);
            printf("Destination: %s\n", destFileName);

            // If the file names are not null send to server and listen
            if (fileName != NULL && destFileName != NULL)
            {
                // Send command to Server
                if (write(connectionSocket, inputCopy, strlen(inputCopy) + 1) < 0)
                {
                    printf("Failed to contact server with %s\n", inputCopy);
                    return 1;
                }

                // If we can access the file, it already exists, warn the user
                if (access(destFileName, F_OK) == 0)
                {
                    printf("File: %s already exists, overwritting the file\n", destFileName);
                }

                destFileName[strlen(destFileName)-1] = '\0';
                // Open or create the file for writing
                FILE *received_file = fopen(destFileName, "w");

                // Receive file length from Server and print it
                read(connectionSocket, reply_msg, MAX_LINE);
                int remainingData = atoi(reply_msg);
                printf("The length of the file is %d bytes\n", remainingData);

                // While we have not received all data, receive data
                while ((remainingData > 0) && ((messageLength = read(connectionSocket, reply_msg, MAX_LINE)) > 0))
                {
                    fwrite(reply_msg, sizeof(char), messageLength, received_file);
                    remainingData -= messageLength;
                    if (remainingData < 100)
                    {
                        messageLength = read(connectionSocket, reply_msg, remainingData);
                        fwrite(reply_msg, sizeof(char), messageLength, received_file);
                        remainingData -= messageLength;
                    }
                }
                fclose(received_file);
            }
            else
            {
                printf("Error: USAGE: download ServerFile LocalFileName\n");
                reply_msg[0] = '\0';
                reply_msg[1] = '\0';
            }
        }
        else if (strstr(input, "upload") != NULL)

        {
            // Make copy of input, set delimeter and get file stats
            char delim[] = " ";
            struct stat file_stat;
            char inputCopy[MAX_LINE];
            strncpy(inputCopy, input, 99);
            printf(inputCopy);

            // Take string apart into tokens
            char *ptr = strtok(input, delim);
            printf("Action: %s\n", ptr);
            char *fileName = strtok(NULL, delim);
            if (fileName != NULL)
            {
                printf("Source: %s\n", fileName);
            }
            char *destFileName = strtok(NULL, delim);
            if (destFileName != NULL)
            {
                printf("Destination: %s\n", destFileName);
            }
            // If all components are there, send to server
            if (fileName != NULL && destFileName != NULL && access(fileName, F_OK) != -1)
            {
                write(connectionSocket, inputCopy, MAX_LINE);
                FILE *file = fopen(fileName, "r");
                int fd = fileno(file);
                if (fstat(fd, &file_stat) < 0)
                {
                    printf("Error gettings stats\n");
                }
                char fileLength[256];
                sprintf(fileLength, "%d", file_stat.st_size);
                printf("The length of the file is %s bytes\n", fileLength);
                // First send file length, then send the file
                write(connectionSocket, fileLength, MAX_LINE);
                sendfile(connectionSocket, fd, NULL, file_stat.st_size);
            }
            else
            {
                if (fileName == NULL || destFileName == NULL)
                {
                    printf("USAGE: upload fileNameOnServer fileNameToBeOnClient\n");
                }
                else
                {
                    printf("The file you requested does not exist\n");
                }
                reply_msg[0] = '\0';
                reply_msg[1] = '\0';
            }
        }

        else
        {
            //Attempt to write to server
            if (write(connectionSocket, input, strlen(input) + 1) < 0)
            {
                printf("Failed to contact server with %s\n", input);
                return 1;
            }
        }

        //Read the response from the server
        while (strcmp(reply_msg, "\0\0") != 0)
        {
            read(connectionSocket, reply_msg, MAX_LINE);
            printf(reply_msg);
        }
        //Reset answer string so that we go back into the above loop
        reply_msg[0] = '1';
    }
    printf("Closing Connection\n");
    close(connectionSocket);
    return 0;
}