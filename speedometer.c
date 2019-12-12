/********************************************************************/
/* uC and LCD Connections
	TP1 - Vcc (+5v)
	TP3 - Vss (Gnd)
	P1.2 - EN
	P1.3 - RS
	P1.4 - D4
	P1.5 - D5
	P1.6 - D6
	P1.7 - D7
	Gnd  - RW
	Gnd  - Vee/Vdd - Connect to Gnd through a 1K Resistor
			- this value determines contrast -
			- i.e. without resistor all dots always visible, whereas
			- higher resistor means dots not at all displayed.
	Gnd  - K (LED-)
	Vcc  - A (LED+) +5V - For Backlight
	Clock: 1MHz														*/
/********************************************************************/
/*
	Magentic sensor connected to P2.0
*/
#include <msp430g2553.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LED BIT0
#define SENSOR BIT0
#define INITIAL_TIME 32000 // 32ms for 1 Mhz clock
#define TIRE_DIAMETER 0.285 // Measure tire diameter in meters and put it here

// uC GPIO Port assignment
#define UC_PORT    	P1OUT
#define UC_PORT_DIR	P1DIR

#define LCD_EN     		BIT2
#define LCD_RS      	BIT3
#define LCD_DATA		BIT4 | BIT5 | BIT6 | BIT7
#define LCD_D0_OFFSET	4	// D0 at BIT4, so it is 4
#define US_MASK			US_TRIG | US_ECHO
#define LCD_MASK		LCD_EN | LCD_RS | LCD_DATA

int long_timer_period = 0;
double distance = 0.0;
long int current_time = 0;
long int limit_time = 500000;
int speed = 0;
double tire_circumference = TIRE_DIAMETER * 2 * M_PI;
char testLine[6];

void lcd_reset()
{
	UC_PORT = 0x00;
	__delay_cycles(20000);

	UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;	
	__delay_cycles(10000);

	UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;	
	__delay_cycles(1000);

	UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;	
	__delay_cycles(1000);

	UC_PORT = (0x02 << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;	
	__delay_cycles(1000);

}

void lcd_cmd (char cmd)
{
	// Send upper nibble
	UC_PORT = (((cmd >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;

	// Send lower nibble
	UC_PORT = ((cmd & 0x0F) << LCD_D0_OFFSET) | LCD_EN;
	UC_PORT &= ~LCD_EN;

	__delay_cycles(5000);
}

void lcd_data (unsigned char dat)
{
	// Send upper nibble
	UC_PORT = ((((dat >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
	UC_PORT &= ~LCD_EN;

	// Send lower nibble
	UC_PORT = (((dat & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
	UC_PORT &= ~LCD_EN;

	__delay_cycles(5000); // a small delay may result in missing char display
}

void lcd_init ()
{
	lcd_reset();         // Call LCD reset

	lcd_cmd(0x28);       // 4-bit mode - 2 line - 5x7 font.
	lcd_cmd(0x0C);       // Display no cursor - no blink.
	lcd_cmd(0x06);       // Automatic Increment - No Display shift.
	lcd_cmd(0x80);       // Address DDRAM with 0 offset 80h.
	lcd_cmd(0x01);		 // Clear screen

}

void display_line(char *line)
{
	while (*line)
		lcd_data(*line++);
}

void display_distance(char *line, int len)
{
	while (len--)
		if (*line)
			lcd_data(*line++);
		else
			lcd_data(' ');
}

// Setting DCO clock to calibrated 1Mhz
void set_DCO_1MHz(){
	DCOCTL = 0;
	BCSCTL1 = CALBC1_1MHZ;
	DCOCTL = CALDCO_1MHZ;
	
	// Divert the SMCLK to DCO
	BCSCTL2 &= ~SELS;
	//BCSCTL2 |= DIVS_3;
}

void initialize_TIMER_A(){
	TACCR0 = INITIAL_TIME;
	TACTL = TASSEL_2|ID_0|MC_1|TACLR;
	// Enable interrupt in up mode
	TACCTL0 &= ~CCIFG;
	TACCTL0 |= CCIE;
}

void initialize_sensor_interrupt(){

	P2DIR &= ~SENSOR;	// Sensor is connected to P2.0
	P2REN |= SENSOR;	// Enable built-in resistor
	P2OUT |= SENSOR;	// Set resistor as pull-up
	P2IES |= SENSOR;	// Set interrupt in faling edge
	P2IFG &= ~SENSOR;
	//P2IE |= SENSOR;		// Enable interrupt
}

//** Short Timer Interrupt ******
#pragma vector = TIMER0_A0_VECTOR
__interrupt void T0A0_ISR() {
	TACTL &= ~MC_0;	// Halt the timer
	TACCTL0 &= CCIFG; // Reset the flag
	if( (P2IN & SENSOR) == 0 ){	// Sensor is raised
		P2IE &= ~SENSOR;		// Disable sensor interrupt
		TACTL = TASSEL_2|MC_1|TACLR;	//Restart the timer to count up to 32ms
		TACCTL0 |= CCIE;
		return;
	}else{	// Sensor is not raised
		current_time += INITIAL_TIME;
		P2IFG &= ~SENSOR;
		P2IE |= SENSOR;	// Enable sesnor interrupt
		TA0CTL = TASSEL_2|ID_0|MC_2|TAIE|TACLR;	//Starting the long timer in continues mode and enabling interrupt
		return;
	}
}

//***Long Timer Interrupt *******
#pragma vector = TIMER0_A1_VECTOR 
__interrupt void T0A1_ISR() {
	P1OUT ^= LED;
	TA0CTL &= ~MC_0;			// Halt the timer
	if( long_timer_period > 20){
		// Reset time veriables (no movement)
		current_time = 0;
		long_timer_period = 0;
		lcd_cmd(0x01); // Clear the screen
		lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
		//lcd_cmd(0x80);
		display_line(" Speed");
		lcd_cmd(0xc0); // select 2nd line (0x80 + addr) - here addr = 0x4e
		//lcd_cmd(0xc0);
		display_line(" 0 kmph");
	}else{
		long_timer_period++;
	}
	TA0CTL = TASSEL_2|ID_0|MC_2|TACLR|TAIE; 	// Restart the timer in continiues mode and enabling interrupt
}

#pragma vector = PORT2_VECTOR;
__interrupt void Port2_ISR() {
	
	P2IFG &= ~SENSOR;
	P2IE &= ~SENSOR;		// Disable sensor interrupt
	TA0CTL &= ~MC_0;	// Halt long timer
	char line1[17];
	char line2[17];
	TACTL &= ~MC_0;	// Halt the timer
	current_time = TA0R + (65535 * long_timer_period);
	speed = (int)(tire_circumference / (current_time / 1000000.0));
	long_timer_period = 0;
	current_time = 0;

	// Print the result to the LCD
	sprintf(line1, " Speed");
	sprintf(line2, " %d kmph", speed);
	lcd_cmd(0x01); // Clear the screen
	lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
	display_line(line1);
	lcd_cmd(0xc0); // select 2nd line (0x80 + addr) - here addr = 0x4e
	display_line(line2);
	
	// Restart the short timer and interrupt in up mode
	TACCR0 = INITIAL_TIME;
	TACTL = TASSEL_2|MC_1|TACLR;
	TACCTL0 &= ~CCIFG;
	TACCTL0 |= CCIE;
	
}

int main(){
	
	WDTCTL = WDTPW | WDTHOLD;	// Stop the Watchdog timer
	
	UC_PORT_DIR = LCD_MASK;			// Output direction for LCD connections
	
	P1DIR |= LED;	// Set LED to output
	P1OUT &= ~LED; // Set LED offs
	
	// Initialize LCD
	lcd_init();
	
	lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
	display_line("Made by");
	lcd_cmd(0xc0); // select 2nd line (0x80 + addr) - here addr = 0x4e
	display_line("Daniel Ram");
	__delay_cycles(1000000);
	
	lcd_cmd(0x01); // Clear the screen
	lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
	display_line(" Speed");
	lcd_cmd(0xc0); // select 2nd line (0x80 + addr) - here addr = 0x4e
	display_line(" 0 kmph");
	
	set_DCO_1MHz();
	initialize_sensor_interrupt();
	initialize_TIMER_A();
	
	// Enable global interrupts
    __enable_interrupt();
	
	for(;;){}

	return 0;
}
