#ifndef COMMS_H
#define COMMS_H

int send_comms(int8_t *data, uint16_t len); // Pass length explicitly
int init_comms(void);
void comms_thread(void *a, void *b, void *c);

#endif
