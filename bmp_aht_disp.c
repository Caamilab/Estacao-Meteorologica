#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"      // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  
#include "lwip/pbuf.h"            // Manipulação de buffers de pacotes de rede (LWIP)
#include "lwip/tcp.h"             // Funções e estruturas para protocolo TCP (LWIP)
#include "lwip/netif.h"           // Interfaces de rede (LWIP)

#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include "globals.h"
#include "server.h"

// --------------------------- DEFINES E CONFIGURAÇÕES ---------------------------
#define BUZZER_PIN       21       // GPIO para o buzzer
#define LED_PIN          CYW43_WL_GPIO_LED_PIN  // LED do chip CYW43

// Credenciais Wi-Fi
#define WIFI_SSID        "Caverna 2.4G"
#define WIFI_PASS        "Oddilson"

// LEDs de alerta
#define LED_BLUE_PIN     12       // GPIO12 - LED azul
#define LED_GREEN_PIN    11       // GPIO11 - LED verde
#define LED_RED_PIN      13       // GPIO13 - LED vermelho

// Configuração I2C para sensores
#define I2C_PORT         i2c0     // i2c0 (pinos 0 e 1) ou i2c1 (pinos 2 e 3)
#define I2C_SDA          0        // Pino SDA
#define I2C_SCL          1        // Pino SCL
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa

// Configuração I2C para display
#define I2C_PORT_DISP    i2c1
#define I2C_SDA_DISP     14
#define I2C_SCL_DISP     15
#define DISPLAY_ADDR     0x3C     // Endereço I2C do display SSD1306

// Botão para entrar em modo BOOTSEL
#define BOTAO_B          6

// --------------------------- VARIÁVEIS GLOBAIS ---------------------------
float offsetTemp = 0.0f;
float offsetUmi  = 0.0f;
float offsetAlt  = 0.0f;

float temperatura_aht = 0.0f;
float umidade         = 0.0f;
float altitude        = 0.0f;

// Estruturas de limites configuráveis
Limite limiteTemp = {0.0f, 0.0f, false};
Limite limiteUmi  = {0.0f, 0.0f, false};
Limite limiteAlt  = {0.0f, 0.0f, false};

// --------------------------- FUNÇÕES AUXILIARES ---------------------------

// Calcula a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure) {
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// Interrupção para entrar em modo BOOTSEL (reboot para UF2)
#include "pico/bootrom.h"
void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}

// --------------------------- FUNÇÃO PRINCIPAL ---------------------------
int main() {
    // ------------------ Configuração do botão BOOTSEL ------------------
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // ------------------ Configuração dos LEDs ------------------
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    stdio_init_all();

    // ------------------ Configuração do Display SSD1306 ------------------
    i2c_init(I2C_PORT_DISP, 400 * 1000);   // I2C do display em 400 kHz
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, DISPLAY_ADDR, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false); // Limpa display
    ssd1306_send_data(&ssd);

    // ------------------ Configuração do I2C para sensores ------------------
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // ------------------ Configuração do Buzzer (PWM) ------------------
    gpio_init(BUZZER_PIN);
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_wrap(slice, 12500);
    pwm_set_chan_level(slice, PWM_CHAN_A, 0);
    pwm_set_enabled(slice, true);

    // ------------------ Inicialização dos Sensores ------------------
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_tmp1[5], str_alt[5], str_tmp2[5], str_umi[5];
    bool cor = true;

    // ------------------ Inicialização do Wi-Fi ------------------
    while (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_enable_sta_mode(); // Modo Station
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    start_http_server();

    // ------------------ LOOP PRINCIPAL ------------------
    while (1) {
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
        altitude = calculate_altitude(pressure) + offsetAlt;

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data)) {
            temperatura_aht = data.temperature + offsetTemp;
            umidade = data.humidity + offsetUmi;
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
        } else {
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        // Verificações de limites e alarmes
        if (limiteTemp.set && (temperatura_aht < limiteTemp.min || temperatura_aht > limiteTemp.max)) {
            gpio_put(LED_RED_PIN, 1);
            pwm_set_gpio_level(BUZZER_PIN, 7812);
            sleep_ms(200);
            gpio_put(LED_RED_PIN, 0);
            pwm_set_gpio_level(BUZZER_PIN, 0);
        }

        if (limiteUmi.set && (umidade < limiteUmi.min || umidade > limiteUmi.max)) {
            gpio_put(LED_BLUE_PIN, 1);
            pwm_set_gpio_level(BUZZER_PIN, 7812);
            sleep_ms(200);
            gpio_put(LED_BLUE_PIN, 0);
            pwm_set_gpio_level(BUZZER_PIN, 0);
        }

        if (limiteAlt.set && (altitude < limiteAlt.min || altitude > limiteAlt.max)) {
            gpio_put(LED_GREEN_PIN, 1);
            pwm_set_gpio_level(BUZZER_PIN, 7812);
            sleep_ms(200);
            gpio_put(LED_GREEN_PIN, 0);
            pwm_set_gpio_level(BUZZER_PIN, 0);
        }

        // Atualização do Display
        sprintf(str_tmp1, "%.1fC", temperature / 100.0);
        sprintf(str_alt, "%.0fm", altitude);
        sprintf(str_tmp2, "%.1fC", temperatura_aht);
        sprintf(str_umi, "%.1f%%", umidade);

        ssd1306_fill(&ssd, !cor);
        ssd1306_line(&ssd, 3, 5, 123, 5, cor);        // Linha superior
        ssd1306_line(&ssd, 3, 17, 123, 17, cor);      // Linha inferior
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 8);
        ssd1306_line(&ssd, 63, 5, 63, 43, cor);       // Linha vertical
        ssd1306_draw_string(&ssd, str_alt, 14, 20);   // Altitude
        ssd1306_draw_string(&ssd, str_tmp2, 73, 20);  // Temperatura
        ssd1306_draw_string(&ssd, str_umi, 73, 31);   // Umidade
        ssd1306_send_data(&ssd);

        // Manutenção do Wi-Fi e delay
        sleep_ms(500);
        cyw43_arch_poll(); // Mantém Wi-Fi ativo
        sleep_ms(100);
    }

    // Desliga a arquitetura CYW43
    cyw43_arch_deinit();
    return 0;
}
