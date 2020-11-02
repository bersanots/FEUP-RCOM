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

enum DataFrameState { FLAG_RCV, A_RCV, C_RCV, BCC1_RCV, D_RCV };


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

int checkDisc(char* DISC, int index) {

  for(int i=0; i<index; i++) {
    if(DISC[0] != (char) FLAG) {
      return FALSE;
    }
    if(DISC[1] != (char) FIELD_A_SC) {
      return FALSE;
    }
    if(DISC[2] != (char) CONTROL_DISC) {
      return FALSE;
    }
    if(DISC[3] != (char) (FIELD_A_RC ^ CONTROL_DISC)) {
      return FALSE;
    }
    if(DISC[4] != (char) FLAG) {
      return FALSE;
    }
  }
  return TRUE;
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

int checkControlPacket(int size, char* buffer) {
  
  for(int i=0; i<size; i++) {
    if(buffer[1] != (char) CONTROL_PACKET_START) {
      return FALSE;
    }
  }
  return TRUE;
}

int checkDataPacket(int size, char* buffer) {

  for(int i=0; i<size; i++) {
    if(buffer[index] != (char) DATA_PACKET_START) {
      return FALSE;
    }
  }

  return TRUE;

}

int readControlPacket(int fd, char* buffer) {

  char byte;
  int index = 0;

  while(read(fd, &byte, 1) > 0) {
    buffer[index++] = byte;
  }

  if(checkControlPacket(index, buffer)) {
    return FALSE;
  } 

  return TRUE; 
}

int readDataPacket(int fd, char* buffer) {

  char byte;
  int index;

  while(read(fd, &byte, 1) > 0) {
    buffer[index++] = byte;
  }

  if(checkDataPacket(index, buffer)) {
    return FALSE;
  }

  return TRUE;
}

int readDiscFrame(int fd, char* buffer) {

  char byte;
  int index = 0;

  while(read(fd, &byte, 1) > 0) {
    buffer[index++] = byte;
  }

  if(checkDisc(buffer, index)) {
    return 1;
  }

  return 0;
}

int writeDiscFrame(int fd) {

  char DISC[5];

  DISC[0] = FLAG; 
  DISC[1] = FIELD_A_SC;
  DISC[2] = CONTROL_DISC;
  DISC[3] = (FIELD_A_RC ^ DISC);
  DISC[4] = FLAG;

  if(write(fd, DISC, 5) < 0) {
    return 1;
  }

  return 0;
}

int readUa(int fd) {

  char byte;
  int index = 0;
  int res;
  char UA[5];
  
  while((res = readUa(fd, &byte, 1)) > 0) {
    buffer[index++] = byte;
    if(res == -1) {
      exit(1);
    }
  }
  
  if(checkUA(UA)) {
    return 1;
  }

  return 0;
}

int llopen(int fd) {
    
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
    printf("SET: ");
    for (int i = 0; i < 5; i++) {
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

int llread(int fd, char* buffer) {  
  
  readControlPacket(int fd, char* buffer);

  readDataPacket(int fd, char* buffer);
}

int llclose(int fd) {

  readDiscFrame(fd, buffer);

  writeDiscFrame(fd);

  readUa(fd);
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
    

    llopen(fd);

  

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
