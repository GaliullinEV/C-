//Измеритель тока/напряжения батареи с CAN-интерфейсом
//MCU: STM32F103C8T6, 
//CAN ID: 0x293, 500 кбит/с, период 100 мс
//Значения тока и напряжения имитируются, 
//CAN-пакет дублируется в UART для проверки

//Генерируем фейковые токи так как отсутствует CAN трансивер mcp2551

//Обработчик таймера(каждые 100 мс)
//Вместо реальных усреднений с АЦП
//подставляем синтезированные значения.
//Напряжение: 370В + небольшая синусоида для реализма.
//Ток: 50А при разряде, -30А при заряде (меняется по счётчику).

#include "main.h"
#include <string.h>
#include <stdio.h>

CAN_HandleTypeDef hcan;
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

//Данные АЦП
volatile uint16_t adc_raw_values[2]; 

//Счетчик и суммы для усреднения
volatile uint32_t adc_sum_current = 0;
volatile uint32_t adc_sum_voltage = 0;
volatile uint16_t adc_sample_count = 0;

//Итоговые значения
volatile float current_value = 0.0f;
volatile float voltage_value = 0.0f;

//Флаг для отправки сообщения
volatile uint8_t send_can_flag = 0; 
volatile uint8_t message_counter = 0; 

//Буфер для UART
char uart_buf[128];

//CRC-8
uint8_t calculate_crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    uint8_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc = (crc << 1);
        }
    }
    return crc;
}

//Обработчик АЦП
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        adc_sum_current += adc_raw_values[0];
        adc_sum_voltage += adc_raw_values[1];
        adc_sample_count++;
        // Перезапуск DMA
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw_values, 2);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        static int test_counter = 0;
        voltage_value = 370.0f + 5.0f * sin(test_counter * 0.1f);
        if (test_counter % 2 == 0) {
            current_value = 50.0f + (test_counter % 10);
        } else {
            current_value = -30.0f - (test_counter % 5);
        }
        test_counter++;

        //Сброс счётчиков АЦП
        adc_sum_current = 0;
        adc_sum_voltage = 0;
        adc_sample_count = 0;

        send_can_flag = 1; //Сигнал главному циклу
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_CAN_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    sprintf(uart_buf, "\r\n=== Battery Monitor Test ===\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw_values, 2);    //Запуск АЦП
    HAL_TIM_Base_Start_IT(&htim2);                              //Запуск таймера на 100 мс
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);      //Включаем светодиод PC13 как индикатор работы

    //Переменные CAN
    uint8_t can_data[8];
    uint32_t tx_mailbox;

    while (1) {
        if (send_can_flag) {
            send_can_flag = 0;
            message_counter++;

            //Формируем 8 байт сообщения 0x293
            int16_t current_int = (int16_t)(current_value * 10); // 0.1 A
            can_data[0] = (uint8_t)(current_int & 0xFF);
            can_data[1] = (uint8_t)((current_int >> 8) & 0xFF);

            uint16_t voltage_int = (uint16_t)(voltage_value * 10); // 0.1 V
            can_data[2] = (uint8_t)(voltage_int & 0xFF);
            can_data[3] = (uint8_t)((voltage_int >> 8) & 0xFF);

            int16_t power_int = (int16_t)((current_value * voltage_value / 1000.0f) * 10); // 0.1 kW
            can_data[4] = (uint8_t)(power_int & 0xFF);
            can_data[5] = (uint8_t)((power_int >> 8) & 0xFF);

            can_data[6] = 0x00;

            uint8_t crc = calculate_crc8(can_data, 7) & 0x0F;
            can_data[7] = ((message_counter & 0x0F) << 4) | crc;

            //Вывод в UART для отладки
            sprintf(uart_buf, 
                    "ID: 0x293 | I: %.1fA | V: %.1fV | P: %.2fkW | Status: 0x%02X | CRC: 0x%01X\r\n",
                    current_value, voltage_value, 
                    current_value * voltage_value / 1000.0f,
                    can_data[6], crc);
            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

            //Отправка в CAN
            CAN_TxHeaderTypeDef TxHeader;
            TxHeader.StdId = 0x293;
            TxHeader.ExtId = 0x00;
            TxHeader.IDE = CAN_ID_STD;
            TxHeader.RTR = CAN_RTR_DATA;
            TxHeader.DLC = 8;
            TxHeader.TransmitGlobalTime = DISABLE;

            // Если CAN-контроллер не может отправить (нет трансивера) сообщаем об ошибке передачи в UART
            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &tx_mailbox) != HAL_OK) {
                sprintf(uart_buf, "CAN TX Error! (No transceiver connected?)\r\n");
                HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
            }
        }
    }
}