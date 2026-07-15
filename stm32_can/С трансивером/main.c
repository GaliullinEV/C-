/**
  * @file 
  * @brief
  * @attention
  */
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
//Таймера (каждые 100 мс)
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

    sprintf(uart_buf, "\r\n=== Battery Monitor Test ===\r\n");
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
                    "ID: 0x293 | I: %.1fA | V: %.1fV | P: %.2fkW | Status: 0x%02X | CRC: 0x%01X\r\n",
                    current_value, voltage_value,
                    current_value * voltage_value / 1000.0f,
                    can_data[6], crc);
            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

            CAN_TxHeaderTypeDef TxHeader;
            TxHeader.StdId = 0x293;
            TxHeader.ExtId = 0x00;
            TxHeader.IDE = CAN_ID_STD;
            TxHeader.RTR = CAN_RTR_DATA;
            TxHeader.DLC = 8;
            TxHeader.TransmitGlobalTime = DISABLE;

            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, can_data, &tx_mailbox) != HAL_OK) {
                sprintf(uart_buf, "CAN TX Error!\r\n");
                HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
            }
        }
    }
}

/**
  * @brief
  * @retval
  */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief
  * @param None
  * @retval
  */
void MX_CAN_Init(void) {
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 16;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief
  * @param None
  * @retval
  */
void MX_TIM2_Init(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    Error_Handler();
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
    Error_Handler();
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    Error_Handler();
}

/**
  * @brief
  * @param None
  * @retval
  */
void MX_USART1_UART_Init(void) {
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
    Error_Handler();
}

/**
  * @brief
  * @param None
  * @retval
  */
void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
/**
  * @brief
  * @retval
  */
void Error_Handler(void) {
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  
  * @param  file
  * @param  line
  * @retval
  */
void assert_failed(uint8_t *file, uint32_t line) {}
#endif