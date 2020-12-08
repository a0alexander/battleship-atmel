/*
 * Battleship_v2.3 - Amila Alexander - Introduction to Embedded Systems
 *
 * Created: 12/21/2019 1:51:43 PM
 * Author : Amila
 */ 

#ifndef F_CPU
#define F_CPU 12000000UL	// Set 12 MHz clock speed (strictly for delay.h in LCD library)
#endif						//delay.h library not required for battleship v_2.3

#include <avr/io.h>
#include <avr/eeprom.h>		//library which enables read,write, wait operations of EEPROM

#include <avr/interrupt.h>	//library for interrupt manipulation
#include <avr/wdt.h>		//watchdog timer library (required for soft_reset)
#include <ctype.h>			//library which enables digit/character comparison ie. is number or letter?
	
#define D4 eS_PORTA0		//defining PINS for LCD board 
#define D5 eS_PORTA1
#define D6 eS_PORTA2
#define D7 eS_PORTA3
#define RS eS_PORTD6
#define EN eS_PORTA4

void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));	//required for soft_reset

//function to execute simulated press of the RESET button 								
#define soft_reset()        \
do                          \
{                           \
	wdt_enable(WDTO_15MS);  \
	for(;;)                 \
	{                       \
	}                       \
} while(0)



#include <stdbool.h>		//required for variable of bool type
#include <string.h>			//required for string operations
#include "lcd.h"			//LCD library by electroSome
#include "max7219.h"		//max7219 library by Davide Gironi
#include <stdlib.h>			//required to cast char to int type





void loadMaptoLED(int row, int col,uint8_t arr[8][8]);	 //method to load any 8x8 map to 8x8 array
void writeNumber(int count, int row,int col);			 //method to write number to LCD 
void writeToLoc(int row, int col,char* s);				// method to write string to any location on LCD 
void shoot(int row, int col);							//method executes when used "shoots"

void clearAll();										//clear all chars from LCD display
void initialize7219();									//initialize max7219 8x8 LED array
void writeTo7219(int row, int col);						//method to write to 8x8 LED array

#define USART_BAUDRATE 9600								//define BAUD rate
//#define inputc (PINC && (1<<PC0))
enum states{LOAD, PLAY, READ,CREATE,OVER,WIN};			//FSM state initialization 
volatile enum states state;								//enumerable initialization 
volatile enum states current_state;						//variable to store current state and parse when required 

#define BAUD_PRESCALE (((( F_CPU / 16) + ( USART_BAUDRATE / 2)) / ( USART_BAUDRATE )) - 1)
volatile char Receivedbyte;								//variable which stores transmitted/received data

int i=0;
int j=0;
int allowance;
void loadMap();				//method runs during state LOAD (puts system into state which waits for map from PC/player2 (if they choose to upload)) 
void readMap();				//method which initiates reading the 8x8 map one line at a time
void blink();				//blink an LED on the board
void play();				//method runs during state PLAY, game is played during this state
void printdata();			//method to print out the print out the array on terminal.exe (for debugging)
void writeLCD(enum states state1, char* s);		//method to write to LCD without constant redundant looping ie. only once during a while loops
void checkData();								//checks if correct data is received in the sent map 
void SerialPrintArray(uint8_t arr[8][8]);		//prints map to both terminal.exe and shows the map on the 8x8 LED array
void blinkForHit(int row, int col, uint8_t arr[8][8]);		//once ship is hit it blinks the location on the 8x8 array
bool checkforOthers(uint8_t shipPart, uint8_t arr[8][8]);	//method checks if a ship is remaining on the battlefield, returns true if ship is present
void defineP2PMap(int row, int col);						//method for changing ship type, ie. ships can have nine versions
void defineP2PMap(int row, int col);
void create();									//method runs during CREATE state for and required for map creation
bool checkforWin(uint8_t arr[8][8]);			//check if all the ships are sunk
void writetoEEPROM(uint8_t arr[8][8]);			//method to write any uint8_t array of 8x8 to EEPROM
void SerialButtons();							//uses MACROS in terminal to enable up/down/left/right/shoot commands (virtual buttons)

	volatile int timerCount=0;					//variable for custom delay timer 
	volatile bool activated=false;				//enables custom timer to function 
	void myDelay(int delay1);					//method for custom timer/ _delay_ms() not required

uint8_t ic = 0;									//defines which 8x8 LED array user references, since only 1 led array, ic=0
						
uint8_t checkArray[8][8];						//array used to store read EEPROM information
uint8_t shotsArray[8][8];						//array used to track the users shot information
uint8_t shipsArray[8][8];						//array to store locations of ships (copies checkarray info and enables manipulation)
uint8_t p2pArray[8][8];							//array used to store the users newly created map (to 2player)


 volatile int row_select=0;						//cursor movements are enabled via these variables
 volatile int col_select=0;						//volatile variable due to INT1 manipulation
 
 uint8_t shipType[] = {'1','2','3','4','5','6','7','8','9'};	//nine different ship types user can define
 int maxShips = 9;								//max ship types 
 int currentShipType =0;						//variable for storing current ship type
 int GAME_OVER_SHOTS = 15;						//number of shots till GAME OVER
 int gameOverCount=0;							//count variable for storing number of shots
 bool hasEERead=false;							//check whether EEPROM has been read 

 



int main(void)
{
    memset(shotsArray,'0',sizeof(shotsArray));		//fill shot tracking 8x8 array with zeros
	memset(shipsArray,'0',sizeof(shipsArray));		//fill "location of ships" array with zeros
	memset(p2pArray,'0',sizeof(p2pArray));			//fill "creator's map" with zeros
	
   	//Setup the Baud Rate
   	UBRRH = ( BAUD_PRESCALE >> 8); //load baud rate to high byte
   	UBRRL = BAUD_PRESCALE ; //load baud rate to low byte
   	//Configure data format for transmission
   	UCSRC = (1 << URSEL ) | (1 << UCSZ0 ) | (1 << UCSZ1 ); // set data frame
   	UCSRB = (1 << RXEN ) | (1 << TXEN ); // Turn on the transmission and reception circuitry
	UCSRB |=(1<<RXCIE);
   
	MCUCR |= (1<<ISC11);							//enable INT1 on rising edge
	MCUCR |= (1<<ISC10);							
	GICR |=(1<<INT1);								//enable external interrupt 1
  
  	DDRA = 0XFF;									//set outputs for LCD display
  	DDRA &= ~(1<<PA5);								//set as input (PA45) used as potentiometer output
  	DDRD |=(1<<PD6);								// VS of LCD requires digital pin 
	  
	  	TCCR1B |= (1 << WGM12)|(1 << CS10);			//enable CTC mode of timer1 with no pre scaler
	  	OCR1A=11999;								//corresponds to timing of 1ms
	 
  	
   
   Lcd4_Init();										//initialize LCD display
   Lcd4_Clear();									//clear all chars from LCD
   max7219_init();									//initialize max7219
   initialize7219();								//run other initialization steps of 8x8 array
   
   
   sei();											//enable global interrupts 
   /*DDRB = 0xFF;*/
   DDRB =0x00;										//enable inputs on PORTB
	
													
   state=PLAY;										//initialize first state on LOAD
   
   
    while (1) 
    {
		
		
		switch(state){
			
					case LOAD:loadMap();PORTB|=(1<<PB5);writeLCD(LOAD,"Upload Map...")				  ;break; //state to enable map receiving
					case PLAY:PORTB&=~(1<<PB5);play(); writeLCD(PLAY,"Play Game!");UCSRB |= (1<<RXCIE);break; //state for playing game
					case READ:PORTB|=(1<<PB7); readMap()											  ;break; //state which reads map received from Serial
					case CREATE:create(); writeLCD(CREATE,"P2P Share")								  ;break; //state for on board map creation
					case OVER: Lcd4_Write_String("GAME OVER     ");writeLCD(OVER,"GAME OVER")		  ;break; //end game state
					case WIN: Lcd4_Write_String("WINNER   "); writeLCD(WIN,"WINNER!")				  ;break; //winner game state
					default: state=PLAY																  ;break; //default game state
	
	      }

	
	
		
		
    }
	
	
	
	
	
	
}

//ISR for receiving Serial data
ISR(USART_RXC_vect){
	
	
	Receivedbyte = UDR;
	UDR = Receivedbyte;
	
	
	
}

//ISR for Interrupt 1 (switches between game mode states PLAY, LOAD & CREATE)
ISR(INT1_vect){
	
	
	
	if(state==PLAY){
		state =LOAD;
		
	}
	else if(state==LOAD){
		
		state = CREATE;
		
	}
	else if(state==CREATE){
		state=PLAY;
		
	}
	else{
		state=PLAY;
		
	}
	
	row_select=0;
	col_select=0;

	
}

//method which detects the character 'b' from serial and sets the state to READ
void loadMap(){
	

	if(Receivedbyte=='b'){
		PORTB ^= (1<<PB4);				//toggle LED
		UCSRB &= ~(1<<RXCIE);			//turn off Interrupt for normal read of serial data
		state = READ;					//set state to READ 

	};
	
		}
	
	uint8_t arr1[]={};					//array to temporarily store incoming serial map data
		int count11=-1;					//variable used to update index of above variable
	
	void readMap(){
		
		while(( UCSRA & (1 << RXC )) == 0){if(state!=READ){break;}}; //wait for incoming bytes, break if state is not equal READ
		Receivedbyte = UDR ;			//set received data to receivedbyte char			
		
		

					if(isdigit(Receivedbyte)){			//check if incoming data is a number 1 ~ 9
						count11++;						//increment index counting variable
						arr1[count11] = Receivedbyte;	//write received data to array
						writeNumber(count11*1.6,2,12);	//write loading progress to LCD
							if(count11==63){			//signals 64 chars have been received
								Lcd4_Set_Cursor(1,0);
								Lcd4_Write_String("Loading...");
								printdata();
								
								
							}
					
						
					}
					
				
					
				
				
				
				PORTB &= ~(1<<PB3);

		
	}
	
	void blink(){
		
		for(int i = 0; i<6; i++){
			PORTB |=(1<<PB2);
			myDelay(200);
			PORTB &=~(1<<PB2);
			myDelay(200);
			
			
		}
		
		
	}
	
	
	int q1=0;				//redundant variable used to write data to serial monitor (for debugging)
	void printdata(){		
		uint8_t newarray31[8][8];

	memcpy(newarray31,arr1,64*sizeof(uint8_t));		//copies contents of arr1 to newarray31
													//converts 1D array to 2D array
	
	for(int h =0;h<8;h++){
		
		for(int q = 0 ;q<8;q++){
			
			UDR = newarray31[h][q];					//prints data to serial monitor
			myDelay(50);
			
		}
		UDR=0x0D;									//ASCII char for line break, so data prints in multiple rows
		
		
	}
	if((sizeof(newarray31)/sizeof(uint8_t))==64){	//verifies if 64 bytes have been received 
			writeNumber((sizeof(newarray31)/sizeof(uint8_t)),2,0);	//write 64 to LCD display
			writeToLoc(1,0,"Load Complete!");						//write load complete on second line of LCD
	}
	else{
		writeToLoc(1,0,"Invalid Data");				//if data is incorrect show user message
	}

	eeprom_write_block((const void*)newarray31,(void*)60,sizeof(newarray31)/sizeof(uint8_t)); //write 2D array to 60th position of EEPROM
	eeprom_busy_wait();								//wait for write to complete
	while(EECR &&(1<<EEWE)){writeToLoc(1,0,"flashing to EEPROM");};
		
		writeToLoc(1,0,"flash complete");			//show user message notifying EEPROM write complete
			 row_select=0;							
			 col_select=0;
		checkData();								//write to serial monitor EEPROM data for verification
		soft_reset();								//perform soft reset

		
	}
	
void writeNumber(int count, int row,int col){	//method for writing integer to LCD display
	
	
	
	char str[12];								//char array to store number
	if(count>9){								//determines if single digit number of multi digit
		Lcd4_Set_Cursor(row,col);
		
	}
	else if(count>=9){
		Lcd4_Set_Cursor(row,col);				//adjust rows and varies accordingly to properly display digits
		Lcd4_Write_String(" ");
	}
	else{
		Lcd4_Set_Cursor(row,col+1);
	}
	Lcd4_Write_String(itoa(count,str,10));		//converts data of int and converts to char*
	
	
}
void writeLCD(enum states state1, char* s){		//special method to write to LCD a single time even in while loops
	

	if(state1!=current_state){					//uses the state variable to determine when to next write
		
		
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String(s);

		
		current_state = state1;					//saves the state variable and compares with the current state to see if write is required 
		
	}
	

}

void writeToLoc(int row, int col,char* s){		//method to write string to predefined location
												//accepts location and string to write
	
	Lcd4_Set_Cursor(row,col);
	Lcd4_Write_String(s);
	
	
}

void checkData(){	//check data which was wrritten in EEPROM
	
	
	
	eeprom_read_block((void*)checkArray, (const void*)60,(sizeof(checkArray)/sizeof(uint8_t))); //reads data from 60th position and saves to checkArray
	eeprom_busy_wait();	//wait till the read operation is complete

	
		
		for(int h =0;h<8;h++){		//print data to serial monitor in rows 
			
			for(int q = 0 ;q<8;q++){
				
				UDR = checkArray[h][q];
				myDelay(10);
				
			}
			UDR=0x0D;
		
		}

	 row_select=0;
	col_select=0;
	

	
	
	
}

		
char char_current;
//button input definitions
#define UP !(PINB & (1<<PB0))
#define RIGHT !(PINB & (1<<PB2))
#define DOWN !(PINB & (1<<PB1))
#define LEFT !(PINB & (1<<PB3))
#define SHOOT !(PINB & (1<<PB4))
#define SET !(PINB & (1<<PB5))
#define DEFINE !(PINB & (1<<PB4))
#define STAGE !(PINB & (1<<PB6))

bool viewMapMode =false;			//variable to enable showing map on LED array (for debugging)

void play(){				//state when playing the game 
	
	if(sizeof(checkArray)/sizeof(uint8_t)){		//perform one time read operation and load to in-program memory array
		
		if((eeprom_read_byte((const uint8_t*)60)=='1' ||eeprom_read_byte((const uint8_t*)60)=='0')&&(hasEERead==false) ){ //uses condition to check if data is available in EEPROM by reading single byte at 60th pos
			writeToLoc(1,0,"Loading EEPROM");																			// if data is in EEPROM 1 byte is received otherwise null is returned which means data CAN be read from EEPROM
			eeprom_read_block((void*)checkArray, (const void*)60,(sizeof(checkArray)/sizeof(uint8_t)));
			eeprom_busy_wait();
			memcpy(shipsArray,checkArray,64*sizeof(uint8_t));			//copies read EEPROM data to shipsArray
			writeToLoc(1,0,"Start Game!       ");
			hasEERead=true;												//variable prevents further reads from EEPROM
		}
		else if(hasEERead==false){										//displays welcome message when no map data in EEPROM
			writeToLoc(1,0,"Battleship v1");
		}
		
		
		
	}
	
	
if(UP){
	
	row_select--;				//decrements cursor position by 1 row
	myDelay(200);				//custom timer 200ms
}	
if(DOWN){
	
	row_select++;				//increments cursor position by 1 row
	myDelay(200);
}
if(LEFT){
	
	col_select--;				//decrements cursor position by 1 column
	myDelay(200);
	
}
if(RIGHT){
	col_select++;				//increments cursor position by 1 column
	myDelay(200);
// 	char sendP2P = 'a';
// 	UDR=sendP2P;
	
}
if(SHOOT){						
	
	shoot(row_select,col_select);		//method takes coordinates of shot and checks is miss/hit/already hit 
	myDelay(200);
		gameOverCount++;				//counts number of shots the user takes
		writeNumber(gameOverCount,1,13);
		if(gameOverCount>GAME_OVER_SHOTS){
			state=OVER;						//if maximum shots are taken then GAME OVER
			
			
		}
}
	

		SerialButtons();			// enable using MACROS of the terminal to simulate button presses up/down/left/right/shoot
			
	if(row_select>7){				//prevents rows/ colums from exceeding avaialable rows/cols in 8x8 array
		row_select=0;
	}
	if(row_select<0){
		row_select=7;
	}
	if(col_select>7){
			col_select=0;
		}
		
	if(col_select<0){
				col_select=7;
			}
			
			
	
	Receivedbyte=0;					//writes received byte (from serial which virtual buttons use) to zero prevent false detecting constant button presses
	if(viewMapMode==true){			//if the map needs to be seen on the serial monitor & 8x8 array 
									//it prevents the rest of program execution (in current method) beyond this point
		return;
	}
	writeTo7219(row_select,col_select);	//method to write to 8x8 array
	
		
	writeNumber(row_select,2,0);	//write current coordinate to LCD display
	writeNumber(col_select,2,3);
	
	
	
}

int shipKillCount=0;				//variable which counts the number of ships sunk

void shoot(int row, int col){		//method which computes hit/miss/already hit and other functions kills/win/game over
	
	
	if(isdigit(checkArray[row][col])==true &&checkArray[row][col]!='0'){	//check if shot location is a number 1 ~ 9 and not 0
																			//zero is water here 
		if(shotsArray[row][col]=='1'){										//if shot location already shot location, don't run rest of current method
			Lcd4_Set_Cursor(1,0);
			Lcd4_Write_String("already hit");
			return;
		}
		
		writeToLoc(1,0,"hit!        ");									//if ship is present at shot location display "hit"
		shotsArray[row][col]='1';										//keep track of shots in this array
		blinkForHit(row,col,shotsArray);								//blink location of LED where shit is hit, ie is (0,3) is hit, blink that LED light
		uint8_t shipType = shipsArray[row][col];						//get the "shiptype" that has been shot and save it as variable
		shipsArray[row][col]='0';									//when ship is shot, map updates to 0 (water)
		
		if(checkforOthers(shipType,shipsArray)){		//perform check to see if ship is fully sunk via method, takes "shiptype" and location of hit
			//writeToLoc(1,0,"others there!        ");				//if ship not fully sunk
			
		}
		else{
			//writeToLoc(1,0,"others not there!        ");
			shipKillCount++;										//increment kill count
			writeNumber(shipKillCount,2,13);						//if ship fully sunk
			writeToLoc(2,8,"Kills");					
			
			
		}
		
		
	}
	else if(checkArray[row][col]=='0'){								//signal "miss" to user
		writeToLoc(1,0,"miss!       ");								//checks for 0 (water)
		
		
	}
	else{	
		writeToLoc(1,0,"error");									// if invalid map data loaded
		
	}
	
	
	if(checkforWin(shipsArray)==true && shipKillCount>0){
		state=WIN;													// if all ships sunk set to WIN state
	}
	
	

}

void initialize7219(){
	
				max7219_shutdown(ic, 1);	//power on
				max7219_test(ic, 0);		//test mode off
				max7219_decode(ic, 0);		//use led matrix number 0
				max7219_intensity(ic, 0);	//intensity, brightness of LEDs
				max7219_scanlimit(ic,7);	//set number rows/columns to drive 8x8, 7x7, 6x6 etc
				
}

void clearAll(){							//method to turn off all LEDs in 8x8 array
	
	for(int i11=0;i11<8;i11++){
		
		max7219_digit(0,i11,0);				//write to LED array, 0th led matrix, i11th row, 0 (byte)

	}
	
}

volatile int prev_row;						
void writeTo7219(int row, int col){ //write to LED array
	
	
	if(row>7 || row<0){
		row=0;
	}
	if(col>7 || col<0){
		col=0;
	}
	
	
	
	loadMaptoLED(row,col,shotsArray);	//method called to write any array to LED matrix
	
	
	
}

void wdt_init(void)			//watch dog timer disable
{
	MCUSR = 0;				//disable all external interrupts
	wdt_disable();			//disable watchdog timer from triggering constant RESET

	return;
}

void initializeProper(){	//ensures LCD displays proper information on first run
	
	if(allowance<2){
		Lcd4_Clear();
		Lcd4_Write_String("Play Game!");
		allowance++;
	}
	
	
}





void loadMaptoLED(int row, int col,uint8_t arr[8][8]){ //special method to write to LED array
													//enables proper cursor movement,blinking, persistance
	for(int i = 0; i<8;i++){
													//reads an array row by row and saves each row to a byte
		uint8_t writeByte=0;
		for (int j =0;j<8;j++){
			
			if(isdigit(arr[i][7-j])==true && arr[i][7-j] != '0'  ){	//if data is present on map (from shotsarary etc) in row, write row to byte
				writeByte|=(1<<(7-j));
				
			}
			else{writeByte&=~(1<<(7-j));}				//if no data present write 0 to bit in writebyte
				
				
		}
		
		if(row==(i)){
				writeByte |=(1<<(col));					//add cursor alongside with shots etc data
		}
		
		max7219_digit(ic,7-i,writeByte);				//writes to LED array row by row (happens extremely quick)
		
	}	
	
}

void SerialPrintArray(uint8_t arr[8][8]){			//print any array to serial monitor (for debugging)
	
		for(int h =0;h<8;h++){
			
			for(int q = 0 ;q<8;q++){
				
				UDR = arr[h][q];
				myDelay(50);
				
			}
			UDR=0x0D;								// ASCII character for line break
			
			
		}
	
	
	
}

void blinkForHit(int row, int col, uint8_t arr[8][8]){	//blinks hit location is ship is hit
	
	
	
			uint8_t writeByte=0;			//reads location of cursor in row and converts row to byte
			for (int j =0;j<8;j++){
				
				if(arr[row][7-j]=='1'){
					writeByte|=(1<<(7-j));	//write 1 to bit in byte, i.e 0b00000001 (if col 0)
					
				}
				else{writeByte&=~(1<<(7-j));}
				
			}
			
			
	
	for(int i= 0; i<5;i++){					//blink hit location LED
		max7219_digit(ic,7-row,writeByte |= (1<<col));
		myDelay(50);
		max7219_digit(ic,7-row,writeByte &= ~(1<<col));
		myDelay(50);
		
		
	}
	

}

int shipPartcounter=0;						//method to detect if ship is sunk or not
bool checkforOthers(uint8_t shipPart, uint8_t arr[8][8]){
	
	for(int j=0; j<8; j++){
		
		for(int i =0;i<8; i++){
			if(arr[j][i]==shipPart){
				shipPartcounter++;
				
			} 
			
		}
	}
	
if(shipPartcounter==0){					//returns false is ship is not yet sunk
	shipPartcounter=0;
	return false;	
}
else{
	shipPartcounter=0;					//returns true if ship is sunk
	return true;
}


	
}

int winCount=0;
bool checkforWin(uint8_t arr[8][8]){		//check for win, if all ships have been sunk
		
		for(int j=0; j<8; j++){
			
			for(int i =0;i<8; i++){
				if(arr[j][i]!='0'){
					winCount++;				//counts the number of ships left
					
				}
				
			}
		}
		

		
		
	if(winCount==0){						//if no ships are left return true
		shipPartcounter=0;
		return true;
	}
	else{
		winCount=0;							//if ships remain return false
		return false;
	}	
	
	
		
		
	
}

void myDelay(int delay1){			//custom delay timer 
		
	activated=true;					//variable which enables the counter to function
	TCNT1=0;						//sets TCNT1 register to zero
	TIMSK |= (1 << OCIE1A);			//enables output compare interrupt 
	
	while(timerCount<delay1){
									//performs count operation/ timerCount increments inside ISR of output compare interrupt
	}
	activated=0;					//turn off timer, enables one time use every time timer is called
	timerCount=0;					//sets timerCount back to zero
	TIMSK &= ~(1 << OCIE1A);		//turns of output compare match interrupt to prevent unnecessary CPU bandwith usage
	
}

ISR (TIMER1_COMPA_vect)				//fires every time OCR1A is equal to value of TCNT1
{		
	if(activated){
		timerCount++;				//increment volatile timerCount variable
	}

}


void create(){						//method used to create map on-board
									//alternatively can be used  for 2 player mode (send cursor locations and shoot)
	if(UP){
		
		row_select--;				//decrements row selection for cursor
		UDR='w';					//write  to serial monitor to signal cursor movements made by second player
		myDelay(200);
	}
	if(DOWN){
		
		row_select++;
		UDR='s';
		myDelay(200);
	}
	if(LEFT){
		
		col_select--;
		UDR='a';
		myDelay(200);
		
	}
	if(RIGHT){
		col_select++;
		UDR='d';
		myDelay(200);
		// 	char sendP2P = 'a';
		// 	UDR=sendP2P;
		
	}
	if(SHOOT){
// 		char a1='a';
// 		UDR=a1;
		UDR='f';
		defineP2PMap(row_select,col_select);	//define where user wants ship to be placed
		myDelay(200);
	}
	if(SET){
		
		currentShipType++;							//while setting up ships user can change shiptype
		writeNumber(currentShipType,2,0);
		myDelay(200);
		
	}
	if(STAGE){										//stage button sets the map up so the second player can play
		
		myDelay(500);
		if(STAGE){			//if stage button is pressed and held it uploads map
			UDR='b';								//writes the char 'b' to second player so they can begin downloading map
			SerialPrintArray(p2pArray);
			writetoEEPROM(p2pArray);
			myDelay(200);
			
		}
		else{				//single press writes complete map to EEPROM
			writetoEEPROM(p2pArray);
			myDelay(200);
		}
		
		
		
	}
	
	
		if(row_select>7){
			row_select=0;
		}
		if(row_select<0){
			row_select=7;
		}
		if(col_select>7){
			col_select=0;
		}
		
		if(col_select<0){
			col_select=7;
		}
		
	loadMaptoLED(row_select,col_select,p2pArray);			//display cursor and ships placed on LED array
	
	
	
}



void defineP2PMap(int row, int col){				//method to define ship location
	
	
	if(currentShipType<=maxShips){

	p2pArray[row][col] = shipType[currentShipType];
	UDR = shipType[currentShipType];				//writes ship type to serial monitor (for debugging)
	myDelay(200);
	
	loadMaptoLED(row,col,p2pArray);					//show map on 8x8 array
	
		}
	
	
	}
	
			
	void writetoEEPROM(uint8_t arr[8][8]){			//method to write any 8x8 uint8_t array to EEPROM
	writeToLoc(2,0,"setting")	;
	
	eeprom_write_block((const void*)arr,(void*)60,sizeof(shipsArray)/sizeof(uint8_t));	//	write received array to 60th position
	eeprom_busy_wait();								//
	
	while(EECR &&(1<<EEWE)){writeToLoc(2,0,"flashing to EEPROM");};
		state=PLAY;
		
		
	}
	
	void SerialButtons(){
		
			if(Receivedbyte=='a'){
				
				col_select--;
				//loadMaptoLED();
				//char_current==Receivedbyte;
			}
			else if(Receivedbyte=='d'){
				col_select++;
				//char_current==Receivedbyte;
				
			}
			else if(Receivedbyte=='w'){
				row_select--;
				//char_current==Receivedbyte;
				
			}
			else if(Receivedbyte=='s'){
				row_select++;
				//char_current==Receivedbyte;
				
			}
			else if(Receivedbyte=='f'){
				
				shoot(row_select,col_select);
				//SerialPrintArray(checkArray);
				
			}
			else if(Receivedbyte=='h'){
				
				checkData();
				viewMapMode=true;
				loadMaptoLED(0,0,checkArray);
			}
		
		
		
		
	}