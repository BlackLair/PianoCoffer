// 라즈베리파이에서 I2C를 활용한 LCD 제어 C언어 소스코드
// 다음 오픈소스를 참조하였음
// https://github.com/eeenjoy/I2C_LCD2004



#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <string.h>


// LCD Address
//#define ADDRESS 0x3F
#define ADDRESS 0x27

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00


//int LCDAddr = 0x27;
int LCDAddr = 0x27;
int BLEN = 1;
int fd;

extern void myDelayms(int);






void lcd_write_word(int data)
{
    int temp = data;
	if ( BLEN == 1 )
		temp |= 0x08;
	else
		temp &= 0xF7;
	wiringPiI2CWrite( fd, temp );
}

void lcd_send_command(int comm)
{
	int buf;
	// Send bit7-4 firstly
	buf = comm & 0xF0;
	buf |= 0x04;			// RS = 0, RW = 0, EN = 1
	lcd_write_word(buf);
	myDelayms(2);
	buf &= 0xFB;			// Make EN = 0
	lcd_write_word(buf);

	// Send bit3-0 secondly
	buf = (comm & 0x0F) << 4;
	buf |= 0x04;			// RS = 0, RW = 0, EN = 1
	lcd_write_word(buf);
	myDelayms(2);
	buf &= 0xFB;			// Make EN = 0
	lcd_write_word(buf);
}

void lcd_send_data(int data)
{
	int buf;
	// Send bit7-4 firstly
	buf = data & 0xF0;
	buf |= 0x05;			// RS = 1, RW = 0, EN = 1
	lcd_write_word(buf);
	myDelayms(2);
	buf &= 0xFB;			// Make EN = 0
	lcd_write_word(buf);

	// Send bit3-0 secondly
	buf = (data & 0x0F) << 4;
	buf |= 0x05;			// RS = 1, RW = 0, EN = 1
	lcd_write_word(buf);
	myDelayms(2);
	buf &= 0xFB;			// Make EN = 0
	lcd_write_word(buf);
}

void lcd_init( void )
{
	fd = wiringPiI2CSetup(LCDAddr);	// LCD I2C 초기화
	lcd_send_command(0x33);	// Must initialize to 8-line mode at first
	delay(5);
	lcd_send_command(0x32);	// Then initialize to 4-line mode
	delay(5);
	lcd_send_command(0x28);	// 2 Lines & 5*7 dots
	delay(5);
	lcd_send_command(0x0C);	// Enable display without cursor
	delay(5);
	lcd_send_command(0x01);	// Clear Screen
	wiringPiI2CWrite(fd, 0x08);
}

void lcd_clear( void )
{
	lcd_send_command(LCD_CLEARDISPLAY);	//clear Screen
	lcd_send_command(LCD_RETURNHOME);	//clear Screen
}

void lcd_write(int x, int y, char data[])
{
	int addr, i;
	int tmp;
	myDelayms(15);
	if (x < 0)  x = 0;
	if (x > 19) x = 19;
	if (y < 0)  y = 0;
	if (y > 3)  y = 3;
	// Move cursor
	if ( y == 0 )
	    addr = 0x80 + x;
    if ( y == 1 )
		addr = 0xC0 + x;
	if ( y == 2 )
		addr = 0x94 + x;
	if ( y == 3 )
		addr = 0xD4 + x;
	lcd_send_command(addr);
	
	tmp = strlen(data);
	for (i = 0; i < tmp; i++)
	{
		lcd_send_data(data[i]);
	}
}
