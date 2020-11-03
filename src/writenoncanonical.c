/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
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

#define CONTROL_0 0x00
#define CONTROL_1 0x40

#define ESC 0x7D
#define ESC_FLAG 0x5E
#define ESC_ESC 0x5D

#define DATA_PACKET 0x01
#define CONTROL_PACKET_START 0x02
#define CONTROL_PACKET_END 0x03

#define FILE_SIZE_FIELD 0x00
#define FILE_NAME_FIELD 0x01

#define C_RR_0	0x05
#define C_RR_1	0x85
#define C_REJ_0	0x01
#define C_REJ_1	0x81

#define MAX_PACKET_SIZE 256

#define FILENAME "pinguim.gif"

volatile int STOP=FALSE;
int alarmFlag = FALSE;
int conta = 0;
int frameNs = 0;

void atende() { // atende alarme
	printf("alarme # %d\n", ++conta);
	alarmFlag=TRUE;
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

int checkUA(char* ua) {
	
	for(int i=0; i<5; i++) {
		if(i == 0){
			if(ua[i] != (char) FLAG) {
				return FALSE;
			}
		}		
		else if(i == 1){
			if(ua[i] != (char) FIELD_A_SC) {
				return FALSE;
			}
		}
		else if(i == 2) {
			if(ua[i] != (char) CONTROL_UA) {
				return FALSE;
			}
		}
		else if(i == 3) {
			if(ua[i] != (char) (FIELD_A_SC ^ CONTROL_UA)) {
				return FALSE;
			}
		}
		else if(i == 4) {
			if(ua[i] != (char) FLAG) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

int llopen(int fd) {

  int tries = 3;
  char message[255];
  int index = 0, res;
  int correctUA = FALSE;
  int readTotalBytes;

  unsigned char buf[5];
  unsigned char SET[5];

  SET[0] = FLAG;
  SET[1] = FIELD_A_SC;
  SET[2] = CONTROL_SET;
  SET[3] = SET[1] ^ SET[2];
  SET[4] = FLAG;

  int time;

  while(tries > 0 && !correctUA) {

    printf("Writing SET... ");
    res = write(fd, SET, sizeof(SET));
    printf("SET sent\n");

    alarm(3);

    time = alarm(3);
      
    printf("Receiving UA... ");
    while (index < 5) {
      res = read(fd,buf,1);
      if (res == -1) {
        break;
      }
      buf[res] = 0;
      readTotalBytes += res;
      message[index++] = buf[0];
    }

    if (res == -1) {
      printf("UA was not read!\n");
      while (time = alarm(time)) {
        sleep(1);
      }
      alarmFlag = FALSE;
      tries--;
      continue;
    }

    if (readTotalBytes == 5) { //UA foi lido
      correctUA = checkUA(message);
      if (correctUA == FALSE) {
        printf("Error reading UA message!\n");
        while (time = alarm(time)) {
          sleep(1);
        }
        alarmFlag = FALSE;
        tries--;
        continue;
      }
      else {
        printf("UA received\n");
      }
    }
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

  if (dataPacketNum * MAX_PACKET_SIZE > fileSize) {
    *packetSize = dataPacketNum * MAX_PACKET_SIZE - fileSize;
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
  memcpy(&packet[4], &data, dataLength);

  return packet;
}

unsigned char getResponse(int fd){
  int position = 0;
  unsigned char c;
  unsigned char control;

  while (!alarmFlag && position != 5) {
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
        if (c == C_RR_0 || c == C_RR_1 || c == C_REJ_0 || c == C_REJ_1 || c == CONTROL_DISC) {
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
          position = 5;
          return control;
        }
        else
          position = 0;
        break;
    }
  }

  return 0x00;
}

int llwrite(int fd, char* buffer, int length) {
  
  int frameLength = length + 6;
  unsigned char *frame = malloc(frameLength * sizeof(unsigned char));

  int index = 0;

  frame[index++] = FLAG;          //F
  frame[index++] = FIELD_A_SC;    //A

  //C
  if (frameNs == 0) {
    frame[index++] = CONTROL_0;
    frameNs = 1;
  }
  else {
    frame[index++] = CONTROL_1;
    frameNs = 0;
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

  int sent = FALSE;
  int tries = 3;
  int res;

  while(tries > 0 && !sent) {
    alarmFlag = FALSE;
    alarm(3);

    res = write(fd, frame, index);

    unsigned char response = getResponse(fd);

    if(response == C_REJ_0 || response == C_REJ_1)   //rejected, resend frame
      tries--;
    else               //accepted
      sent = TRUE;

    alarm(0);   //reset alarm
    conta = 0;
  }

  if(tries == 0){
    printf("Error: Reached maximum number of tries\n");
    return -1;
  }

  return index;
}

int llclose(int fd) {

  int tries = 3;
  char message[255];
  int index = 0, res;
  int correctUA = FALSE;
  int readTotalBytes;

  unsigned char buf[5];
  unsigned char DISC[5];

  DISC[0] = FLAG;
  DISC[1] = FIELD_A_SC;
  DISC[2] = CONTROL_DISC;
  DISC[3] = DISC[1] ^ DISC[2];
  DISC[4] = FLAG;

  int time;

  while(tries > 0 && !correctUA) {

    printf("Writing DISC...\n");
    res = write(fd, DISC, sizeof(DISC));

    printf("Message sent:\n");
    printf("DISC: ");
    for (int i = 0; i < 5; i++) {
      printf("%4X ", DISC[i]);
    }
    printf("\n");

    time = alarm(3);
      
    printf("Receiving UA...\n");
    while (index < 5) {
      res = read(fd,buf,1);
      if (res == -1) {
        break;
      }
      buf[res] = 0;
      readTotalBytes += res;
      message[index++] = buf[0];
    }

    if (res == -1) {
      printf("UA was not read!\n");
      while (time = alarm(time)) {
        sleep(1);
      }
      alarmFlag = FALSE;
      tries--;
      continue;
    }

    if (readTotalBytes == 5) { //UA foi lido
      correctUA = checkUA(message);
      if (correctUA == FALSE) {
        printf("Error reading UA message!\n");
        while (time = alarm(time)) {
          sleep(1);
        }
        alarmFlag = FALSE;
        tries--;
        continue;
      }
      else {
        printf("Message received:\n");
        printf("UA: ");
        for (int i = 0; i < 5; i++) {
          printf("%4X ", message[i]);
        }
        printf("\n");
        alarm(0);
        conta = 0;
      }
    }
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



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

	  tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    (void) signal(SIGALRM, atende); //criar handler para sigalarm

    printf("Establishing connection...\n");

    if(llopen(fd) == -1) {
      perror("Error establishing connection\n");
      exit(1);
    }

    printf("Connection successfully established\n\n");

    unsigned char *fileName = FILENAME;
    off_t fileSize;

    printf("Opening file... ");

    unsigned char *file = openFile(fileName, &fileSize);

    printf("File successfully read\n\n");

    int packetSize = 0;

    unsigned char *controlPacket = buildControlPacket(CONTROL_PACKET_START, fileSize, fileName, &packetSize);

    printf("Sending first control packet... ");

    if(llwrite(fd, controlPacket, packetSize) == -1) {
      perror("Error sending first control packet\n");
      exit(1);
    }

    printf("First control packet sent\n\n");
    
    int numPackets = 1;
    int totalPackets = fileSize / MAX_PACKET_SIZE;

    if(fileSize % MAX_PACKET_SIZE != 0) {
      totalPackets++;
    }

    printf("Sending data packets...\n");
    
    //send message packets
    while(numPackets <= totalPackets) {
      unsigned char *data = splitFileData(file, fileSize, numPackets, &packetSize);

      unsigned char *dataPacket = buildDataPacket(data, &numPackets, &packetSize);

      if(llwrite(fd, dataPacket, packetSize) == -1) {
        perror("Error sending data packet\n");
        exit(1);
      }

      printf("Sent data packet number %d\n", numPackets);
    }

    printf("Data packets sent\n\n");

    controlPacket = buildControlPacket(CONTROL_PACKET_END, fileSize, fileName, &packetSize);

    printf("Sending final control packet... ");
    
    if(llwrite(fd, controlPacket, packetSize) == -1) {
      perror("Error sending final control packet\n");
      exit(1);
    }

    printf("Final control packet sent\n");

    printf("Disconnecting...");

    if(llclose(fd) == -1) {
      perror("Error closing connection\n");
      exit(1);
    }

    printf("Connection successfully closed\n");
 

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */



   
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}
