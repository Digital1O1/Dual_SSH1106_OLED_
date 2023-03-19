/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
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
#include <sys/_stdint.h>
#include "main.h"
#include "Font.h"
#include <string.h>
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"

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
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define OLED_I2C_PORT        hi2c1
#define OLED_BUFFER_SIZE   130 * 64 / 8
#define X_OFFSET_LOWER 0
#define X_OFFSET_UPPER 0

// Placing the buffers here require duplicates of the same functions to get the needed responses
//static uint8_t SSD1306_Buffer[OLED_BUFFER_SIZE];
//static uint8_t SSD1306_Buffer2[OLED_BUFFER_SIZE];

#define OLED_WIDTH           130
#define OLED_HEIGHT          64
//#define OLED1_Address 				  0x78 //0x3C << 1 THIS WAS WRONG
#define INCLUDE_FONT_6x8
#define INCLUDE_FONT_7x10
#define INCLUDE_FONT_11x18
#define INCLUDE_FONT_16x26
// Enumeration for screen colors
typedef enum
{
	Black = 0x00, // Black color, no pixel
	White = 0x01  // Pixel is set. Color depends on OLED
} SSD1306_COLOR;

typedef struct
{
		uint8_t FontWidth; /*!< Font width in pixels */
		uint8_t FontHeight; /*!< Font height in pixels */
		const uint16_t *data; /*!< Pointer to data font data array */
} FontDef;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

extern "C" PUTCHAR_PROTOTYPE
{
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
	return ch;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t oled_WriteCommand_status, oled_WriteData_status, oled_status1,
		oled_status2 = 0;
FontDef Font_7x10 = { 7, 10, Font7x10 };

class OLED
{
		// NOTE : All variables accessible to OLED class will be prefixed with an underscore
		//				Example : _I2C_address --> Class variable
		//									I2C_address  --> Regular variable

	private:
		unsigned char _I2C_address; // Should be accessible to entire OLED class
																// Note, this variable is PRIVATE
		uint16_t CurrentX = 0;
		uint16_t CurrentY = 0;
		uint8_t Initialized = 0;
		unsigned char current_Color = 0;
		uint8_t SSD1306_Buffer[OLED_BUFFER_SIZE];

	public:
		void set_I2C_Address(unsigned char I2C_address)
		{
			_I2C_address = I2C_address << 1;
		}
		unsigned char get_I2C_Address()
		{
			return _I2C_address;
		}

		void oled_WriteCommand(uint8_t incoming_byte)
		{
//			printf("I2C address being used : 0x%.2X || command byte : 0x%.2X \r\n",
//					_I2C_address, incoming_byte);
			// 0x3c --> 0x78
			// 0x3d --> 0x7A

			oled_WriteCommand_status = HAL_I2C_Mem_Write(&OLED_I2C_PORT, _I2C_address,
					0x00, 1, &incoming_byte, 1,
					HAL_MAX_DELAY);

			//printf("oled_WriteCommand_status : %d \r\n", oled_WriteCommand_status);

			if (oled_WriteCommand_status != HAL_OK)
			{
				printf("ERROR TRANSMITTING I2C COMMAND : 0x%.2X  FROM 0x%.2X\r\n",
						incoming_byte, _I2C_address);
				HAL_GPIO_WritePin(GPIOA, LED_Pin, GPIO_PIN_RESET);
			}
		}

		// Send data
		void oled_WriteData(uint8_t *buffer, size_t buff_size)
		{
//			printf("I2C Address : 0x%.2X | Writing : %u | With size : 0x%d \r\n",
//					_I2C_address, buffer, buff_size);
			HAL_I2C_Mem_Write(&hi2c1, _I2C_address, 0x40, 1, buffer, buff_size,
			HAL_MAX_DELAY);

			//printf("oled_WriteData_status : %d \r\n", oled_WriteData_status);

			if (oled_WriteData_status != HAL_OK)
			{
				printf("ERROR WRITING BUFFER : %hhn FOR SIZE : %d\r\n", (buffer),
						buff_size);
				HAL_GPIO_WritePin(GPIOA, LED_Pin, GPIO_PIN_RESET);
			}
		}

		void drawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
		{
			if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
			{
				// Don't write outside the buffer
				return;
			}

			// Draw in the right color
			if (color == White)
			{
				SSD1306_Buffer[x + (y / 8) * OLED_WIDTH] |= 1 << (y % 8);

			}
			else
			{
				SSD1306_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));

			}
		}

		char writeChar(char ch, FontDef Font, SSD1306_COLOR color)
		{
			uint32_t i, b, j;
			// Check if character is valid
			if (ch < 32 || ch > 126)
				return 0;

			// Check remaining space on current line
			if (OLED_WIDTH < (CurrentX + Font.FontWidth) ||
			OLED_HEIGHT < (CurrentY + Font.FontHeight))
			{
				// Not enough space on current line
				return 0;
			}

			// This is being skipped since Font Width and Height == 0
			// Use the font to write
			for (i = 0; i < Font.FontHeight; i++)
			{
				b = Font.data[(ch - 32) * Font.FontHeight + i];
				for (j = 0; j < Font.FontWidth; j++)
				{
					if ((b << j) & 0x8000)
					{
						drawPixel(CurrentX + j, (CurrentY + i), (SSD1306_COLOR) color);
					}
					else
					{
						drawPixel(CurrentX + j, (CurrentY + i), (SSD1306_COLOR) !color);
					}
				}
			}

			// The current space is now taken
			CurrentX += Font.FontWidth;

			// Return written char for validation
			return ch;
		}

		char writeString(char *str, FontDef Font, SSD1306_COLOR color)
		{
			// Write until null-byte
			while (*str)
			{
				HAL_GPIO_WritePin(GPIOA, LED_Pin, GPIO_PIN_RESET);
				if (writeChar(*str, Font, color) != *str)
				{
					// Char could not be written
					printf("CHAR COULDN'T BE WRITTEN \r\n");
					return *str;
				}
				str++;
				HAL_GPIO_WritePin(GPIOA, LED_Pin, GPIO_PIN_SET);
			}
			return *str;
		}

		void SetCursor(uint8_t x, uint8_t y)
		{
			CurrentX = x;
			CurrentY = y;
		}

		void fillScreen(SSD1306_COLOR color)
		{
			//Set current color
			set_Color(color);

			/* Set memory */
			uint32_t i;

			for (i = 0; i < sizeof(SSD1306_Buffer); i++)
			{
				SSD1306_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
			}
		}
		void updateScreen()
		{
			// Write data to each page of RAM. Number of pages
			// depends on the screen height:
			//
			//  * 32px   ==  4 pages
			//  * 64px   ==  8 pages
			//  * 128px  ==  16 pages
			for (uint8_t i = 0; i < OLED_HEIGHT / 8; i++)
			{
				// Set the current RAM page address.
				oled_WriteCommand(0xB0 + i);

				// Set column address 4 lower bits
				oled_WriteCommand(0x00 + X_OFFSET_LOWER);

				// Set column address 4 higher bits
				oled_WriteCommand(0x10 + X_OFFSET_UPPER);

				oled_WriteData(&SSD1306_Buffer[OLED_WIDTH * i], OLED_WIDTH);
				//printf("Buffer1 : %c \r\n", SSD1306_Buffer[i]);
			}
		}

		void set_Color(SSD1306_COLOR color)
		{
			current_Color = color;
		}
		void clearScreen()
		{
			for (uint32_t i = 0; i < sizeof(SSD1306_Buffer); i++)
			{
				SSD1306_Buffer[i] = (current_Color == Black) ? 0x00 : 0xFF;
			}
			updateScreen();

		}

};
// End of 'class OLED'

void init_OLED_Screens(OLED &oled1, OLED &oled2)
{
	// Set display OFF
	oled1.oled_WriteCommand(0xAE);
	oled2.oled_WriteCommand(0xAE);

	// Set Memory Addressing Mode
	oled1.oled_WriteCommand(0x20);
	oled2.oled_WriteCommand(0x20);

	// 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
	// 10b,Page Addressing Mode (RESET); 11b,Invalid
	oled1.oled_WriteCommand(0x00);
	oled2.oled_WriteCommand(0x00);

	// Set Page Start Address for Page Addressing Mode,0-7
	oled1.oled_WriteCommand(0xB0);
	oled2.oled_WriteCommand(0xB0);

	// Set COM Output Scan Direction
	oled1.oled_WriteCommand(0xC8);
	oled2.oled_WriteCommand(0xC8);

	//---set low column address
	oled1.oled_WriteCommand(0x00);
	oled2.oled_WriteCommand(0x00);

	//---set high column address
	oled1.oled_WriteCommand(0x10);
	oled2.oled_WriteCommand(0x10);

	//--set start line address - CHECK
	oled1.oled_WriteCommand(0x40);
	oled2.oled_WriteCommand(0x40);

//	const uint8_t kSetContrastControlRegister = 0x81;
//	oled1.oled_WriteCommand(kSetContrastControlRegister);
//	oled2.oled_WriteCommand(kSetContrastControlRegister);

	//const uint16_t = 0xFF;
	oled1.oled_WriteCommand(0xFF);
	oled2.oled_WriteCommand(0xFF);

	//--set segment re-map 0 to 127 - CHECK
	oled1.oled_WriteCommand(0xA1);
	oled2.oled_WriteCommand(0xA1);

	//--set normal color
	oled1.oled_WriteCommand(0xA6);
	oled2.oled_WriteCommand(0xA6);

	//ssd1306_WriteCommand(0xA8); //--set multiplex ratio(1 to 64) - CHECK
	//--set multiplex ratio(1 to 64) - CHECK
	oled1.oled_WriteCommand(0xA8);
	oled2.oled_WriteCommand(0xA8);

	// Set multiplex ratio. 128 x 64
//	oled1.oled_WriteCommand(0xFF);
//	oled2.oled_WriteCommand(0xFF);
	oled1.oled_WriteCommand(0x3F);
	oled2.oled_WriteCommand(0x3F);

	//0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	oled1.oled_WriteCommand(0xA4);
	oled2.oled_WriteCommand(0xA4);

	//-set display offset - CHECK
	oled1.oled_WriteCommand(0xD3);
	oled2.oled_WriteCommand(0xD3);

	//-not offset
	oled1.oled_WriteCommand(0x00);
	oled2.oled_WriteCommand(0x00);

	//--set display clock divide ratio/oscillator frequency
	oled1.oled_WriteCommand(0xD5);
	oled2.oled_WriteCommand(0xD5);

	//--set divide ratio
	oled1.oled_WriteCommand(0xF0);
	oled2.oled_WriteCommand(0xF0);

	//--set pre-charge period
	oled1.oled_WriteCommand(0xD9);
	oled2.oled_WriteCommand(0xD9);
	oled1.oled_WriteCommand(0x22);
	oled2.oled_WriteCommand(0x22);

	//--set com pins hardware configuration - CHECK
	oled1.oled_WriteCommand(0xDA);
	oled2.oled_WriteCommand(0xDA);
	oled1.oled_WriteCommand(0x12);
	oled2.oled_WriteCommand(0x12);

	//--set vcomh
	oled1.oled_WriteCommand(0xDB);
	oled2.oled_WriteCommand(0xDB);

	//0x20,0.77xVcc
	oled1.oled_WriteCommand(0x20);
	oled2.oled_WriteCommand(0x20);

	//--set DC-DC enable
	oled1.oled_WriteCommand(0x8D);
	oled2.oled_WriteCommand(0x8D);
	//
	oled1.oled_WriteCommand(0x14);
	oled2.oled_WriteCommand(0x14);

	//--turn on SSD1306 panel
	oled1.oled_WriteCommand(0xAF);
	oled2.oled_WriteCommand(0xAF);
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
	MX_USART2_UART_Init();
	MX_I2C1_Init();

	/* USER CODE BEGIN 2 */
	OLED oled1;
	OLED oled2;

	// Set I2C addresses
	oled1.set_I2C_Address(0x3C);
	oled2.set_I2C_Address(0x3D);

	oled_status1 = HAL_I2C_IsDeviceReady(&hi2c1, oled1.get_I2C_Address(), 3, 5);
	oled_status2 = HAL_I2C_IsDeviceReady(&hi2c1, oled2.get_I2C_Address(), 3, 5);

	if (oled_status1 != HAL_OK && oled_status2 != HAL_OK)
	{
		// If error persists after uploading correct I2C address
		// Power cycle the stm32 board
		printf("ERROR WITH I2C CONNECTION \r\n");
		printf("IF THE PROBLEM IS PERSISTANT, POWER CYCLE YOUR DEVICE\r\n");

		Error_Handler();
	}

	HAL_GPIO_WritePin(GPIOA, LED_Pin, GPIO_PIN_SET);
	printf("PROGRAM STARTED\r\n");

	init_OLED_Screens(oled1, oled2);

	oled1.fillScreen(White);
	oled2.fillScreen(Black);

//	oled1.SetCursor(5, 5);
//	oled2.SetCursor(5, 30);
//
//	char text1[] = "Screen 1";
//	char text2[] = "Screen 2";
//
//	oled1.writeString(text1, Font_7x10, Black);
//	oled2.writeString(text2, Font_7x10, White);
//
//	oled1.updateScreen();
//	oled2.updateScreen();
//
//	oled1.clearScreen();
//	oled2.clearScreen();
//
//	oled1.SetCursor(1, 1);
//	oled2.SetCursor(1, 5);
//	HAL_Delay(1000);
//
//	char text1_[] = "HELLO WORLD 1";
//	char text2_[] = "HELLO WORLD 2";
//
//	oled1.writeString(text1_, Font_7x10, Black);
//	oled2.writeString(text2_, Font_7x10, White);
//
//	oled1.updateScreen();
//	oled2.updateScreen();

	oled1.SetCursor(30, 1);
	oled2.SetCursor(30, 1);

	char text1_[] = "TESTING OLED";
	char text2_[] = "TESTING OLED";

	oled1.writeString(text1_, Font_7x10, Black);
	oled2.writeString(text2_, Font_7x10, White);

	oled1.SetCursor(30, 30);
	oled2.SetCursor(30, 30);

	char text1__[] = "SCREEN";
	char text2__[] = "SCREEN";

	oled1.writeString(text1__, Font_7x10, Black);
	oled2.writeString(text2__, Font_7x10, White);

	oled1.updateScreen();
	oled2.updateScreen();

	oled1.SetCursor(30, 50);
	oled2.SetCursor(30, 50);

	int count_1 = 1;
	char count_1_array[100];
	int count_2 = 13;
	char count_2_array[100];
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		oled1.SetCursor(30, 50);

		// int snprintf ( char * s, size_t n, const char * format, ... );
		// Parameters [ where it's stored , number of bytes , format , variable to be stored ]
		count_1++;

		// snprintf() needs to be used for displaying integer values since writeString() takes a char pointer
		// More details about arrays vs char pointers can be found here : https://www.youtube.com/watch?v=Qp3WatLL_Hc&ab_channel=PortfolioCourses
		snprintf(count_1_array, 100, "%d", count_1);
		oled1.writeString(count_1_array, Font_7x10, Black);
		oled1.updateScreen();

		oled2.SetCursor(30, 50);
		count_2 += 5;
		snprintf(count_2_array, 100, "%d", count_2);
		oled2.writeString(count_2_array, Font_7x10, White);
		oled2.updateScreen();
		HAL_Delay(1);
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

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
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : LED_Pin */
	GPIO_InitStruct.Pin = LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

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

#ifdef  USE_FULL_ASSERT
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
