/*      (C)2000 FEUP  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"

struct UrlInfo
{
    char user[255];
    char password[255];
    char host[255];
    char urlPath[255];
    char filename[255];
};

struct hostent *getip(char host[])
{
    struct hostent *h;

    if ((h = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }

    printf("Host name: %s\n", h->h_name);
    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

    return h;
}

int createAndConnectToSocket(char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);          /*server TCP port must be network byte ordered */

    /*open an TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(0);
    }

    /*connect to the server*/
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(0);
    }

    return sockfd;
}

int readSocket(int sockfd, char *response)
{
    int index = 0;
    char c;
    int foundCode = 0;

    do
    {
        read(sockfd, &c, 1);
        response[index] = c;
        index++;
    } while (c != '\n');

    //check for 3 digit code followed by ' '
    if (!isdigit(response[0]) || !isdigit(response[1]) || !isdigit(response[2]) || response[3] != ' ')
    {
        printf("Error receiving response code\n");
        return 1;
    }

    return 0;
}

int divideUrl(char url[255], int size, struct UrlInfo *urlInfo)
{
    //ftp://[<user>:<password>@]<host>/<url-path>
    char strAux[255];
    int cont = 0;
    int index = 0;
    int lastSlash = 0;
    char hostname[255];
    int temUserPass = 0;

    memset(strAux, 0, 255);

    for (int i = 6; i < size; i++)
    {
        if (url[i] == '/')
        {
            lastSlash = i;
            if (cont == 0)
            {
                cont++;
                index = 0;
                strcpy(hostname, strAux);
                memset(strAux, 0, 255);
            }
        }
        strAux[index] = url[i];
        index++;
    }

    if (cont == 1)
    {
        index = 0;
        strcpy(urlInfo->urlPath, strAux);
        memset(strAux, 0, 255);

        for (int i = lastSlash + 1; i < size; i++)
        {
            strAux[index] = url[i];
            index++;
        }

        strcpy(urlInfo->filename, strAux);
    }

    index = 0;
    memset(strAux, 0, 255);

    for (int i = 0; i < strlen(hostname); i++)
    {
        if (hostname[i] == ':')
        {
            temUserPass = 1;
            index = 0;
            strcpy(urlInfo->user, strAux);
            memset(strAux, 0, 255);
            continue;
        }
        if (hostname[i] == '@')
        {
            temUserPass = 1;
            index = 0;
            strcpy(urlInfo->password, strAux);
            memset(strAux, 0, 255);
            continue;
        }
        strAux[index] = hostname[i];
        index++;
    }

    if (temUserPass == 0)
    {
        memset(urlInfo->password, 0, 255);
        memset(urlInfo->user, 0, 255);
    }

    strcpy(urlInfo->host, strAux);

    return 0;
}

int sendCommand(int sockfd, int readSock, const char *command, char *commandValue, char *response)
{
    int bytes = 0;

    //send the command to the server
    bytes += write(sockfd, command, strlen(command));
    bytes += write(sockfd, commandValue, strlen(commandValue));
    bytes += write(sockfd, "\n", 1);

    if (bytes <= 0)
    {
        perror("Error sending command\n");
        return 1;
    }

    printf("%s%s - %d bytes written\n", command, commandValue, bytes);

    if (readSock)
    {
        if (readSocket(sockfd, response) != 0)
        {
            perror("Error reading from socket\n");
            return 1;
        };

        printf("Server response: %s", response);
    }

    return 0;
}

int getIpAndPort(int sockfd, char *ip, int *portNum, char *string)
{
    if (strlen(string) < 39 || string[26] != '(')
    {
        perror("Invalid string\n");
        return 1;
    }

    char value[4];
    int port[2];
    int stringIdx = 27, valueIdx = 0, ipIdx = 0, portIdx = 0, ipCnt = 0;
    char c;

    do
    {
        c = string[stringIdx];
        if (c == ',' || c == ')')
        {
            if (ipCnt < 4)
            {
                if (ipCnt < 3)
                    ip[ipIdx] = '.';
                ipIdx++;
                ipCnt++;
            }
            else if (portIdx < 2)
            {
                port[portIdx] = atoi(value);
                portIdx++;
                bzero(value, 4);
                valueIdx = 0;
            }
        }
        else if (isdigit(c))
        {
            if (ipCnt < 4)
            {
                ip[ipIdx] = c;
                ipIdx++;
            }
            else if (portIdx < 2)
            {
                value[valueIdx] = c;
                valueIdx++;
            }
        }
        stringIdx++;
    } while (c != ')');

    //calculate TCP port number
    *portNum = port[0] * 256 + port[1];

    return 0;
}

int downloadFile(int fd, char *filename)
{
    char buf[255];
    int file_fd;
    int size;

    if ((file_fd = open(filename, O_WRONLY | O_CREAT, 0666)) < 0)
    {
        perror("Error opening file\n");
        return 1;
    }

    while ((size = read(fd, buf, 255)) > 0)
    {
        if (size == -1)
        {
            perror("Error reading file\n");
            return 1;
        }

        if (write(file_fd, buf, size) == -1)
        {
            perror("Error writing file\n");
            return 1;
        }
    }

    close(file_fd);

    printf("File %s was downloaded\n", filename);

    return 0;
}

int main(int argc, char **argv)
{
    struct UrlInfo urlInfo;
    int sockfd;
    struct hostent *h;

    if (argc < 2)
    {
        printf("Error: You must provide an url as an argument.\n");
        exit(1);
    }

    divideUrl(argv[1], strlen(argv[1]), &urlInfo);

    printf("Username: %s\n", urlInfo.user);
    printf("Password: %s\n", urlInfo.password);
    printf("Host: %s\n", urlInfo.host);
    printf("Path: %s\n", urlInfo.urlPath);
    printf("Filename: %s\n", urlInfo.filename);

    h = getip(urlInfo.host);

    if ((sockfd = createAndConnectToSocket(inet_ntoa(*((struct in_addr *)h->h_addr)), SERVER_PORT)) == -1)
    {
        perror("Error when creating or connecting to socket");
        exit(1);
    }

    printf("\n");

    char response[255];
    bzero(response, 255);

    printf("Server response: ");

    if (readSocket(sockfd, response))
    {
        perror("Error reading from socket\n");
        exit(1);
    }
    else if (response[0] == '2')
    {
        printf("%s", response);
        printf("FTP connection established\n");
    }
    else
    {
        perror("FTP connection denied");
        exit(1);
    }

    printf("\n");

    bzero(response, 255);
    printf("Sending username...\n");
    if (sendCommand(sockfd, 1, "user ", urlInfo.user, response))
    {
        perror("Error sending user command\n");
        exit(1);
    }
    else if (response[0] <= '5')
    {
        printf("Username sent\n");
    }
    else
    {
        perror("Username command denied");
        exit(1);
    }

    printf("\n");

    bzero(response, 255);
    printf("Sending password...\n");
    if (sendCommand(sockfd, 1, "pass ", urlInfo.password, response))
    {
        perror("Error sending password command\n");
        exit(1);
    }
    else if (response[0] == '2')
    {
        printf("Password sent\n");
    }
    else
    {
        perror("Password command denied");
        exit(1);
    }

    printf("\n");

    bzero(response, 255);
    printf("Entering passive mode...\n");
    if (sendCommand(sockfd, 1, "pasv", "", response))
    {
        perror("Error sending passive mode command");
        exit(1);
    }
    else if (response[0] != '2')
    {
        perror("Passive mode command denied");
        exit(1);
    }

    int port;
    char ip[32];

    if (getIpAndPort(sockfd, ip, &port, response))
    {
        perror("Error getting IP and Port\n");
        exit(1);
    }

    if ((sockfd = createAndConnectToSocket(ip, port)) == -1)
    {
        perror("Error when creating or connecting to socket");
        exit(1);
    }

    printf("Entered passive mode\n\n");

    bzero(response, 255);
    printf("Sending retr...\n");
    if (sendCommand(sockfd, 0, "retr ", urlInfo.urlPath, response))
    {
        perror("Error sending retr command\n");
        exit(1);
    }

    printf("Retr sent\n");

    printf("Downloading file...\n");
    downloadFile(sockfd, urlInfo.filename);

    close(sockfd);
    exit(0);
}
