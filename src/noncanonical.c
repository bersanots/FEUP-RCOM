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
#define CONTROL_SET 0x03
#define CONTROL_DISC 0x0B
#define CONTROL_UA 0x07
#define FIELD_A_SC 0x03
#define FIELD_A_RC 0x01
#define SET_BCC (FIELD_A_SC ^ CONTROL_SET)
#define UA_BCC (FIELD_A_SC ^ CONTROL_UA)
#define BCC_DISC ()


#define CONTROL_0 0x00
#define CONTROL_1 0x40

#define ESC 0x7D
#define ESC_FLAG 0x5E
#define ESC_ESC 0x5D

#define DATA_PACKET 0x01
#define CONTROL_PACKET_START 0x02
#define CONTROL_PACKET_END 0x03

#define DATA_PACKET_START 0x01

#define FILE_SIZE_FIELD 0x00
#define FILE_NAME_FIELD 0x01

#define C_RR_0	0x05
#define C_RR_1	0x85
#define C_REJ_0	0x01
#define C_REJ_1	0x81

#define MAX_PACKET_SIZE 256

volatile int STOP=FALSE;
int success;
int frameNs = 0;


int checkSET(unsigned char* SET) {

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

int checkDISC(unsigned char* DISC) {

  for(int i=0; i<5; i++) {
    if(DISC[0] != (char) FLAG) {
      return FALSE;
    }
    if(DISC[1] != (char) FIELD_A_SC) {
      return FALSE;
    }
    if(DISC[2] != (char) CONTROL_DISC) {
      return FALSE;
    }
    if(DISC[3] != (char) (FIELD_A_SC ^ CONTROL_DISC)) {
      return FALSE;
    }
    if(DISC[4] != (char) FLAG) {
      return FALSE;
    }
  }
  return TRUE;
}

int readControlPacket(unsigned char control, unsigned char* buffer, off_t *fileSize, unsigned char *fileName, int *fileNameLength) {

  int index = 0;
  *fileSize = 0;

  if(buffer[index++] != control)              //C
    return -1;

  if(buffer[index++] != FILE_SIZE_FIELD)      //T1
    return -1;

  int fileSizeLength = buffer[index++];       //L1

  //V1
  for (int i = fileSizeLength - 1; i >= 0; i--) {
    *fileSize |= (buffer[index++] << (8 * i));
  }

  if(buffer[index++] != FILE_NAME_FIELD)       //T2
    return -1;

  *fileNameLength = buffer[index++];           //L2

  fileName = realloc(fileName, *fileNameLength + 1);

  //V2
  for (int i = 0; i < *fileNameLength; i++) {
    fileName[i] = buffer[index++];
  }

  fileName[*fileNameLength] = '\0';

  return 0;
}

int sendResponse(int fd, unsigned char control) {
  unsigned char response[5];

  response[0] = FLAG;
  response[1] = FIELD_A_SC;
  response[2] = control;
  response[3] = response[1] ^ response[2];
  response[4] = FLAG;
  
  if(write(fd, response, 5) < 0) {
    return 1;
  }

  return 0;
}

int llopen(int fd) {
    
    unsigned char SET[5], UA[5];
    unsigned char buf[5];
    int index = 0, res;
    
    printf("Receiving SET... ");
    while (index < 5) {       
      res = read(fd,buf,1);   
      buf[res] = 0;               
      SET[index++] = buf[0]; 
    }

    if(checkSET(SET) == FALSE) {
      printf("Error receiving SET message!\n");
      exit(1);
    }

    printf("SET received\n");
    
    UA[0] = FLAG;
    UA[1] = FIELD_A_SC;
    UA[2] = CONTROL_UA;
    UA[3] = UA[1] ^ UA[2];
    UA[4] = FLAG;
    
    printf("Writing UA... ");
    write(fd, UA, sizeof(UA));
    printf("UA sent\n");

    return 0; 
}

int checkBCC2(unsigned char *buf, int bufSize) {
  unsigned char BCC2 = buf[0];

  for (int i = 1; i < bufSize - 1; i++)
    BCC2 ^= buf[i];

  return BCC2 == buf[bufSize - 1];
}

unsigned char *llread(int fd, int* size) {

  int packetSize = 0;
  int position = 0;
  int frameNum = 0;
  int accept = FALSE;
  unsigned char c;
  unsigned char control;
  unsigned char *buffer = malloc(0);

  while (position != 6) {
    read(fd, &c, 1);
    switch (position) {
      case 0:
        if (c == FLAG)
          position = 1;
        break;
      case 1:
        if (c == FIELD_A_SC)
          position = 2;
        else if (c == FLAG)
          position = 1;
        else
          position = 0;
        break;
      case 2:
        if (c == CONTROL_0) {
          frameNum = 0;
          control = c;
          position = 3;
        }
        else if (c == CONTROL_1) {
          frameNum = 1;
          control = c;
          position = 3;
        }
        else if (c == FLAG)
          position = 1;
        else
          position = 0;
        break;
      case 3:
        if (c == FIELD_A_SC ^ control)
          position = 4;
        else
          position = 0;
        break;
      case 4:
        if (c == FLAG) {
          if (checkBCC2(buffer, packetSize)) {
            if (frameNum == 0)
              sendResponse(fd, C_RR_1);
            else
              sendResponse(fd, C_RR_0);
            position = 6;
            accept = TRUE;
          }
          else {
            if (frameNum == 0)
              sendResponse(fd, C_REJ_0);
            else
              sendResponse(fd, C_REJ_1);
            position = 6;
            accept = FALSE;
          }
        }
        else if (c == ESC)
          position = 5;
        else {
          buffer = realloc(buffer, ++packetSize);
          buffer[packetSize - 1] = c;
        }
        break;
      case 5:
        if (c == ESC_FLAG) {
          buffer = realloc(buffer, ++packetSize);
          buffer[packetSize - 1] = FLAG;
        }
        else if (c == ESC_ESC) {
          buffer = realloc(buffer, ++packetSize);
          buffer[packetSize - 1] = ESC;
        }
        else {
          perror("Invalid character after escape");
          exit(-1);
        }
        position = 4;
        break;
    }
  }

  buffer = realloc(buffer, --packetSize);   //remove BCC2

  if (accept && frameNum == frameNs)
    frameNs ^= 1;
  else
    packetSize = 0;   //rejected or discard duplicate

  *size = packetSize;

  return buffer;
}

int llclose(int fd) {

  unsigned char DISC[5], UA[5];
  unsigned char buf[5];
  int index = 0, res;
  
  printf("Receiving DISC... ");

  while (index < 5) {       
    res = read(fd,buf,1);   
    buf[res] = 0;               
    DISC[index++] = buf[0]; 
  }

  if(checkDISC(DISC) == FALSE) {
    printf("Error receiving DISC message!\n");
    exit(1);
  }

  printf("DISC received\n");
    
  UA[0] = FLAG;
  UA[1] = FIELD_A_SC;
  UA[2] = CONTROL_UA;
  UA[3] = UA[1] ^ UA[2];
  UA[4] = FLAG;
    
  printf("Writing UA... ");
  write(fd, UA, sizeof(UA));
  printf("UA sent\n");

  return 0;
}

int main(int argc, char** argv) {
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    /*if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }*/

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

    printf("New termios structure set\n\n");

    printf("[Establishing connection...]\n");
    
    if(llopen(fd) == -1) {
      perror("Error establishing connection\n");
      exit(1);
    }

    printf("Connection successfully established\n\n");

    printf("[Reading first control packet...]\n");

    int controlPacketSize;
    unsigned char *controlPacket = llread(fd, &controlPacketSize);

    if(controlPacketSize == -1) {
      perror("Error reading first control packet\n");
      exit(1);
    }

    unsigned char *fileName = malloc(0);
    off_t fileSize = 0;
    int fileNameLength = 0;

    readControlPacket(CONTROL_PACKET_START, controlPacket, &fileSize, fileName, &fileNameLength);

    printf("First control packet read\n\n");

    int numPackets = 0;
    int packetSize;
    int totalSize = 0;

    int file_fd = open(fileName, O_RDWR | O_CREAT , 777);

    printf("[Reading data packets...]\n");

    while(1) {
      unsigned char *packet = llread(fd, &packetSize);

      if(packetSize == -1) {
        perror("Error reading data packet\n");
        exit(1);
      }
      else if (packetSize == 0) continue;

      if(packet[0] == DATA_PACKET) {
        int dataSize = 256 * packet[2] + packet[3];
        write(file_fd, &packet[4], dataSize);
        printf("Read data packet number %d with %d bytes\n", ++numPackets, dataSize);
        totalSize += dataSize;
      }
      else if(packet[0] == CONTROL_PACKET_END) {
        break;
      }
    }

    printf("All data packets read\n");
    printf("Total size: %d bytes\n\n", totalSize);

    printf("Final control packet read\n\n");

    close(file_fd);

    printf("[Disconnecting...]\n");

    if(llclose(fd) == -1) {
      perror("Error closing connection\n");
      exit(1);
    }

    printf("Connection successfully closed\n");
  

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
