
#ifndef CONEXAO_H
#define CONEXAO_H

#include "lwip/tcp.h"


static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
void start_http_server(void);



#endif
