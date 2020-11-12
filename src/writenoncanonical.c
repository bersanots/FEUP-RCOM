/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "constants.h"
#include "SU_frame.h"

int alarmFlag = FALSE;
int contaAlarme = 0;
int frameNs = 0;
int framesSent = 0, RRcount = 0, REJcount = 0;

void atende() { // atende alarme
	printf("alarme #%d\n", ++contaAlarme);
	alarmFlag = TRUE;
}

unsigned char *openFile(unsigned char *fileName, off_t *fileSize) {

  int fd = open(fileName, O_RDONLY);

  if (fd == -1) {
    perror("Error opening file\n");
    exit(-1);
  }
  
  struct stat metadata;
  fstat(fd, &metadata);

  *fileSize = metadata.st_size;
  unsigned char *fileData = malloc((*fileSize) * sizeof(unsigned char));

  read(fd, fileData, *fileSize);
  
  return fileData;
}

int llopen(int fd) {

  int tries = MAX_TRIES;
  int correctUA = FALSE;

  while(tries > 0 && !correctUA) {

    printf("Writing SET... ");
    sendSUframe(fd, CONTROL_SET);
    printf("SET sent\n");

    alarmFlag = FALSE;
    alarm(TIMEOUT);
      
    printf("Receiving UA... ");
    unsigned char response = getSUframe(fd);

    if(response == CONTROL_UA) {
      printf("UA received\n");
      correctUA = TRUE;
    }
    else {          //resend SET
      printf("Error reading UA message!\n");
      tries--;
    }

    alarm(0);   //reset alarm
    contaAlarme = 0;
  }

  if(tries == 0){
    printf("Error: Reached maximum number of tries\n");
    return -1;
  }

  return 0;
}

unsigned char *buildControlPacket(unsigned char control, off_t fileSize, unsigned char *fileName, int *packetSize) {
  
  int fileSizeLength = sizeof(fileSize);
  int fileNameLength = strlen(fileName);
  *packetSize = 5 + fileSizeLength + fileNameLength;
  unsigned char *packet = malloc(*packetSize * sizeof(unsigned char));

  int index = 0;

  packet[index++] = control;                    //C

  packet[index++] = FILE_SIZE_FIELD;            //T1
  packet[index++] = fileSizeLength;             //L1

  //V1
  for (int i = fileSizeLength - 1; i >= 0; i--) {
      packet[index++] = (fileSize >> (8 * i)) & 0xFF;
  }

  packet[index++] = FILE_NAME_FIELD;            //T2
  packet[index++] = fileNameLength;             //L2

  //V2
  for (int i = 0; i < fileNameLength; i++) {
      packet[index++] = fileName[i];
  }

  return packet;
}

unsigned char *splitFileData(unsigned char *data, off_t fileSize, int dataPacketNum, int *packetSize) {

  *packetSize = MAX_PACKET_SIZE;

  if ((dataPacketNum + 1) * MAX_PACKET_SIZE > fileSize) {
    *packetSize = fileSize - dataPacketNum * MAX_PACKET_SIZE;
  }

  unsigned char *packet = malloc(*packetSize);

  memcpy(packet, &data[dataPacketNum * MAX_PACKET_SIZE], *packetSize);

  return packet;
}

unsigned char *buildDataPacket(unsigned char *data, int *dataPacketNum, int *packetSize) {

  int dataLength = *packetSize;
  *packetSize = 4 + dataLength;
  unsigned char *packet = malloc(*packetSize * sizeof(unsigned char));

  packet[0] = DATA_PACKET;                //C
  packet[1] = (*dataPacketNum)++ % 255;   //N
  packet[2] = (int)dataLength / 256;      //L2
  packet[3] = (int)dataLength % 256;      //L1

  //P
  memcpy(&packet[4], data, dataLength);

  return packet;
}

int llwrite(int fd, unsigned char* buffer, int length) {
  
  int frameLength = length + 6;
  unsigned char *frame = malloc(frameLength * sizeof(unsigned char));

  int index = 0;

  frame[index++] = FLAG;          //F
  frame[index++] = FIELD_A_SC;    //A

  //C
  if (frameNs == 0) {
    frame[index++] = CONTROL_0;
  }
  else {
    frame[index++] = CONTROL_1;
  }

  frame[index++] = frame[1] ^ frame[2];   //BCC1

  //Data (buffer) byte stuffing
  for (int i=0; i < length; i++) {
    if (buffer[i] == FLAG) {
      frame = realloc(frame, ++frameLength * sizeof(unsigned char));    //add a char to the original size
      frame[index++] = ESC;
      frame[index++] = ESC_FLAG;
    }
    else if (buffer[i] == ESC) {
      frame = realloc(frame, ++frameLength * sizeof(unsigned char));    //add a char to the original size
      frame[index++] = ESC;
      frame[index++] = ESC_ESC;
    }
    else {
      frame[index++] = buffer[i];
    }
  }

  unsigned char BCC2 = buffer[0];

  //BCC2 calculation
  for (int i=1; i < length; i++) {
    BCC2 ^= buffer[i];
  }

  //BCC2 byte stuffing
  if (BCC2 == FLAG) {
    frame = realloc(frame, ++frameLength * sizeof(unsigned char));    //add a char to the original size
    frame[index++] = ESC;
    frame[index++] = ESC_FLAG;
  }
  else if (BCC2 == ESC) {
    frame = realloc(frame, ++frameLength * sizeof(unsigned char));    //add a char to the original size
    frame[index++] = ESC;
    frame[index++] = ESC_ESC;
  }
  else {
    frame[index++] = BCC2;
  }

  frame[index++] = FLAG;    //F


  /* --- Send frame --- */

  int accepted = FALSE;
  int tries = MAX_TRIES;

  while(tries > 0 && !accepted) {

    write(fd, frame, index);
    framesSent++;

    alarmFlag = FALSE;
    alarm(TIMEOUT);

    unsigned char response = getSUframe(fd);

    if(response == C_RR_0 && frameNs == 1 || response == C_RR_1 && frameNs == 0) {
      accepted = TRUE;
      frameNs ^= 1;
    }
    else {          //resend frame
      printf("Resending frame...\n");
      tries--;
    }

    if(response == C_RR_0 || response == C_RR_1)
      RRcount++;
    else if(response == C_REJ_0 || response == C_REJ_1)
      REJcount++;

    alarm(0);   //reset alarm
    contaAlarme = 0;
  }

  if(tries == 0){
    printf("Error: Reached maximum number of tries\n");
    return -1;
  }

  return index;
}

int llclose(int fd) {

  int tries = MAX_TRIES;
  int correctUA = FALSE;

  while(tries > 0 && !correctUA) {

    printf("Writing DISC... ");
    sendSUframe(fd, CONTROL_DISC);
    printf("DISC sent\n");

    alarmFlag = FALSE;
    alarm(TIMEOUT);
      
    printf("Receiving UA... ");
    unsigned char response = getSUframe(fd);

    if(response == CONTROL_UA) {
      printf("UA received\n");
      correctUA = TRUE;
    }
    else {          //resend SET
      printf("Error reading UA message!\n");
      tries--;
    }

    alarm(0);   //reset alarm
    contaAlarme = 0;
  }

  if(tries == 0){
    printf("Error: Reached maximum number of tries\n");
    return -1;
  }

  return 0;
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;
    
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

    (void) signal(SIGALRM, atende); //criar handler para sigalarm

    clock_t start_t, end_t, total_t;

    start_t = clock();

    printf("[Establishing connection...]\n");

    if(llopen(fd) == -1) {
      perror("Error establishing connection\n");
      exit(1);
    }

    printf("Connection successfully established\n\n");

    unsigned char *fileName = FILENAME;
    off_t fileSize;

    printf("[Opening file...]\n");

    unsigned char *file = openFile(fileName, &fileSize);

    printf("File successfully read\n");
    
    printf("File name: %s     File size: %i bytes\n\n", fileName, fileSize);

    int packetSize = 0;

    unsigned char *controlPacket = buildControlPacket(CONTROL_PACKET_START, fileSize, fileName, &packetSize);

    printf("[Sending first control packet...]\n");

    if(llwrite(fd, controlPacket, packetSize) == -1) {
      perror("Error sending first control packet\n");
      exit(1);
    }

    printf("First control packet sent\n\n");
    
    int numPackets = 0;
    int totalPackets = fileSize / MAX_PACKET_SIZE;
    int totalSize = 0;

    if(fileSize % MAX_PACKET_SIZE != 0) {
      totalPackets++;
    }

    printf("[Sending data packets...]\n");
    
    //send message packets
    while(numPackets < totalPackets) {
      unsigned char *data = splitFileData(file, fileSize, numPackets, &packetSize);

      totalSize += packetSize;

      unsigned char *dataPacket = buildDataPacket(data, &numPackets, &packetSize);

      if(llwrite(fd, dataPacket, packetSize) == -1) {
        perror("Error sending data packet\n");
        exit(1);
      }

      printf("Sent data packet number %d with %d bytes\n", numPackets, packetSize - 4);
    }

    printf("All data packets sent\n");
    printf("Total size: %d bytes\n\n", totalSize);

    controlPacket = buildControlPacket(CONTROL_PACKET_END, fileSize, fileName, &packetSize);

    printf("[Sending final control packet...]\n");
    
    if(llwrite(fd, controlPacket, packetSize) == -1) {
      perror("Error sending final control packet\n");
      exit(1);
    }

    printf("Final control packet sent\n\n");

    printf("[Disconnecting...]\n");

    if(llclose(fd) == -1) {
      perror("Error closing connection\n");
      exit(1);
    }

    printf("Connection successfully closed\n\n");

    end_t = clock();

    total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;

    printf("[Protocol Efficiency Statistics:]\n");
    printf("Elapsed time: %f seconds\n", end_t);
    printf("Number of information frames sent: %d\n", framesSent);
    printf("Number of packets rejected (REJ frames received): %d\n", REJcount);
    printf("Number of packets accepted (RR frames received): %d\n", RRcount);

   
    if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}
