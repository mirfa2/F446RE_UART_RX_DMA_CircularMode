/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

	//if we dont know how much total_data to receive (eg continuous data stream), or if the mcu doesnt have
	//enough sram to store the data (eg; receive video in Mbytes) and we should store the received data to
	//other external memory (like sdcard connected to the mcu), then we can receive in dma circular mode and
	//continuously write the data, in portions, to the external memory using halftransfer and completetransfer
	//callback methods, like in uart tx dma. we STILL expect to receive the data_length data like before

	//f466re has 512 kilobytes of non-volatile flash memory (we it stores compiled c codes, etc)
	//and 128 kilobytes of SRAM, volatile temporary working memory

uint8_t RxData[256];	//main buffer to temporarily store the received data in portions in the sram.
uint8_t externalMemory[4096];	//represents our external memory for this demo's sake

int isSizeReceived = 0;	//flag var to keep track if we have receive the data_length data
uint16_t dataLength=0;	//data_length data

int HTC = 0;	//half transfer complete flag var
int FTC = 0;	//full transfer complete flag var
uint16_t indx=0;	//index var to keep track on how much data is already transferred to the external memory
					//also a pointer to where we store the data in the external memory

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
	//first receive and store the data_length, then transfer the received data in the first half buffer to external memory
	if (isSizeReceived == 0)
	{
		dataLength = ((RxData[0]-48)*1000)+((RxData[1]-48)*100)+((RxData[2]-48)*10)+((RxData[3]-48));  //store data_length
		indx = 0;	//to store the data at the start of the external memory
		memcpy(externalMemory+indx, RxData+4, 124);  // copy the data from the mcu receive buffer into the external memory
		memset(RxData, '\0', 128);  // clear the first half of the receive buffer, half buffer size = 128 bytes
		indx += 124;  //update index. we transferred 124 bytes of the received data to external mem
		isSizeReceived = 1;  //on the next half transfer we should continue receiving and transfering the data packet
	}
	else	//continue receiving the data and transferring the data in first half buffer to external memory
	{
		memcpy(externalMemory+indx, RxData, 128);	//now we can copy the entire half buffer because now all bytes are data bytes
		memset(RxData, '\0', 128);
		indx += 128;
	}

	//set the transfer flags
	HTC=1;
	FTC=0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	  memcpy(externalMemory+indx, RxData+128, 128);	//transfer the 2nd half of the receive buffer
	  memset(RxData+128, '\0', 128);	//clear the 2nd half of the receive buffer
	  indx+=128;

	  HTC=0;
	  FTC=1;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  //dma rx in circularmode, receive data in chunks of 256 bytes
  HAL_UART_Receive_DMA(&huart2, RxData, 256);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

//since we define our receive buffer in 256 chunks, the HTC and FTC will only trigger when the total_data is a multiple of 256.
//we need to handle the case when total_data isnt a multiple of 256, meaning there are remaining bytes in the mcu receive buffer
//that we need to manually transfer to the external memory. "datalength-indx" equals 0 when all data in the packet is
//transferred, at the start of the transfer it is naturally >0, then at the end, it should be <128 because it is now the remaider
//of the transfer. we transfer data at 128 chunks from receive buffer to external memory

	  if (  ((dataLength-indx)>0) && ((dataLength-indx)<128)  )
	  {
		  //remaining bytes in the 2nd half of the buffer
		  if (HTC==1)
		  {
			  //strcpy((char *)externalMemory+indx, (char *)RxData+128);
			  memcpy(externalMemory+indx, RxData+128, (dataLength-indx));	//copy the remainder to external memory
			  indx = dataLength;	//transfer complete

			  //set up dma transfer for the next packet
			  isSizeReceived = 0;
			  HTC = 0;

			  //because dma in circular mode, we need to restart it first so that the next dma will store data starting at the
			  //start of the receive buffer, not continuing from the previous position
			  HAL_UART_DMAStop(&huart2);
			  HAL_UART_Receive_DMA(&huart2, RxData, 256);
		  }

		  //remaining bytes in the 1st half of the buffer
		  else if (FTC==1)
		  {
			  //strcpy((char *)externalMemory+indx, (char *)RxData);	because we cleared the buffer we can use strcpy() too
			  memcpy(externalMemory+indx, RxData, (dataLength-indx));
			  indx = dataLength;
			  isSizeReceived = 0;
			  FTC = 0;
			  HAL_UART_DMAStop(&huart2);
			  HAL_UART_Receive_DMA(&huart2, RxData, 256);
		  }
	  }

	  //reset the dma for the next transfer in case of total_data a multiple of 256
	  else if ((indx == dataLength) && ((HTC==1)||(FTC==1)))
	  {
		  isSizeReceived = 0;
		  HTC = 0;
		  FTC = 0;
		  HAL_UART_DMAStop(&huart2);
		  HAL_UART_Receive_DMA(&huart2, RxData, 256);
	  }

	  //blink LED every 500ms, cpu free cause dma
	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
	  HAL_Delay(500);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
