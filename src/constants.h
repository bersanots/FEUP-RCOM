#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define CONTROL_SET 0x03
#define CONTROL_UA 0x07
#define CONTROL_DISC 0x0B
#define FIELD_A_SC 0x03
#define FIELD_A_RC 0x01
#define SET_BCC (FIELD_A_SC ^ CONTROL_SET)
#define UA_BCC (FIELD_A_SC ^ CONTROL_UA)
#define DISC_BCC (FIELD_A_SC ^ CONTROL_DISC)

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

#define TIMEOUT 3
#define MAX_TRIES 3

#define FILENAME "pinguim.gif"