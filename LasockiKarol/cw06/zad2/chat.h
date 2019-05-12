#ifndef SYSOPY_CHAT_H
#define SYSOPY_CHAT_H


#define MAX_ARG_LEN 128
#define SERVER_QUEUE_NAME "/serverQueue"
#define QUEUE_PERMISSIONS 0777
#define MAX_CLIENTS 11


enum Type {
    STOP = 10L, LIST = 9L, FRIENDS = 8L, INIT = 7L, ECHO = 6L, TO_ALL = 5L, TO_FRIENDS = 4L, TO_ONE = 3L, DEL = 2L, ADD = 1L
};


struct Message {
    long type; // and priority as well
    int sender_id;
    int int_val;
    char argument[MAX_ARG_LEN];
};

#define MESSAGE_SIZE sizeof(struct Message)


#endif //SYSOPY_CHAT_H
