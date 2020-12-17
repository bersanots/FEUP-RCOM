#include "SU_frame.h"

int sendSUframe(int fd, unsigned char control) {
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

unsigned char getSUframe(int fd){
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
        if (c == C_RR_0 || c == C_RR_1 || c == C_REJ_0 || c == C_REJ_1 || c == CONTROL_SET || c == CONTROL_UA || c == CONTROL_DISC) {
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

  return 0xFF;
}