#include <stm32f4xx_conf.h>
#include <stm32f4xx.h>
#include <misc.h>
#include <stm32f4xx_usart.h>
#include <stm32f4xx_gpio.h>
#include <stm32f4xx_rcc.h>
#include <stm32f4xx_crc.h>
#include <stdlib.h>

//#define MAX_STRLEN 12 		 // this is the maximum string length of our string in characters
typedef struct stp_data stp_data;
volatile char received_string[4];
volatile char data[65536];
volatile int CURRENT_LENGTH;
volatile int CURRENT_ARG;
volatile int READY_FLAG = 0;

void Delay(__IO uint32_t nCount) {
	while (nCount--) {
	}
}

struct stp_data{
	char f_data[65536];
	int arg;
};

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

void USART_putByte(USART_TypeDef* USARTx, volatile uint8_t ch) {

	// wait until data register is empty
	while (!(USARTx->SR & 0x00000040))
		;
	/* Transmit Data */
	USARTx->DR = (ch & (uint16_t) 0x01FF);
	// USART_SendData(USART1, (uint8_t) ch);

}

/**
 * Frame:
 * 0xAA | 2B - lenght | 1B - arg | 0 - 65536B | CRC/XOR | 0x55
 */
void STP_send(uint8_t arg, char* data) {

	int i = 0;
	uint8_t start = 170; //0xAA
	uint16_t length = strLen(data);
	uint8_t end = 85; //0x55

	int tmpOut = (((arg << 8) + length) << 16) + start;

	USART_putByte(USART1, start & (uint16_t) 0x01FF);
	USART_putByte(USART1, (length & 0xff) & (uint16_t) 0x01FF);
	USART_putByte(USART1, (length >> 8) & (uint16_t) 0x01FF);
	USART_putByte(USART1, arg & (uint16_t) 0x01FF);
	for (i = 0; i < 1000; i++)
			;
	USART_puts(USART1, data);
	USART_putByte(USART1, end & (uint16_t) 0x01FF);
}

stp_data STPRead(){
	if (strLen(data) > 0 && READY_FLAG) {
		stp_data d = { .f_data = data, .arg = CURRENT_ARG };
		return d;
	} else {
		stp_data d = { .f_data = "", .arg = 0 };
		return d;
	}
}

int STPStatus(){
	return READY_FLAG;
}

int main(void) {

	SystemInit();
	init_GPIO();
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
	CRC_ResetDR();
	CRC_SetIDRegister(69);
	init_USART1(9600); // initialize USART1 @ 9600 baud
	mryg();
	STP_send(3, "Init complete! Hello World!\r\n"); // just send a message to indicate that it works

	while (1) {
		if(/*STPStatus()*/READY_FLAG){
			stp_data r = STPRead();
			STP_send(r.arg, r.f_data);
		}
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

int strLen(char* str) {
	int len = 0;
	int i = 0;
	while (str[i]) {
		len++;
		i++;
	}
	return len;
}

void AnalyzeData(char* str) {
	CURRENT_LENGTH = (str[2] << 8) + str[1];
}

void actionLed(char* str) {
	if (str[3] == '1')
		GPIO_SetBits(GPIOD, GPIO_Pin_13);
	else
		GPIO_ResetBits(GPIOD, GPIO_Pin_13);

	if (str[4] == '1')
		GPIO_SetBits(GPIOD, GPIO_Pin_14);
	else
		GPIO_ResetBits(GPIOD, GPIO_Pin_14);

	if (str[5] == '1')
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
	static uint8_t reading_flag = 0;
	// check if the USART1 receive interrupt flag was set
	if (USART_GetITStatus(USART1, USART_IT_RXNE)) {

		static int cnt = 0; // this counter is used to determine the string length
		int t = USART1->DR; // the character from the USART1 data register is saved in t
		//STP_send(1, t);
		//AnalyzeData(t);

		GPIO_SetBits(GPIOD, GPIO_Pin_12);
		if (reading_flag == 0) {
			if ((cnt < 4)) {
				if(READY_FLAG) READY_FLAG = 0;
				received_string[cnt] = t;
				cnt++;
			}
			if (cnt == 4 && received_string[0] == 170) {
				//AnalyzeData(received_string);
				CURRENT_LENGTH = ((received_string[2] << 8) + received_string[1])+1;
				//data[0] = 0;

				received_string[cnt] = 0;
				cnt = 0;
				GPIO_ResetBits(GPIOD, GPIO_Pin_12);
				reading_flag = 1;
			}
		}
		else{
			if (cnt < CURRENT_LENGTH) {
				data[cnt] = t;
				cnt++;
			} else {
				if(t!='\n');
				if(data);
				char asd = t;
				READY_FLAG = 1;
				cnt = 0;
				GPIO_ResetBits(GPIOD, GPIO_Pin_12);
				reading_flag = 0;
			}

		}
	}

}
void mryg() {
	int i;
	GPIO_SetBits(GPIOD, GPIO_Pin_12);
	for (i = 0; i < 1000000; i++)
		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_12);

	GPIO_SetBits(GPIOD, GPIO_Pin_13);
	for (i = 0; i < 1000000; i++)
		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_13);

	GPIO_SetBits(GPIOD, GPIO_Pin_14);
	for (i = 0; i < 1000000; i++)
		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_14);

	GPIO_SetBits(GPIOD, GPIO_Pin_15);
	for (i = 0; i < 1000000; i++)
		;
	GPIO_ResetBits(GPIOD, GPIO_Pin_15);
	for (i = 0; i < 1000000; i++)
		;
}
