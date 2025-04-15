#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "ssd1306.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define VRX_PIN 26
#define VRY_PIN 27
#define BTN_G 16
#define BTN_R 17

QueueHandle_t xQueueADC;

typedef struct {
    int id;
    int dados;
} adc_t;

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
        
        vTaskDelay(pdMS_TO_TICKS(10));
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
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void uart_task(void *parametros) {
    adc_t dados_recebidos;

    while (1) {
        if (xQueueReceive(xQueueADC, &dados_recebidos, portMAX_DELAY)) {
            uint8_t byte_1 = ((uint16_t)dados_recebidos.dados) >> 8 & 0xFF;
            uint8_t byte_0 = (uint8_t)dados_recebidos.dados & 0xFF;

            uart_putc_raw(uart_default, dados_recebidos.id);
            uart_putc_raw(uart_default, byte_0);
            uart_putc_raw(uart_default, byte_1);
            uart_putc_raw(uart_default, 0xFF);
        }

    }
}

int main(void) {
    inicializar_hardware();
    
    xQueueADC = xQueueCreate(64, sizeof(adc_t));
    
    xTaskCreate(x_task, "Tarefa Eixo X", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "Tarefa Eixo Y", 4095, NULL, 1, NULL);
    xTaskCreate(uart_task, "Tarefa UART", 4095, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    while (true);
}