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

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"

struct UrlInfo {
	char user[255];
	char password[255];
	char host[255];
	char urlPath[255];
};


int divideUrl(char url[255], int size, struct UrlInfo* urlInfo) {

	//ftp://[<user>:<password>@]<host>/<url-path>
	int i=0;
	char strAux[255];
	int cont = 0;
	int index = 0;
	char hostname[255];
	int temUserPass = 0;

	memset(strAux, 0, 255);

	for(int i=6; i<size; i++) {
		if(url[i] == '/' && cont == 0) {
			cont++;
			index = 0;
			strcpy(hostname, strAux);
			memset(strAux, 0, 255);
		}
		if(url[i] == '/' && cont == 1) {
			index = 0;
			strcpy(urlInfo->urlPath, strAux);
			memset(strAux, 0, 255);
		}
		strAux[index] = url[i];
		index++;
	}
	
	index = 0;

	printf("%s\n", hostname);

	for(int i=0; i<strlen(hostname); i++) {
		if(hostname[i] == ':') {
			temUserPass = 1;
			index = 0;
			strcpy(urlInfo->user, strAux);
			memset(strAux, 0, 255);
			continue;
		}
		if(hostname[i] == '@') {
			temUserPass = 1;
			index = 0;
			strcpy(urlInfo->password, strAux);
			memset(strAux, 0, 255);
			continue;
		}
		strAux[index] = hostname[i];
		index++;
	}

	if(temUserPass == 0) {
		memset(urlInfo->password, 0, 255);
		memset(urlInfo->user, 0, 255);
	}
	
	strcpy(urlInfo->host, strAux);
}


int main(int argc, char** argv){

	int	sockfd;
	struct	sockaddr_in server_addr;
	char	buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";  
	int	bytes;
	struct UrlInfo urlInfo;


	divideUrl(argv[1], strlen(argv[1]), &urlInfo);

	printf("%s\n", urlInfo.host);
	printf("%s\n", urlInfo.password);
	printf("%s\n", urlInfo.urlPath);
	printf("%s\n", urlInfo.user);

	
	/*server address handling*/
	//bzero((char*)&server_addr,sizeof(server_addr));
	//server_addr.sin_family = AF_INET;
	//server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);	/*32 bit Internet address network byte ordered*/
	//server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	/*
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
		*/
	/*connect to the server*/
    	/*
		if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}
	*/
    	/*send a string to the server*/
	//bytes = write(sockfd, buf, strlen(buf));
	//printf("Bytes escritos %d\n", bytes);

	//close(sockfd);
	exit(0);
}


