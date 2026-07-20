#include "main.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_CAN_Init(void);
void MX_TIM2_Init(void);
void MX_USART1_UART_Init(void);

CAN_HandleTypeDef hcan;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

volatile float current_value = 0.0f;
volatile float voltage_value = 0.0f;
volatile uint8_t send_can_flag = 0;
volatile uint8_t message_counter = 0;
char uart_buf[128];

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
        send_can_flag = 1;
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_CAN_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    sprintf(uart_buf, "\r\n=== Battery Monitor CAN v1.0 ===\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
    sprintf(uart_buf, "CAN Transceiver: MCP2551 detected\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

    HAL_TIM_Base_Start_IT(&htim2);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    uint8_t can_data[8];
    uint32_t tx_mailbox;

    while (1) {
        if (send_can_flag) {
            send_can_flag = 0;
            message_counter++;

            int16_t current_int = (int16_t)(current_value * 10);
            can_data[0] = (uint8_t)(current_int & 0xFF);
            can_data[1] = (uint8_t)((current_int >> 8) & 0xFF);

            uint16_t voltage_int = (uint16_t)(voltage_value * 10);
            can_data[2] = (uint8_t)(voltage_int & 0xFF);
            can_data[3] = (uint8_t)((voltage_int >> 8) & 0xFF);

            int16_t power_int = (int16_t)((current_value * voltage_value / 1000.0f) * 10);
            can_data[4] = (uint8_t)(power_int & 0xFF);
            can_data[5] = (uint8_t)((power_int >> 8) & 0xFF);

            can_data[6] = 0x00;

            uint8_t crc = calculate_crc8(can_data, 7) & 0x0F;
            can_data[7] = ((message_counter & 0x0F) << 4) | crc;

            sprintf(uart_buf,
                    "CAN> ID:0x293 I:%+.1fA V:%.1fV P:%+.2fkW CRC:0x%01X\r\n",
                    current_value, voltage_value,
                    current_value * voltage_value / 1000.0f, crc);
            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

            CAN_TxHeaderTypeDef TxHeader;
            TxHeader.StdId = 0x293;
            TxHeader.ExtId = 0x00;
            TxHeader.IDE = CAN_ID_STD;
            TxHeader.RTR = CAN_RTR_DATA;
            TxHeader.DLC = 8;
            TxHeader.TransmitGlobalTime = DISABLE;

            HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &tx_mailbox);
        }
    }
}