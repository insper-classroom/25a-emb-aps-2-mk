#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <string.h> 
#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/irq.h"

#define VRX_PIN 26
#define VRY_PIN 27
#define BTN_G 16
#define BTN_R 17
#define BTN_ESQ 14
#define BTN_DIR 15

#define BTN_INTERACAO 1
#define BTN_MISSAO 1
#define BTN_MACRO 1

#define LED_CONEXAO 13

QueueHandle_t xQueueADC;
QueueHandle_t xQueueBTN;
volatile bool timer_fired = false;

typedef struct {
    int id;
    int dados;
} adc_t;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    timer_fired = true;

    return 0;
}

void btn_callback(uint gpio, uint32_t events) {
    adc_t btn;

    // ids 0 e 1 -> joystick
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

    gpio_init(LED_CONEXAO);
    gpio_set_dir(LED_CONEXAO, GPIO_OUT);
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

void btn_task (void *parametros){
    adc_t btn_recebido;
    alarm_id_t alarm;
    int alarme_adicionado = 0;

    while(1){
        if (xQueueReceive(xQueueBTN, &btn_recebido, 10) && !alarme_adicionado) {
            if (btn_recebido.dados == 1){
                xQueueSend(xQueueADC, &btn_recebido, 10);
                vTaskDelay(pdMS_TO_TICKS(50));
                btn_recebido.dados = 0;
                xQueueSend(xQueueADC, &btn_recebido, 10);
                alarme_adicionado = 1;
                alarm = add_alarm_in_ms(750, alarm_callback, NULL, false);
            }
        }

        if (timer_fired){
            alarme_adicionado = 0;
            timer_fired = false;
            cancel_alarm(alarm);
        }

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void led_task(void *parametros) {
    int conectado = 0;
    char comando;
    
    while (1) {
        comando = getchar_timeout_us(10000);
        if (comando == 'c'){
            conectado = 1;
        } else if (comando == 'e'){
            conectado = 0;
        }

        gpio_put(LED_CONEXAO, conectado);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main(void) {

    inicializar_hardware();
    
    xQueueADC = xQueueCreate(8, sizeof(adc_t));
    xQueueBTN = xQueueCreate(8, sizeof(adc_t));
    
    xTaskCreate(x_task, "Tarefa Eixo X", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "Tarefa Eixo Y", 4095, NULL, 1, NULL);
    xTaskCreate(uart_task, "Tarefa UART", 4095, NULL, 1, NULL);
    xTaskCreate(btn_task, "Tarefa BTN", 4095, NULL, 1, NULL);
    xTaskCreate(led_task, "Tarefa LED Conexao", 4095, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    while (true);
}
