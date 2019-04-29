#ifndef SYSOPY_CHAT_H
#define SYSOPY_CHAT_H


#define MAX_ARG_LEN 1024
#define SERVER_ID_PATH "/etc"
#define SERVER_ID_SEED 11
#define QUEUE_PERMISSIONS 0666
#define PRIORITY_QUEUE -10
#define MAX_CLIENTS 11


enum Type {
    STOP = 1L, LIST = 2L, FRIENDS = 3L, INIT = 4L, ECHO = 5L, TO_ALL = 6L, TO_FRIENDS = 7L, TO_ONE = 8L, DEL = 9L, ADD = 10L
};


struct Message {
    long type; // and priority as well
    int sender_id;
    int int_val;
    char argument[MAX_ARG_LEN];
};

#define MESSAGE_SIZE sizeof(struct Message) - sizeof(long)


#endif //SYSOPY_CHAT_H
