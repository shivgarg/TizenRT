struct myparm
{   
    int argc;
    char **argv;
 };

int opus_demo_entry(void *arg);
int socket_test(void *param);

#define OPUS_ENCODE_START 1
#define OPUS_ENCODE_END 2
#define OPUS_ENCODE_EXIT 3

#define OPUS_DECODE_START 4
#define OPUS_DECODE_END 5
#define OPUS_MSG_PRIO 6

typedef struct opus_msg
{
  uint32_t seqnum;
  uint16_t code;        /* Message ID */
  struct timeval ts; 
}opus_msg_s;
