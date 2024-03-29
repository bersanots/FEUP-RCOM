/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <time.h>
#include "constants.h"
#include "SU_frame.h"

int alarmFlag = FALSE;
int frameNs = 0;
int framesReceived = 0, RRcount = 0, REJcount = 0;

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

int llopen(int fd) {

    printf("Receiving SET... ");
    unsigned char response = getSUframe(fd);

    if(response == CONTROL_SET) {
      printf("SET received\n");
    }
    else {
      printf("Error receiving SET message!\n");
      exit(1);
    }
    
    printf("Writing UA... ");
    sendSUframe(fd, CONTROL_UA);
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
              sendSUframe(fd, C_RR_1);
            else
              sendSUframe(fd, C_RR_0);
            accept = TRUE;
            RRcount++;
          }
          else {
            if (frameNum == frameNs) {
              if (frameNum == 0)
                sendSUframe(fd, C_REJ_0);
              else
                sendSUframe(fd, C_REJ_1);
              REJcount++;
            }
            else {
              if (frameNum == 0)
                sendSUframe(fd, C_RR_1);
              else
                sendSUframe(fd, C_RR_0);
              RRcount++;
            }
            accept = FALSE;
          }
          position = 6;
          framesReceived++;          
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
  
  printf("Receiving DISC... ");
  unsigned char response = getSUframe(fd);

  if (response == CONTROL_DISC) {
    printf("DISC received\n");
  }
  else {
    printf("Error receiving DISC message!\n");
    exit(1);
  }
    
  printf("Writing UA... ");
  sendSUframe(fd, CONTROL_UA);
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
  

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n\n");

    clock_t start_t = clock();

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

    printf("File name: %s     File size: %i bytes\n\n", fileName, fileSize);

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

    printf("Connection successfully closed\n\n");

    clock_t end_t = clock();

    printf("[Protocol Efficiency Statistics:]\n");
    printf("Elapsed time: %f seconds\n", (end_t - start_t) / (double) CLOCKS_PER_SEC);
    printf("Number of information frames received: %d\n", framesReceived);
    printf("Number of packets rejected (REJ frames sent): %d\n", REJcount);
    printf("Number of packets accepted (RR frames sent): %d\n", RRcount);


    if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}
