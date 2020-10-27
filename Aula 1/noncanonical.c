/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define CONTROL_SET 0x02
#define CONTROL_DISC 0x0b
#define CONTROL_UA 0x07
#define FIELD_A_SC 0x03
#define FIELD_A_RC 0x01
#define BCC1 0x00
#define BCC2 0x00

volatile int STOP=FALSE;

int checkSET(char* SET) {

  for(int i=0; i<5; i++) {
    if(i == 0) {
      if(SET[0] != (char) FLAG) {
        return FALSE;
      }
    }
    else if(i == 1) {
      if(SET[1] != (char) FIELD_A_SC) {
        return FALSE;
      }
    }
    else if(i == 2) {
      if(SET[2] != (char) CONTROL_SET) {
        return FALSE;
      }
    }
    else if(i == 3) {
      if(SET[3] != (char) (FIELD_A_SC ^ CONTROL_SET)) {
        return FALSE;
      }
    }
    else if(i == 4) {
      if(SET[4] != (char) FLAG) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

int lopen(int fd) {
    
    unsigned char SET[5], UA[5];
    unsigned char buf[5];
    int index = 0, res;
    
    printf("Receiving SET...\n");
    while (index < 5) {       
      res = read(fd,buf,1);   
      buf[res] = 0;               
      SET[index++] = buf[0]; 
    }

    printf("Message Received:\n");
    printf("SET:\n");
    for(int i=0; i<5; i++) {
        printf("%4X", SET[i]);    
    }
    printf("\n"); 

    if(checkSET(SET) == FALSE) {
      printf("Error receiving SET message!\n");
      exit(1);
    }
    
    UA[0] = FLAG;
    UA[1] = FIELD_A_SC;
    UA[2] = CONTROL_UA;
    UA[3] = UA[1] ^ UA[2];
    UA[4] = FLAG;
	
    printf("Writing UA...\n");
    write(fd, UA, sizeof(UA));

    printf("Message sent:\n");
	  printf("UA: ");
    for (int i = 0; i < 5; i++){
       printf("%4X ", UA[i]);
    }
    printf("\n");

    return 0; 
}

int lread(int fd, char* buffer) {  
  
  char *readChar;
  int index=0;

  while(readChar != '\0') {
    read(fd, readChar, 1);
    buffer[index++] = *readChar;
  }

  index = 0;
  printf("Message Recieved:\n");
  printf("I:\n");
  while(TRUE) {
    printf("%4X", buffer[index]);
    if(buffer[index] == '\0') {
      break;
    }
    index++;
  }
  printf("\n");

  return 1;
}

int main(int argc, char** argv) {
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }
  
  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */
  
  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");
    

    lopen(fd);

    /*
    char message[255];
    int index = 0;

    while (index < 5) {*/       /* loop for input */
      //res = read(fd,buf,1);   /* returns after 5 chars have been input */
      //buf[res]=0;               /* so we can printf... */
      //printf(":%s:%d\n", buf, res);
      //message[index++] = buf[0];
      //if (buf[0]=='\0') STOP=TRUE;
    //}
    /*
    printf("Message received:\n");
    printf("SET: ");
      for (int i = 0; i < 5; i++){
      printf("%4X ", message[i]);
    }
    printf("\n");
    */
    //printf("Message received: %s\n", message);
    //printf("%d bytes received\n", index);
    
    /*
    unsigned char UA[5];
    UA[0] = FLAG;
    UA[1] = FIELD_A_SC;
    UA[2] = CONTROL_UA;
    UA[3] = UA[1] ^ UA[2];
    UA[4] = FLAG;

    printf("\nMessage sent:\n");    
    printf("UA: ");
    for (int i = 0; i < 5; i++){  
      printf("%4X ", UA[i]);
    }
    printf("\n");

    res = write(fd, UA, sizeof(UA));
    */

  
    //res = write(fd,message,index);
    //printf("Message resent: %s\n", message);
    //printf("%d bytes written\n", res);

  

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
