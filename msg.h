#include "mfs.h"

#define INIT (0)
#define LOOKUP (1)
#define STAT (2)
#define WRITE (3)
#define READ (4)
#define CREAT (5)
#define UNLINK (6)
#define SHUTDOWN (7)

typedef struct{
    int func;
    char *hostname;
    int port;
    int pinum;
    char name[4096];
    int inum;
    // MFS_Stat_t *m; potentially remove
    char buffer[4096];
    int offset;
    int nbytes;
    int type;
} msg_t;
// move buffer outside and increase size then concatenate the struct msg_t to the buffer in the mfs write or whatever
typedef struct{
    int inode;
    int type;
    int size;
    char buffer[4096];
    int returnCode;
    MFS_Stat_t m;
} s_msg_t;
