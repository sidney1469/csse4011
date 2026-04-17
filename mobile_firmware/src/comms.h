#ifndef COMMS_H
#define COMMS_H

int send_comms(uint8_t *string);
int init_comms(void);
void comms_thread(void *a, void *b, void *c);

#endif
