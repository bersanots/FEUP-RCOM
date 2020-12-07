#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

#include "constants.h"

extern int alarmFlag;

int sendSUframe(int fd, unsigned char control);

unsigned char getSUframe(int fd);