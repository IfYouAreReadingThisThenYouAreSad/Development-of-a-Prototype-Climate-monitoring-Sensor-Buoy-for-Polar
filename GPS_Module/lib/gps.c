/*
 * gps.c
 *
 *  Created on: 17 Feb 2026
 *      Author: thomasneedham
 */



//#include "main.h"


#include "gps.h"

#include <string.h>

GPS_Values GPS = {0};







//------turns off GPS by EN pin---------///


uint8_t GPS_OFF(const gps_pin_assigment *GPS_PINS){



	HAL_GPIO_WritePin(GPS_PINS->port,GPS_PINS->pin, GPIO_PIN_RESET);

	return 0;





}


//-------turns on GPS by EN pin ----------/////

uint8_t GPS_ON(const gps_pin_assigment *GPS_PINS){


	HAL_GPIO_WritePin(GPS_PINS->port, GPS_PINS->pin, GPIO_PIN_SET);

	return 0;



}



//----gets the NEMA strings from GPS and puts it in a buffer in the program-----//
// @return 1 if its recieved NMEA properly, return 0 if fails .

uint8_t getNMEA(const gps_pin_assigment *GPS_PINS){

	GPS.index = 0;
	uint8_t GPS_char = '\0';
	//static uint8_t buffer[100]; // buffer & index are here becuase i cannot see
	//static uint8_t index = 0; // GPS_Buf or gpsIndex in debugger, hence using these for debugging

	HAL_StatusTypeDef GPS_status = HAL_UART_Receive(GPS_PINS->uart, &GPS_char, 1, 100);
	if(HAL_OK == GPS_status && GPS_char == '$'){
		  //buffer[GPS.index] = GPS_char;
		  //HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		  //GPS.Buf[GPS.index] = buffer[GPS.index];
		  GPS.Buf[GPS.index] = GPS_char;
		  GPS.index++;
		  //HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		while(GPS_char != '\n'){


			//uint8_t GPS_char;
			GPS_status = HAL_UART_Receive(GPS_PINS->uart, &GPS_char, 1, 100);

			  if(HAL_OK == GPS_status){
				  //buffer[GPS_index] = GPS_char;
				  //HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
				  //GPS_Buf[GPS_index] = buffer[GPS_index];
				  GPS.Buf[GPS.index] = GPS_char;
				  GPS.index++;
				  //HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
				  if (GPS_char == '\n'){
					  //buffer[GPS_index] = '\0';
					  //GPS_Buf[GPS_index] = buffer[GPS_index];
					  GPS.Buf[GPS.index] = '\0';


					  return 1;}}
			  if(GPS.index >= sizeof(GPS.Buf) -1 ){ // stops buffer overflow, resets index counter

				  GPS.index = 0;
				  return 0;;
			  }

		}}
	return 0;
}



//------ used split NMEA strings to find coordinates and speed -----///
void DecodeNMEA(void){



	/*

	char time[10];
	char postition[4][10];
	char date[10];
	char speedKnots[10];
	char course[10];

	char speedKmh[10];
	char altitude[10];

	*/

	// for debugging



	//-------------------------------$GPRMC------------------------------------------//
	if(strncmp(GPS.token[0], "$GPRMC", 6) == 0 && strncmp(GPS.token[2], "A", 1) == 0){


		strncpy(GPS.time, GPS.token[1], 9);
		GPS.time[9] = '\0';
		strncpy(GPS.speedKnots, GPS.token[7], 9);
		GPS.speedKnots[9] = '\0';
		strncpy(GPS.course, GPS.token[8],9);
		GPS.course[9] = '\0';
		strncpy(GPS.date,GPS.token[9],9);
		GPS.date[9] = '\0';
		for(uint8_t i = 3, idx = 0; i < 6; i++){
			strncpy(GPS.postition[idx],GPS.token[i],9);
			GPS.postition[idx][9] = '\0';
			idx++;
		}
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}
	//-------------------------------$GPGGA------------------------------------------//
	else if(strncmp(GPS.token[0], "$GPGGA", 6) == 0 && GPS.token[6][0] != '0'){
		strncpy(GPS.time, GPS.token[1], 9);
		GPS.time[9] = '\0';
		for(uint8_t i = 2, idx = 0; i < 5; i++){
			strncpy(GPS.postition[idx],GPS.token[i],9);
			GPS.postition[idx][9] = '\0';
			idx++;
		}
		strncpy(GPS.altitude, GPS.token[9], 9);
		GPS.altitude[9] = '\0';
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}



	}
	//-------------------------------$GPVTG------------------------------------------//
	else if(strncmp(GPS.token[0], "$GPVTG",6) == 0){

		strncpy(GPS.speedKmh,GPS.token[7], 9);
		GPS.speedKmh[9] = '\0';
		/* note can added things to record bearing angles*/
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}
	//-------------------------------$GPGSA------------------------------------------//
	else if(strncmp(GPS.token[0], "$GPGSA", 6) == 0){

		/* mainly going to be used to see if the gps should keep trying to get posiston*/
		if(GPS.token[2][0] == '0'){
			//GPS_OFF();//used to turn of gps if there is no satalines in sight
		}
		//while(1){HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);}
	}


}

//----------- splits up NEMA string so it can be used for decoding --------------------///
void paraphase(void){



	  char token[11][20];//using this to see into GPS_token with debugger
	  //HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); //debugging
	  GPS.token[0] = strtok((char*)GPS.Buf,","); //the (char*) tells the compliler to shut the fuck up
	  strncpy(token[0], GPS.token[0], 19);
	  //token[0][19] = '\0';
	  for(uint8_t i =1; i < 10 ; i++){
		  GPS.token[i] = strtok(NULL,","); //this with with line 121 splits the buffer 6 times for parphasing
		  strncpy(token[i], GPS.token[i], 19);
		  token[i][19] = '\0';
		  if(GPS.token[i] == NULL) return;
	  }





}


//---------------- used to check if recieved NMEA is currpted  -------//
// @ return 1 if NMEA is not currpted , return 0 if currpted


uint8_t checksum(void){

	uint8_t idx = 1; // since buffer [0] will have $
	uint8_t checksumValue = 0;
	uint8_t GPSchecksumValue;
	while(GPS.Buf[idx] != '*' && idx <= GPS.index){ //the extra && protect from overflow


		checksumValue ^= GPS.Buf[idx++]; // doing XOR along the NMEA and then increasing idx



	}

	GPSchecksumValue = HexToDec(GPS.Buf[idx+1])*16 + HexToDec(GPS.Buf[idx+2]);


	if(checksumValue == GPSchecksumValue) return 1; // return 1 if date is not currupt

	return 0; // returns 0 if date is currupt







}

//----------- Hexadecimal number to decimal number -------------------///
//@param hexadecimal
// @return decimal
uint8_t HexToDec(char hexValue){


	switch(hexValue){


	case('F'):
			return 15;
	case('E'):
			return 14;
	case('D'):
			return 13;
	case('C'):
			return 12;
	case('B'):
			return 11;
	case('A'):
			return 10;
	case('9'):
			return 9;
	case('8'):
			return 8;
	case('7'):
			return 7;
	case('6'):
			return 6;
	case('5'):
			return 5;
	case('4'):
			return 4;
	case('3'):
			return 3;
	case('2'):
			return 2;
	case('1'):
			return 1;
	case('0'):
			return 0;


	default:
		return 16; //invalid




	}




	//------ used split NMEA strings to find coordinates and speed -----///
	// quicker then old DecodeNMEA
	//
	//
	// @return 1 if finds relvent NMEA string returns 0 if hit default condition or fails

	uint8_t UpgradedDecodeNMEA(void){



		if (!GPS.token[0] || GPS.token[0][0] != '$')
		    return 0;




		switch(GPS.token[0][4]){


	//-------$GPRMC
		case('M'):
				if(strncmp(GPS.token[2], "A", 1) == 0){
				strncpy(GPS.time, GPS.token[1], 9);
				GPS.time[9] = '\0';
				strncpy(GPS.speedKnots, GPS.token[7], 9);
				GPS.speedKnots[9] = '\0';
				strncpy(GPS.course, GPS.token[8],9);
				GPS.course[9] = '\0';
				strncpy(GPS.date,GPS.token[9],9);
				GPS.date[9] = '\0';
				for(uint8_t i = 3, idx = 0; i < 6; i++){
					strncpy(GPS.postition[idx],GPS.token[i],9);
					GPS.postition[idx][9] = '\0';
					idx++;
				}
				return 1;
				}
				return 0;

	//-------$GPGGA
		case('G'):
				if(GPS.token[6][0] != '0'){
				strncpy(GPS.time, GPS.token[1], 9);
				GPS.time[9] = '\0';
				for(uint8_t i = 2, idx = 0; i < 5; i++){
					strncpy(GPS.postition[idx],GPS.token[i],9);
					GPS.postition[idx][9] = '\0';
					idx++;
				}
				strncpy(GPS.altitude, GPS.token[9], 9);
				GPS.altitude[9] = '\0';
				return 1;
				}
				return 0;


	//-------$GPVTG
		case('T'):
				strncpy(GPS.speedKmh,GPS.token[7], 9);
				GPS.speedKmh[9] = '\0';
				/* note can added things to record bearing angles*/ // but im lazy
				return 1;


	//-------$GPGSA
		case('S'):
				/* mainly going to be used to see if the gps should keep trying to get posiston*/
				if(GPS.token[2][0] == '0'){
					//GPS_OFF();//used to turn of gps if there is no satalines in sight
					return 0;
				}
				return 1;

		default:
			return 0;


















		}
















	}








}
