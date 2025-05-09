#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <string.h> 
#include <stdio.h>

#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "gfx.h"

#define VRX_PIN 26
#define VRY_PIN 27
#define BTN_G 16
#define BTN_R 17
#define BTN_ESQ 14
#define BTN_DIR 15

#define BTN_INTERACAO 18
#define BTN_MISSAO 19
#define BTN_MACRO 22

#define LED_CONEXAO 13
#define BUZZER 12

QueueHandle_t xQueueADC;
QueueHandle_t xQueueBUZ;

typedef struct {
    int id;
    int dados;
} adc_t;

typedef struct {
    int notas[10];
    int duracao[10];
    int tamanho;
} sound_t;

static sound_t sounds[] = {
    // 0: Ligar - Melodia de Boas-Vindas
    {{392, 440, 523, 587, 0, 0, 0, 0, 0, 0}, {200, 200, 200, 300, 0, 0, 0, 0, 0, 0}, 4},
    // 1: Conectar - Sinfonia de Conexão Estelar
    {{523, 659, 784, 880, 784, 659, 880, 1046, 0, 0}, {150, 150, 150, 150, 150, 150, 200, 300, 0, 0}, 8},
    // 2: BTN_G - Impacto da Ferramenta
    {{330, 523, 0, 0, 0, 0, 0, 0, 0, 0}, {100, 100, 0, 0, 0, 0, 0, 0, 0, 0}, 2},
    // 3: BTN_INTERACAO - Seleção Rápida (Interação)
    {{784, 880, 0, 0, 0, 0, 0, 0, 0, 0}, {100, 100, 0, 0, 0, 0, 0, 0, 0, 0}, 2},
    // 4: BTN_MISSAO - Abertura de Menu
    {{440, 523, 659, 0, 0, 0, 0, 0, 0, 0}, {150, 150, 150, 0, 0, 0, 0, 0, 0, 0}, 3},
    // 5: BTN_R - Abertura de Menu (mesmo som de MISSÃO)
    {{440, 523, 659, 0, 0, 0, 0, 0, 0, 0}, {150, 150, 150, 0, 0, 0, 0, 0, 0, 0}, 3}
};

void buzzer_task(void *parametros) {
    sound_t sound;
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);

    while(1){
        if (xQueueReceive(xQueueBUZ, &sound, portMAX_DELAY)){
            for (int i = 0; i < sound.tamanho; i++) {
                int freq = sound.notas[i];
                int dur = sound.duracao[i];

                if (freq <= 0) {
                    pwm_set_enabled(slice_num, false);
                    vTaskDelay(pdMS_TO_TICKS(dur));
                    continue;
                }

                // Redefinindo a frequência do PWM
                uint32_t clock = 125000000;
                uint32_t divider16 = 16 * clock / freq / 65536 + 1;
                if (divider16 > 256 * 16) divider16 = 256 * 16;

                float divider = divider16 / 16.0f;
                uint32_t top = clock / freq / divider;

                // PWM emite a onda
                pwm_set_clkdiv(slice_num, divider);
                pwm_set_wrap(slice_num, top);
                pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER), top / 2);

                // Tocando o Buzzer
                pwm_set_enabled(slice_num, true);
                vTaskDelay(pdMS_TO_TICKS(dur));
                pwm_set_enabled(slice_num, false);
            }
        }
    }
}

void btn_callback(uint gpio, uint32_t events) {
    adc_t btn;

    // ids 0 e 1 -> joystick
    // 2345678e
    if (gpio == BTN_G){
        btn.id = 2;
    } else if (gpio == BTN_INTERACAO){
        btn.id = 3;
    } else if (gpio == BTN_MISSAO){
        btn.id = 4;
    } else if (gpio == BTN_R) {
        btn.id = 5;
    } else if (gpio == BTN_ESQ){
        btn.id = 6;
    } else if (gpio == BTN_DIR){
        btn.id = 7;
    } else if (gpio == BTN_MACRO){
        btn.id = 8;
    }

    if (events & 0x4) { 
        btn.dados = 1;
    } else if (events == 0x8) { 
        btn.dados = 0;
    }

    xQueueSendFromISR(xQueueADC, &btn, 0);
    
}

int media_movel(int *contagem_amostras, int array_dados[], int novo_valor) {
    // Se ? V : F 
    int divisor = (*contagem_amostras < 5) ? ++(*contagem_amostras) : 5;
    int indice = (*contagem_amostras - 1) % 5;
    
    array_dados[indice] = novo_valor;

    int soma = 0;
    for (int i = 0; i < divisor; i++) {
        soma += array_dados[i];
    }
    
    return (soma / divisor);
}

void inicializar_hardware(void) {
    stdio_init_all();

    uart_init(uart_default, 115200);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    gpio_init(BTN_G);
    gpio_set_dir(BTN_G, GPIO_IN);
    gpio_pull_up(BTN_G);
    gpio_set_irq_enabled_with_callback(BTN_G, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    gpio_init(BTN_R);
    gpio_set_dir(BTN_R, GPIO_IN);
    gpio_pull_up(BTN_R);
    gpio_set_irq_enabled(BTN_R, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_MISSAO);
    gpio_set_dir(BTN_MISSAO, GPIO_IN);
    gpio_pull_up(BTN_MISSAO);
    gpio_set_irq_enabled(BTN_MISSAO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_INTERACAO);
    gpio_set_dir(BTN_INTERACAO, GPIO_IN);
    gpio_pull_up(BTN_INTERACAO);
    gpio_set_irq_enabled(BTN_INTERACAO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_MACRO);
    gpio_set_dir(BTN_MACRO, GPIO_IN);
    gpio_pull_up(BTN_MACRO);
    gpio_set_irq_enabled(BTN_MACRO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_DIR);
    gpio_set_dir(BTN_DIR, GPIO_IN);
    gpio_pull_up(BTN_DIR);
    gpio_set_irq_enabled(BTN_DIR, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_ESQ);
    gpio_set_dir(BTN_ESQ, GPIO_IN);
    gpio_pull_up(BTN_ESQ);
    gpio_set_irq_enabled(BTN_ESQ, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(BTN_MACRO);
    gpio_set_dir(BTN_MACRO, GPIO_IN);
    gpio_pull_up(BTN_MACRO);
    gpio_set_irq_enabled(BTN_MACRO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    gpio_init(LED_CONEXAO);
    gpio_set_dir(LED_CONEXAO, GPIO_OUT);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);

    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), true);
}

void x_task(void *parametros) {
    int contagem_amostras = 0;
    int dados_x[5] = {0};
    adc_t leitura_x = {.id = 0};
    int mandou_zero_x = 0;

    while (1) {
        adc_select_input(0);
        double raw_x = (double)(adc_read() - 2047) * (255.0/2047.0);
        
        leitura_x.dados = media_movel(&contagem_amostras, dados_x, (int)raw_x);
        // Se ? V : ( Se ? V : F )
        leitura_x.dados = (leitura_x.dados < -255) ? -255 : ( (leitura_x.dados > 255) ? 255 : leitura_x.dados );

        if (leitura_x.dados > 30 || leitura_x.dados < -30) {
            xQueueSend(xQueueADC, &leitura_x, 1000);
            mandou_zero_x = 0;
        } else if(!mandou_zero_x){
            leitura_x.dados = 0;
            xQueueSend(xQueueADC, &leitura_x, 1000);
            mandou_zero_x = 1; 
        }
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void y_task(void *parametros) {
    int contagem_amostras = 0;
    int dados_y[5] = {0};
    adc_t leitura_y = {.id = 1};
    int mandou_zero_y = 0;

    while (1) {
        adc_select_input(1);
        double raw_y = (double)(adc_read() - 2047) * (255.0/2047.0);
        
        leitura_y.dados = media_movel(&contagem_amostras, dados_y, (int)raw_y);
        // Se ? V : ( Se ? V : F )
        leitura_y.dados = (leitura_y.dados < -255) ? -255 : ( (leitura_y.dados > 255) ? 255 : leitura_y.dados );

        if (leitura_y.dados > 30 || leitura_y.dados < -30) {
            xQueueSend(xQueueADC, &leitura_y, 1000);
            mandou_zero_y = 0;
        } else if (!mandou_zero_y){
            leitura_y.dados = 0;
            xQueueSend(xQueueADC, &leitura_y, 1000);
            mandou_zero_y = 1;
        }
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void uart_task(void *parametros) {
    adc_t dados_recebidos;

    while (1) {
        if (xQueueReceive(xQueueADC, &dados_recebidos, portMAX_DELAY)) {
            uint8_t byte_1 = ((uint16_t)dados_recebidos.dados) >> 8 & 0xFF;
            uint8_t byte_0 = (uint8_t)dados_recebidos.dados & 0xFF;

            uart_putc_raw(uart_default, 0xFF);
            uart_putc_raw(uart_default, dados_recebidos.id);
            uart_putc_raw(uart_default, byte_0);
            uart_putc_raw(uart_default, byte_1);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void talk_task(void *parametros) {
    int conectado = 0, batendo = 0;
    sound_t som;

    while (1) {
        char comando = getchar_timeout_us(10000);

        if (comando == 'c'){
            conectado = 1;
            som = sounds[1];
            xQueueSend(xQueueBUZ, &som, 1000);
        } else if (comando == 'e'){
            conectado = 0;
        } else if (comando == 'l'){
            batendo = 1;
        } else if (comando == 'u'){
            batendo = 0;
        }else if (comando == 'x'){
            som = sounds[3];
            xQueueSend(xQueueBUZ, &som, 1000);
        } else if (comando == 'f'){
            som = sounds[4];
            xQueueSend(xQueueBUZ, &som, 1000);
        } else if (comando == 'i'){
            som = sounds[5];
            xQueueSend(xQueueBUZ, &som, 1000);
        }

        if (batendo == 1){
            som = sounds[2];
            xQueueSend(xQueueBUZ, &som, 1000);
        }
        
        gpio_put(LED_CONEXAO, conectado);

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

int main(void) {

    inicializar_hardware();
    
    xQueueADC = xQueueCreate(8, sizeof(adc_t));
    xQueueBUZ = xQueueCreate(8, sizeof(sound_t));
    
    xTaskCreate(x_task, "Tarefa Eixo X", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "Tarefa Eixo Y", 4095, NULL, 1, NULL);
    xTaskCreate(uart_task, "Tarefa UART", 4095, NULL, 1, NULL);
    xTaskCreate(talk_task, "Tarefa Talk", 4095, NULL, 1, NULL);
    xTaskCreate(buzzer_task, "Tarefa Buzzer", 4095, NULL, 1, NULL);

    sound_t som = sounds[0];
    xQueueSend(xQueueBUZ, &som, 1000);
    
    vTaskStartScheduler();
    
    while (true);
}
