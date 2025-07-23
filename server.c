#include "server.h"   
#include "globals.h"  
#include <string.h>   
#include <stdio.h>    
#include <stdlib.h>   

// Conteúdo HTML da página que será exibida no navegador
const char HTML_BODY[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Monitor</title>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"  // Responsividade
"<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"         // Biblioteca para gráficos
"<style>"                                                               // Estilos CSS da página
"body{font-family:sans-serif;text-align:center;background:#eef2f3;margin:0;padding:10px}"
".box{margin:auto;padding:10px;border:1px solid #ccc;border-radius:10px;width:98%;background:#fff;box-shadow:0 2px 6px rgba(0,0,0,.1)}"
".linha{display:flex;justify-content:space-around;flex-wrap:wrap}"
".bloco{display:flex;flex-direction:column;align-items:center;width:30%}"
"canvas{width:100%;max-width:300px;height:200px}"
".valor{font-size:1.1em;margin:5px;color:#444}"
".ajustes{display:flex;justify-content:space-between;text-align:left;margin-top:10px;gap:20px;flex-wrap:wrap}"
"fieldset{border:none;padding:0;margin:0;min-width:220px}"
"label{font-size:0.9em;display:inline-block;width:100px}"
"input{width:80px;margin:2px 0}"
"</style>"
// Script JavaScript para lidar com atualização de dados e gráficos
"<script>let max=20,temp=[],umid=[],alt=[],labels=[];"
"function atualizar(){fetch('/dados').then(r=>r.json()).then(d=>{"  // Requisição dos dados
"document.getElementById('temp').innerText=d.temp_aht.toFixed(1)+' °C';"
"document.getElementById('umid').innerText=d.umidade.toFixed(1)+' %';"
"document.getElementById('alt').innerText=d.altitude.toFixed(1)+' m';"
"document.getElementById('offsetTemp').value=d.offsetTemp;"
"document.getElementById('offsetUmi').value=d.offsetUmi;"
"document.getElementById('offsetAlt').value=d.offsetAlt;"
"if(temp.length>=max){temp.shift();umid.shift();alt.shift();labels.shift();}"
"temp.push(d.temp_aht);umid.push(d.umidade);alt.push(d.altitude);labels.push('');"
"g1.update();g2.update();g3.update();});}"
"function atualizarOffset(){const t=parseFloat(offsetTemp.value),u=parseFloat(offsetUmi.value),a=parseFloat(offsetAlt.value);fetch(`/set_offset?temp=${t}&umi=${u}&alt=${a}`).then(atualizar);}"
"function atualizarLimites(){const maxt=parseFloat(maxTemp.value),mint=parseFloat(minTemp.value),maxu=parseFloat(maxUmi.value),minu=parseFloat(minUmi.value),maxa=parseFloat(maxAlt.value),mina=parseFloat(minAlt.value);fetch(`/set_limits?maxt=${maxt}&mint=${mint}&maxu=${maxu}&minu=${minu}&maxa=${maxa}&mina=${mina}`).then(atualizar);}"
"let g1,g2,g3;window.onload=()=>{"
"offsetTemp.oninput=offsetUmi.oninput=offsetAlt.oninput=atualizarOffset;"  // Atualiza offset ao digitar
"g1=new Chart(g1ctx.getContext('2d'),{type:'line',data:{labels,datasets:[{data:temp,borderColor:'red',fill:false}]},options:{plugins:{legend:{display:false}},scales:{y:{beginAtZero:true}}}});"
"g2=new Chart(g2ctx.getContext('2d'),{type:'line',data:{labels,datasets:[{data:umid,borderColor:'blue',fill:false}]},options:{plugins:{legend:{display:false}},scales:{y:{beginAtZero:true}}}});"
"g3=new Chart(g3ctx.getContext('2d'),{type:'line',data:{labels,datasets:[{data:alt,borderColor:'green',fill:false}]},options:{plugins:{legend:{display:false}},scales:{y:{beginAtZero:true}}}});"
"atualizar();setInterval(atualizar,3000);}"  // Atualiza a cada 3 segundos
"</script></head><body><div class='box'>"
"<h2>Estação Meteorológica</h2>"
// Blocos de exibição de temperatura, umidade e altitude
"<div class='linha'>"
"<div class='bloco'><div>Temperatura</div><div class='valor' id='temp'>--</div><canvas id='g1ctx'></canvas></div>"
"<div class='bloco'><div>Umidade</div><div class='valor' id='umid'>--</div><canvas id='g2ctx'></canvas></div>"
"<div class='bloco'><div>Altitude</div><div class='valor' id='alt'>--</div><canvas id='g3ctx'></canvas></div>"
"</div>"
// Formulário para ajustar offsets e limites
"<div class='ajustes'>"
"<fieldset><legend><b>Calibração</b></legend>"
"<label>Temperatura:</label><input id='offsetTemp' type='number' step='0.1'><br>"
"<label>Umidade:</label><input id='offsetUmi' type='number' step='0.1'><br>"
"<label>Altitude:</label><input id='offsetAlt' type='number' step='0.1'><br>"
"</fieldset>"
"<fieldset>"
"<legend style='text-align:center; font-weight:bold;'>Limites</legend>"
"<div class='limites-grid'>"
"<div>"
"<label>Max Temp:</label><input id='maxTemp' type='number' step='0.1'><br>"
"<label>Min Temp:</label><input id='minTemp' type='number' step='0.1'><br>"
"<label>Max Umidade:</label><input id='maxUmi' type='number' step='0.1'><br>"
"</div>"
"<div>"
"<label>Min Umidade:</label><input id='minUmi' type='number' step='0.1'><br>"
"<label>Max Altitude:</label><input id='maxAlt' type='number' step='0.1'><br>"
"<label>Min Altitude:</label><input id='minAlt' type='number' step='0.1'><br>"
"</div>"
"</div>"
"<div style='text-align:center; margin-top:10px'>"
"<button onclick='atualizarLimites()'>Confirmar Limites</button>"
"</div>"
"</fieldset>"
"</div></div></body></html>";

// Estrutura que mantém o estado da conexão HTTP
struct http_state {
    char response[8192];  // Buffer da resposta HTTP
    size_t len;           // Tamanho total da resposta
    size_t sent;          // Quantidade de dados já enviados
};

// Callback chamado quando dados foram enviados com sucesso
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);  // Fecha conexão após envio completo
        free(hs);         // Libera memória alocada
    }
    return ERR_OK;
}

// Callback chamado quando uma requisição HTTP é recebida
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {  // Conexão fechada pelo cliente
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;  // Conteúdo da requisição
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    // Requisição para obter dados em JSON
    if (strstr(req, "GET /dados")) {
        char json[512];
        int json_len = snprintf(json, sizeof(json),
            "{\"temp_aht\":%.2f,\"umidade\":%.2f,\"altitude\":%.2f,\"offsetTemp\":%.2f,\"offsetUmi\":%.2f,\"offsetAlt\":%.2f,\"maxTemp\":%.2f,\"minTemp\":%.2f,\"maxUmi\":%.2f,\"minUmi\":%.2f,"
            "\"maxAlt\":%.2f,\"minAlt\":%.2f}",
            temperatura_aht, umidade, altitude, offsetTemp, offsetUmi, offsetAlt,limiteTemp.max, limiteTemp.min, limiteUmi.max, limiteUmi.min, limiteAlt.max, limiteAlt.min);

        // Monta resposta JSON
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
            json_len, json);

    // Requisição para alterar offsets
    } else if (strstr(req, "GET /set_offset")) {
        sscanf(strstr(req, "temp="), "temp=%f&umi=%f&alt=%f", &offsetTemp, &offsetUmi, &offsetAlt);
        hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");

    // Requisição para alterar limites
    } else if (strstr(req, "GET /set_limits")) {
        sscanf(strstr(req, "maxt="), "maxt=%f&mint=%f&maxu=%f&minu=%f&maxa=%f&mina=%f",
        &limiteTemp.max, &limiteTemp.min,
        &limiteUmi.max, &limiteUmi.min,
        &limiteAlt.max, &limiteAlt.min);
        limiteTemp.set = true;
        limiteUmi.set = true;
        limiteAlt.set = true;

        hs->len = snprintf(hs->response, sizeof(hs->response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");

    // Requisição padrão (HTML principal)
    } else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
            (int)strlen(HTML_BODY), HTML_BODY);
    }

    // Configura callbacks e envia resposta
    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

// Callback de nova conexão aceita
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);  // Define função para tratar dados recebidos
    return ERR_OK;
}

// Função principal para iniciar o servidor HTTP
void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();  // Cria novo bloco de controle TCP
    if (!pcb) {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);  // Coloca socket em modo escuta
    tcp_accept(pcb, connection_callback);  // Define função para aceitar conexões
    printf("Servidor HTTP rodando na porta 80...\n");
}
