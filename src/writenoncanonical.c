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
#define CONTROL_DISC 0x0b
#define CONTROL_UA 0x07
#define FIELD_A_SC 0x03
#define FIELD_A_RC 0x01
#define SET_BCC (FIELD_A_SC ^ CONTROL_SET)
#define UA_BCC (FIELD_A_SC ^ CONTROL_UA)

#define DATA_PACKET 0x01
#define CONTROL_PACKET_START 0x02
#define CONTROL_PACKET_END 0x03

#define FILE_SIZE_FIELD 0x00
#define FILE_NAME_FIELD 0x01

#define MAX_PACKET_SIZE 256

volatile int STOP=FALSE;
int alarmFlag = FALSE;
int conta = 0;

void atende() { // atende alarme
	printf("alarme # %d\n", ++conta);
	alarmFlag=TRUE;
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

      printf("Writing SET...\n");
      res = write(fd, SET, sizeof(SET));

      printf("Message sent:\n");
      printf("SET: ");
      for (int i = 0; i < 5; i++) {
        printf("%4X ", SET[i]);
      }
      printf("\n");

      alarm(3);

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
        }
      }
  }

  return 0;
}

unsigned char *buildControlPacket(unsigned char control, off_t fileSize, unsigned char *fileName) {
  
  int fileSizeLength = sizeof(fileSize);
  int fileNameLength = strlen(fileName);
  unsigned char *packet = malloc((5 + fileSizeLength + fileNameLength) * sizeof(unsigned char));

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

unsigned char *splitFileData(unsigned char *data, off_t fileSize, int dataPacketNum) {

  int packetSize = MAX_PACKET_SIZE;

  if (dataPacketNum * MAX_PACKET_SIZE > fileSize) {
    packetSize = dataPacketNum * MAX_PACKET_SIZE - fileSize;
  }

  unsigned char *packet = malloc(packetSize);

  memcpy(packet, &data[dataPacketNum * MAX_PACKET_SIZE], packetSize);

  return packet;
}

unsigned char *buildDataPacket(unsigned char *data, int *dataPacketNum) {

  int dataLength = strlen(data);
  unsigned char *packet = malloc((4 + dataLength) * sizeof(unsigned char));

  packet[0] = DATA_PACKET;                //C
  packet[1] = (*dataPacketNum)++ % 255;   //N
  packet[2] = (int)dataLength / 256;      //L2
  packet[3] = (int)dataLength % 256;      //L1

  //P
  memcpy(&packet[4], &data, dataLength);

  return packet;
}

int llwrite(int fd, char* buffer, int length) {

  /*int index = 0, res;

  unsigned char message[255];

  

  printf("Writing SET...\n");
  res = write(fd, message, sizeof(message));*/

  return 0;
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;
    
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
    leitura do(s) pr�ximo(s) caracter(es)
  */

	  tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    (void) signal(SIGALRM, atende); //criar handler para sigalarm

    llopen(fd);

    unsigned char *fileName = "pinguim.gif";
    off_t fileSize = 3200;

    unsigned char *controlPacket = buildControlPacket(CONTROL_PACKET_START, fileSize, fileName);
    //llwrite(fd, controlPacket, strlen(controlPacket));

    unsigned char *file = "abdn1 rfdnwuqowijmsw2198qdubhdvbuwb!_ddueoidb12e/$iwubdiubdeiub";
    
    int numPackets = 0;
    int totalPackets = fileSize / MAX_PACKET_SIZE;

    if(fileSize % MAX_PACKET_SIZE != 0) {
      totalPackets++;
    }
    
    //send message packets
    while(numPackets < totalPackets) {

      unsigned char *data = splitFileData(file, fileSize, numPackets);
    
      unsigned char *dataPacket = buildDataPacket(data, &numPackets);

      //llwrite(fd, dataPacket, strlen(dataPacket));
    }

    controlPacket = buildControlPacket(CONTROL_PACKET_END, fileSize, fileName);
    //llwrite(fd, controlPacket, strlen(controlPacket));

    //llclose(fd);
 

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
