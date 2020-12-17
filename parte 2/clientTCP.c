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

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

    return h;
}

int createAndConnectToSocket(char *ip)
{
    int sockfd;
    struct sockaddr_in server_addr;
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    int bytes;

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(SERVER_PORT);   /*server TCP port must be network byte ordered */

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

int main(int argc, char **argv)
{
    struct UrlInfo urlInfo;
    int sockfd, bytes;
    char buf[255];
    struct hostent *h;

    if (argc < 2)
    {
        printf("Error: You must provide an url as an argument.\n");
        exit(2);
    }

    divideUrl(argv[1], strlen(argv[1]), &urlInfo);

    printf("Username: %s\n", urlInfo.user);
    printf("Password: %s\n", urlInfo.password);
    printf("Host: %s\n", urlInfo.host);
    printf("Path: %s\n", urlInfo.urlPath);
    printf("Filename: %s\n", urlInfo.filename);

    h = getip(urlInfo.host);

    if ((sockfd = createAndConnectToSocket(inet_ntoa(*((struct in_addr *)h->h_addr)))) == -1)
    {
        herror("error when creating socket or connecting");
        exit(1);
    }

    /*send a string to the server*/
    /*bytes = write(sockfd, buf, strlen(buf));
    printf("Bytes escritos %d\n", bytes);*/

    close(sockfd);
    exit(0);
}
