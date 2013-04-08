#include <stm32f4xx_conf.h>
#include <stm32f4xx.h>
#include <misc.h>
#include <stm32f4xx_usart.h>
#include <stm32f4xx_gpio.h>
#include <stm32f4xx_rcc.h>
#include <stm32f4xx_crc.h>

#define MAX_STRLEN 12 		 // this is the maximum string length of our string in characters
volatile char received_string[MAX_STRLEN + 1]; // this will hold the recieved string

void Delay(__IO uint32_t nCount) {
	while (nCount--) {
	}
}

void init_GPIO() {
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14
			| GPIO_Pin_15;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/**
 * This funcion initializes the USART1 peripheral
 *
 * Arguments: baudrate --> the baudrate at which the USART is
 * 						   supposed to operate
 */
void init_USART1(uint32_t baudrate) {

	GPIO_InitTypeDef GPIO_InitStruct; // this is for the GPIO pins used as TX and RX
	USART_InitTypeDef USART_InitStruct; // this is for the USART1 initilization
	NVIC_InitTypeDef NVIC_InitStructure; // this is used to configure the NVIC (nested vector interrupt controller)

	/**
	 * enable APB2 peripheral clock for USART1
	 * note that only USART1 and USART6 are connected to APB2
	 * the other USARTs are connected to APB1
	 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	/**
	 * enable the peripheral clock for the pins used by
	 * USART1, PB6 for TX and PB7 for RX
	 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	/**
	 * This sequence sets up the TX and RX pins
	 * so they work correctly with the USART1 peripheral
	 */
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // Pins 6 (TX) and 7 (RX) are used
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF; // the pins are configured as alternate function so the USART peripheral has access to them
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // this defines the IO speed and has nothing to do with the baudrate!
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; // this defines the output type as push pull mode (as opposed to open drain)
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP; // this activates the pullup resistors on the IO pins
	GPIO_Init(GPIOB, &GPIO_InitStruct); // now all the values are passed to the GPIO_Init() function which sets the GPIO registers

	/**
	 * The RX and TX pins are now connected to their AF
	 * so that the USART1 can take over control of the
	 * pins
	 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);

	/**
	 * Now the USART_InitStruct is used to define the
	 * properties of USART1
	 */
	USART_InitStruct.USART_BaudRate = baudrate; // the baudrate is set to the value we passed into this init function
	USART_InitStruct.USART_WordLength = USART_WordLength_8b; // we want the data frame size to be 8 bits (standard)
	USART_InitStruct.USART_StopBits = USART_StopBits_1; // we want 1 stop bit (standard)
	USART_InitStruct.USART_Parity = USART_Parity_No; // we don't want a parity bit (standard)
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // we don't want flow control (standard)
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; // we want to enable the transmitter and the receiver
	USART_Init(USART1, &USART_InitStruct); // again all the properties are passed to the USART_Init function which takes care of all the bit setting

	/**
	 * Here the USART1 receive interrupt is enabled
	 * and the interrupt controller is configured
	 * to jump to the USART1_IRQHandler() function
	 * if the USART1 receive interrupt occurs
	 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // enable the USART1 receive interrupt

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn; // we want to configure the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; // this sets the priority group of the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; // this sets the subpriority inside the group
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // the USART1 interrupts are globally enabled
	NVIC_Init(&NVIC_InitStructure); // the properties are passed to the NVIC_Init function which takes care of the low level stuff

	// finally this enables the complete USART1 peripheral
	USART_Cmd(USART1, ENABLE);
}

/**
 * This function is used to transmit a string of characters via
 * the USART specified in USARTx.
 *
 * It takes two arguments: USARTx --> can be any of the USARTs e.g. USART1, USART2 etc.
 * 						   (volatile) char *s is the string you want to send
 *
 * Note: The string has to be passed to the function as a pointer because
 * 		 the compiler doesn't know the 'string' data type. In standard
 * 		 C a string is just an array of characters
 *
 * Note 2: At the moment it takes a volatile char because the received_string variable
 * 		   declared as volatile char --> otherwise the compiler will spit out warnings
 * */
void USART_puts(USART_TypeDef* USARTx, volatile char *s) {

	while (*s) {
		//GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		// wait until data register is empty
		while (!(USARTx->SR & 0x00000040))
			;
		USART_SendData(USARTx, *s);
		*s++;
		//GPIO_ResetBits(GPIOD, GPIO_Pin_12);
	}
}

void USART_putByte(USART_TypeDef* USARTx, volatile u08_t dataToSend) {

	  /* Transmit Data */
	  USARTx->DR = (dataToSend & (uint16_t)0x01FF);
	// wait until data register is empty
	while (!(USARTx->SR & 0x00000040))
		;
}

int main(void) {

	SystemInit();
	init_GPIO();
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
	CRC_ResetDR();
	CRC_SetIDRegister(69);
	init_USART1(9600); // initialize USART1 @ 9600 baud
	mryg();
	USART_puts(USART1, "Init complete! Hello World!\r\n"); // just send a message to indicate that it works

	while (1) {
		/**
		 * You can do whatever you want in here
		 */
	}
}
/*
 char* toHEX(char* buff, int len, unsigned int x) {
 buff += len;
 *buff = 0;
 char tmp;
 while (x) {
 buff--;
 tmp = x % 16;
 x /= 16;
 if (tmp >= 10)
 tmp += 'A' - 10;
 else
 tmp += '0';
 *buff = tmp;
 }
 return buff;
 }

 u32 dupa(u32 data) {
 int i;
 u32 rev = 0;
 for (i = 0; i < 32; i++)
 rev |= ((data >> i) & 1) << (31 - i);
 return rev;
 }
 ;*/

/**
 * zakładam nast. ramkę: [0|1,2|3-5|6]
 * 0 - nagłówek
 * 1,2 - argument
 * 3-5 - dane
 * 6 - koniec ramki
 */

int strLen(char* str) {
	int len = 0;
	int i = 0;
	while (str[i]) {
		len++;
		i++;
	}
	return len;
}

/**
 * l - diody
 */
void respond(char* str) {
	if (strLen(str) > 7) {
		USART_puts(USART1, "\r\nZa dluga");
		return;
	}
	if (strLen(str) < 4) {
		USART_puts(USART1, "\r\nNiepelna ramka");
		return;
	}
	if (str[1] == '0' && str[2] == '0') //diody
			{
		actionLed(str);
		USART_puts(USART1, "\r\nSUCCESS!");
		return;
	} else if (str[1] == '0' && str[2] == '1') //echo
			{
		USART_puts(USART1, &str[3]);
	//	USART_puts(USART1, str[4]);
	//	USART_puts(USART1, str[5]);
	//	USART_puts(USART1, "\r\nSUCCESS!");
		return;
	} else if (str[1] == '1' && str[2] == '0') {
		USART_puts(USART1, "\r\nNieobslugiwany argument");
		return;
	} else if (str[1] == '1' && str[2] == '1') {
		USART_puts(USART1, "\r\nNieobslugiwany argument");
		return;
	} else
	{
		USART_puts(USART1, "\r\nNieznana ramka");
	}

}

void actionLed(char* str) {
	if (str[3]=='1')
		GPIO_SetBits(GPIOD, GPIO_Pin_13);
	else
		GPIO_ResetBits(GPIOD, GPIO_Pin_13);

	if (str[4]=='1')
		GPIO_SetBits(GPIOD, GPIO_Pin_14);
	else
		GPIO_ResetBits(GPIOD, GPIO_Pin_14);

	if (str[5]=='1')
		GPIO_SetBits(GPIOD, GPIO_Pin_15);
	else
		GPIO_ResetBits(GPIOD, GPIO_Pin_15);
}

void actionEcho(char* str) {
	char* tmp = &str[3];
	USART_puts(USART1, &tmp[1]);
//	USART_puts(USART1, &str[4]);
//	USART_puts(USART1, &str[5]);
/*	int i;
	char* tmp;
	for (i = 3; i <= 5; i++)
		tmp[i - 3] = str[i];
	tmp[3] = 0;
	USART_puts(USART1, tmp);*/
}

// this is the interrupt request handler (IRQ) for ALL USART1 interrupts
void USART1_IRQHandler(void) {
	// check if the USART1 receive interrupt flag was set
	if (USART_GetITStatus(USART1, USART_IT_RXNE)) {
		//crc// static unsigned int tmp = 0;
		static uint8_t cnt = 0; // this counter is used to determine the string length
		char t = USART1->DR; // the character from the USART1 data register is saved in t

		AnalyzeData(t);

		/**
		 * check if the received character is not the LF character (used to determine end of string)
		 * or the if the maximum string length has been been reached
		 */
		GPIO_SetBits(GPIOD, GPIO_Pin_12);
		if ((t != '\n') && (cnt < MAX_STRLEN)) {

			received_string[cnt] = t;
			//crc// tmp<<=8;
			//crc// tmp|=t;
			cnt++;
			//crc//if (!(cnt%4))
			//crc// CRC_CalcCRC(tmp);
		} else { // otherwise reset the character counter and print the received string

			//crc//if (cnt%4) {
			//crc//	tmp<<=8*(4-(cnt%4));
			//crc//	CRC_CalcCRC(tmp);
			//crc//}

			received_string[cnt] = 0;
			cnt = 0;
			if (strLen(received_string))
				respond(received_string);

			//USART_puts(USART1, "Odebralem: ");
			//USART_puts(USART1, received_string);
			//crc//USART_puts(USART1, " CRC: ");
			//crc//char buff[10];
			//crc//USART_puts(USART1,toHEX(buff,10,dupa(CRC_GetCRC()^0xFFFFFFFF)));
			//USART_puts(USART1,"\r\n");
			GPIO_ResetBits(GPIOD, GPIO_Pin_12);
			//crc//CRC_ResetDR();
		}
	}

}
void mryg() {
	int i;
	GPIO_SetBits(GPIOD, GPIO_Pin_12);
	for (i = 0; i < 1000000; i++)		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_12);

	GPIO_SetBits(GPIOD, GPIO_Pin_13);
	for (i = 0; i < 1000000; i++)		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_13);

	GPIO_SetBits(GPIOD, GPIO_Pin_14);
	for (i = 0; i < 1000000; i++)		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_14);

	GPIO_SetBits(GPIOD, GPIO_Pin_15);
	for (i = 0; i < 1000000; i++)		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_15);
	for (i = 0; i < 1000000; i++)		;
}
