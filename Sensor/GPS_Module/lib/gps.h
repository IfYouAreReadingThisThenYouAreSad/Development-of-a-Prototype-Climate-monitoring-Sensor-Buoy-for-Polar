/*
 * gps.h
 *
 *  Created on: 17 Feb 2026
 *      Author: thomasneedham
 */

#ifndef INC_GPS_H_
#define INC_GPS_H_


#include <stdint.h>
#include "stm32f4xx_hal.h"

//-------structs---------------//

typedef struct {


//------ ENable pin ----///

	GPIO_TypeDef *port;
	uint16_t pin;


//-------uart pins-----------//
	UART_HandleTypeDef *uart;



} gps_pin_assigment;

typedef struct {


	volatile uint8_t Buf[100];
	volatile uint8_t index;
	char* token[10];



	char time[10];
	char postition[4][10];
	char date[10];
	char speedKnots[10];
	char course[10];

	char speedKmh[10];
	char altitude[10];
}GPS_Values;





//-----------Globle variables --------------///

extern GPS_Values GPS;



//--------------Functions-------------------//
uint8_t GPS_OFF(const gps_pin_assigment *GPS_PINS);
uint8_t GPS_ON(const gps_pin_assigment *GPS_PINS);
uint8_t getNMEA(const gps_pin_assigment *GPS_PINS);
void DecodeNMEA(void);
void paraphase(void);
uint8_t checksum(void);
uint8_t HexToDec(char hexValue);
uint8_t UpgradedDecodeNMEA(void);





#endif /* INC_GPS_H_ */
