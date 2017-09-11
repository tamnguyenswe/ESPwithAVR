#include <delay.h>
#include <string.h>
#include <mega128.h>
#include <stdlib.h>

// Graphic Display functions
#include <glcd.h>

// Font used for displaying text
// on the graphic display
#include <font5x7.h>

// Declare your global variables here

#define DATA_REGISTER_EMPTY (1<<UDRE0)
#define RX_COMPLETE (1<<RXC0)
#define FRAMING_ERROR (1<<FE0)
#define PARITY_ERROR (1<<UPE0)
#define DATA_OVERRUN (1<<DOR0)

// USART0 Receiver buffer
#define RX_BUFFER_SIZE0 64
char rx_buffer0[RX_BUFFER_SIZE0];

#if RX_BUFFER_SIZE0 <= 256
unsigned char rx_wr_index0=0,rx_rd_index0=0;
#else
unsigned int rx_wr_index0=0,rx_rd_index0=0;
#endif

#if RX_BUFFER_SIZE0 < 256
unsigned char rx_counter0=0;
#else
unsigned int rx_counter0=0;
#endif

char buff[260];
int i = 0, time_flow = 0, time_s = 0;
bool module_response = true;

void del_string(unsigned char *s) {
    while (*s) {
        *s = '\0';
        s++;
    }
	i = 0;
}

// This flag is set on USART0 Receiver buffer overflow
bit rx_buffer_overflow0;

// USART0 Receiver interrupt service routine
interrupt [USART0_RXC] void usart0_rx_isr(void) {
    char status,data;
    status=UCSR0A;
    data=UDR0;
    if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0) {
        rx_buffer0[rx_wr_index0++]=data;
#if RX_BUFFER_SIZE0 == 256
        // special case for receiver buffer size=256
        if (++rx_counter0 == 0) rx_buffer_overflow0=1;
#else
        if (rx_wr_index0 == RX_BUFFER_SIZE0) rx_wr_index0=0;
        if (++rx_counter0 == RX_BUFFER_SIZE0) {
            rx_counter0=0;
            rx_buffer_overflow0=1;
        }
#endif
    }
    buff[i] = data;
    i++;
}

#ifndef _DEBUG_TERMINAL_IO_
// Get a character from the USART0 Receiver buffer
#define _ALTERNATE_GETCHAR_
#pragma used+
char getchar(void) {
    char data;
    while (rx_counter0==0);
    data=rx_buffer0[rx_rd_index0++];
#if RX_BUFFER_SIZE0 != 256
    if (rx_rd_index0 == RX_BUFFER_SIZE0) rx_rd_index0=0;
#endif
#asm("cli")
    --rx_counter0;
#asm("sei")
    return data;
}
#pragma used-
#endif

// USART0 Transmitter buffer
#define TX_BUFFER_SIZE0 64
char tx_buffer0[TX_BUFFER_SIZE0];

#if TX_BUFFER_SIZE0 <= 256
unsigned char tx_wr_index0=0,tx_rd_index0=0;
#else
unsigned int tx_wr_index0=0,tx_rd_index0=0;
#endif

#if TX_BUFFER_SIZE0 < 256
unsigned char tx_counter0=0;
#else
unsigned int tx_counter0=0;
#endif

// USART0 Transmitter interrupt service routine
interrupt [USART0_TXC] void usart0_tx_isr(void) {
    if (tx_counter0) {
        --tx_counter0;
        UDR0=tx_buffer0[tx_rd_index0++];
#if TX_BUFFER_SIZE0 != 256
        if (tx_rd_index0 == TX_BUFFER_SIZE0) tx_rd_index0=0;
#endif
    }
}

interrupt [TIM0_OVF] void timer0_ovf_isr(void) {
	//0.02 ms => 50k = 1s.
	TCNT0=0x60;
	// Place your code here
    time_flow++;
	
    if (time_flow == 50000) {
        time_s++;
        time_flow = 0;
    }
}

#ifndef _DEBUG_TERMINAL_IO_
// Write a character to the USART0 Transmitter buffer
#define _ALTERNATE_PUTCHAR_
#pragma used+
void putchar(char c) {
    while (tx_counter0 == TX_BUFFER_SIZE0);
#asm("cli")
    if (tx_counter0 || ((UCSR0A & DATA_REGISTER_EMPTY)==0)) {
        tx_buffer0[tx_wr_index0++]=c;
#if TX_BUFFER_SIZE0 != 256
        if (tx_wr_index0 == TX_BUFFER_SIZE0) tx_wr_index0=0;
#endif
        ++tx_counter0;
    } else
        UDR0=c;
#asm("sei")
}
#pragma used-
#endif

// Standard Input/Output functions
#include <stdio.h>

void put_string (unsigned char *s) {
    while(*s) {
        putchar(*s);
		delay_ms(50);
        s++;
    }
}

void refresh(int time_ms) {
    int j;
	
	delay_ms(time_ms);

    glcd_clear();
    glcd_moveto(0,0);

    glcd_outtext(buff);

    del_string(buff);    

    i = 0;

    

}

void wait_until(unsigned char *keyword, int time_out_s) {
	/* 	deu biet cai temp2 de lam gi nhung khong co thi no khong chay trong 1 so truong hop @@
		Vi du nhap vao "Hell" thi no se tach thua ra them 2 char. Co the do vi tri o nho. Cha biet @@*/
	char temp[20], temp2[20];
	int i = 0, time_start, time_temp;
	
	if (!module_response) return;
	
	del_string(temp);
	
	while (*keyword) {
		temp[i] = *keyword;
		temp2[i] = temp[i];
		keyword++;
		i++;
	}
	
	time_start = time_s;
	
	while (1) {
		if (time_s < time_start) {
			time_temp = time_s + 60;
			if (time_temp - time_start > time_out_s) {
				glcd_outtext("Timed out\r\n");
				break;
			}
		} else {
			if (time_s - time_start > time_out_s) {
				glcd_outtext("Timed out\r\n");
				break;
			}
		}
		
		if ((strstr(buff, temp)) || (strstr(buff, temp2))) {
			break;
		}
		if (strstr(buff, "ERROR")) {
			glcd_outtext("Error found, attempting to continue..\r\n");
			delay_ms(3000);
			glcd_clear();
			glcd_moveto(0,0);
			break;
			//nen lam them ve cai nay nua
		}
	}
}

void read_and_send(unsigned char *s){
    // Thay nhung ham respones_read bang ham wait_until
    char api_key[20], cmd[] = "GET /update?key=", temp[20], temp2[20];
    int length = 0, i = 0;
	
	memset(api_key, '\0',20);
	memset(temp, '\0',20);
	memset(temp2, '\0',20);
	
	while (*s) {
		temp2[i] = *s;
		api_key[i] = temp2[i];
		s++;
		i++;
	}
	
	do {
	
		put_string("AT+CIPMUX=1");
		delay_ms(300);
		put_string("\r\n");
		wait_until("OK", 2);
		refresh(0);
		
		put_string("AT+CIPSTART=0,\"TCP\",\"api.thingspeak.com\",80");
		delay_ms(300);
		put_string("\r\n");
		wait_until("OK", 20);
		refresh(2000);
			
		glcd_clear();
		glcd_moveto(0,0);
		glcd_outtext("Server connected\r\n");
		
		strcat(cmd, api_key);
		strcat(cmd, "&field1=");
		
		// itoa(station_receive.temp, temp);
		
		itoa(200, temp);
		strcat(cmd, temp);
		
		strcat(cmd, "&field2=");
		// itoa(station_receive.humi, temp);
		itoa(200, temp);
		strcat(cmd, temp);
	   
		strcat(cmd, "&field3=");
		// itoa(station_receive.water, temp);
		itoa(200, temp);
		strcat(cmd, temp);
		
		strcat(cmd, "\r\n");
		
		length = strlen(cmd);
		
		itoa(length, temp);	
		
		glcd_outtext("Sending..\r\n");
		put_string("AT+CIPSEND=0,");
		put_string(temp); 
		delay_ms(1000);
		put_string("\r\n");
		
		wait_until("> ", 5);
		
		glcd_clear();
		glcd_moveto(0,0);
		glcd_outtext(cmd);
		
		memset(buff, '\0', 260);
		
		put_string(cmd);
		delay_ms(1000);
		putchar(0x1A);
		
		delay_ms(1000);
	while(!strstr(buff, "IPD")); //IPD: khi gui du lieu xong va nhan ve data tu server. Neu co IDP => da gui thanh cong
	
	put_string("AT+CIPCLOSE\r\n");
	wait_until("OK",5);
}

void main(void) {

    char temp;
	
	{
// Declare your local variables here
// Variable used to store graphic display
// controller initialization data
    GLCDINIT_t glcd_init_data;

// Input/Output Ports initialization
// Port A initialization
// Function: Bit7=In Bit6=Out Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRA=(0<<DDA7) | (1<<DDA6) | (0<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (0<<DDA0);
// State: Bit7=T Bit6=1 Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTA=(0<<PORTA7) | (0<<PORTA6) | (0<<PORTA5) | (0<<PORTA4) | (0<<PORTA3) | (0<<PORTA2) | (0<<PORTA1) | (0<<PORTA0);

// Port B initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRB=(0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (1<<DDB3) | (1<<DDB2) | (0<<DDB1) | (0<<DDB0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTB=(0<<PORTB7) | (0<<PORTB6) | (0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3) | (1<<PORTB2) | (0<<PORTB1) | (0<<PORTB0);

// Port C initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRC=(0<<DDC7) | (0<<DDC6) | (0<<DDC5) | (0<<DDC4) | (0<<DDC3) | (0<<DDC2) | (0<<DDC1) | (0<<DDC0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTC=(0<<PORTC7) | (0<<PORTC6) | (0<<PORTC5) | (0<<PORTC4) | (0<<PORTC3) | (0<<PORTC2) | (0<<PORTC1) | (0<<PORTC0);

// Port D initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRD=(0<<DDD7) | (0<<DDD6) | (0<<DDD5) | (0<<DDD4) | (0<<DDD3) | (0<<DDD2) | (0<<DDD1) | (0<<DDD0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTD=(0<<PORTD7) | (0<<PORTD6) | (0<<PORTD5) | (0<<PORTD4) | (0<<PORTD3) | (0<<PORTD2) | (0<<PORTD1) | (0<<PORTD0);

// Port E initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRE=(0<<DDE7) | (0<<DDE6) | (0<<DDE5) | (0<<DDE4) | (0<<DDE3) | (0<<DDE2) | (0<<DDE1) | (0<<DDE0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTE=(0<<PORTE7) | (0<<PORTE6) | (0<<PORTE5) | (0<<PORTE4) | (0<<PORTE3) | (0<<PORTE2) | (0<<PORTE1) | (0<<PORTE0);

// Port F initialization
// Function: Bit7=In Bit6=In Bit5=In Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRF=(0<<DDF7) | (0<<DDF6) | (0<<DDF5) | (0<<DDF4) | (0<<DDF3) | (0<<DDF2) | (0<<DDF1) | (0<<DDF0);
// State: Bit7=T Bit6=T Bit5=T Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTF=(0<<PORTF7) | (0<<PORTF6) | (0<<PORTF5) | (0<<PORTF4) | (0<<PORTF3) | (0<<PORTF2) | (0<<PORTF1) | (0<<PORTF0);

// Port G initialization
// Function: Bit4=In Bit3=In Bit2=In Bit1=In Bit0=In
    DDRG=(0<<DDG4) | (0<<DDG3) | (0<<DDG2) | (0<<DDG1) | (0<<DDG0);
// State: Bit4=T Bit3=T Bit2=T Bit1=T Bit0=T
    PORTG=(0<<PORTG4) | (0<<PORTG3) | (0<<PORTG2) | (0<<PORTG1) | (0<<PORTG0);

// Timer/Counter 0 initialization
// Clock source: System Clock
// Clock value: 8000.000 kHz
// Mode: Normal top=0xFF
// OC0 output: Disconnected
// Timer Period: 0.02 ms

        ASSR=0<<AS0;
        TCCR0=(0<<WGM00) | (0<<COM01) | (0<<COM00) | (0<<WGM01) | (0<<CS02) | (0<<CS01) | (1<<CS00);
        TCNT0=0x60;
        OCR0=0x00;

// Timer/Counter 1 initialization
// Clock source: System Clock
// Clock value: Timer1 Stopped
// Mode: Normal top=0xFFFF
// OC1A output: Disconnected
// OC1B output: Disconnected
// OC1C output: Disconnected
// Noise Canceler: Off
// Input Capture on Falling Edge
// Timer1 Overflow Interrupt: Off
// Input Capture Interrupt: Off
// Compare A Match Interrupt: Off
// Compare B Match Interrupt: Off
// Compare C Match Interrupt: Off
    TCCR1A=(0<<COM1A1) | (0<<COM1A0) | (0<<COM1B1) | (0<<COM1B0) | (0<<COM1C1) | (0<<COM1C0) | (0<<WGM11) | (0<<WGM10);
    TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (0<<WGM12) | (0<<CS12) | (0<<CS11) | (0<<CS10);
    TCNT1H=0x00;
    TCNT1L=0x00;
    ICR1H=0x00;
    ICR1L=0x00;
    OCR1AH=0x00;
    OCR1AL=0x00;
    OCR1BH=0x00;
    OCR1BL=0x00;
    OCR1CH=0x00;
    OCR1CL=0x00;

// Timer/Counter 2 initialization
// Clock source: System Clock
// Clock value: Timer2 Stopped
// Mode: Normal top=0xFF
// OC2 output: Disconnected
    TCCR2=(0<<WGM20) | (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<CS22) | (0<<CS21) | (0<<CS20);
    TCNT2=0x00;
    OCR2=0x00;

// Timer/Counter 3 initialization
// Clock source: System Clock
// Clock value: Timer3 Stopped
// Mode: Normal top=0xFFFF
// OC3A output: Disconnected
// OC3B output: Disconnected
// OC3C output: Disconnected
// Noise Canceler: Off
// Input Capture on Falling Edge
// Timer3 Overflow Interrupt: Off
// Input Capture Interrupt: Off
// Compare A Match Interrupt: Off
// Compare B Match Interrupt: Off
// Compare C Match Interrupt: Off
    TCCR3A=(0<<COM3A1) | (0<<COM3A0) | (0<<COM3B1) | (0<<COM3B0) | (0<<COM3C1) | (0<<COM3C0) | (0<<WGM31) | (0<<WGM30);
    TCCR3B=(0<<ICNC3) | (0<<ICES3) | (0<<WGM33) | (0<<WGM32) | (0<<CS32) | (0<<CS31) | (0<<CS30);
    TCNT3H=0x00;
    TCNT3L=0x00;
    ICR3H=0x00;
    ICR3L=0x00;
    OCR3AH=0x00;
    OCR3AL=0x00;
    OCR3BH=0x00;
    OCR3BL=0x00;
    OCR3CH=0x00;
    OCR3CL=0x00;

// Timer(s)/Counter(s) Interrupt(s) initialization
    TIMSK=(0<<OCIE2) | (0<<TOIE2) | (0<<TICIE1) | (0<<OCIE1A) | (0<<OCIE1B) | (0<<TOIE1) | (0<<OCIE0) | (0<<TOIE0);
    ETIMSK=(0<<TICIE3) | (0<<OCIE3A) | (0<<OCIE3B) | (0<<TOIE3) | (0<<OCIE3C) | (0<<OCIE1C);

// External Interrupt(s) initialization
// INT0: Off
// INT1: Off
// INT2: Off
// INT3: Off
// INT4: Off
// INT5: Off
// INT6: Off
// INT7: Off
    EICRA=(0<<ISC31) | (0<<ISC30) | (0<<ISC21) | (0<<ISC20) | (0<<ISC11) | (0<<ISC10) | (0<<ISC01) | (0<<ISC00);
    EICRB=(0<<ISC71) | (0<<ISC70) | (0<<ISC61) | (0<<ISC60) | (0<<ISC51) | (0<<ISC50) | (0<<ISC41) | (0<<ISC40);
    EIMSK=(0<<INT7) | (0<<INT6) | (0<<INT5) | (0<<INT4) | (0<<INT3) | (0<<INT2) | (0<<INT1) | (0<<INT0);

// USART0 initialization
// Communication Parameters: 8 Data, 1 Stop, No Parity
// USART0 Receiver: On
// USART0 Transmitter: On
// USART0 Mode: Asynchronous
// USART0 Baud Rate: 9600
    UCSR0A=(0<<RXC0) | (0<<TXC0) | (0<<UDRE0) | (0<<FE0) | (0<<DOR0) | (0<<UPE0) | (0<<U2X0) | (0<<MPCM0);
    UCSR0B=(1<<RXCIE0) | (1<<TXCIE0) | (0<<UDRIE0) | (1<<RXEN0) | (1<<TXEN0) | (0<<UCSZ02) | (0<<RXB80) | (0<<TXB80);
    UCSR0C=(0<<UMSEL0) | (0<<UPM01) | (0<<UPM00) | (0<<USBS0) | (1<<UCSZ01) | (1<<UCSZ00) | (0<<UCPOL0);
    UBRR0H=0x00;
    UBRR0L=0x33;

// USART1 initialization
// USART1 disabled
    UCSR1B=(0<<RXCIE1) | (0<<TXCIE1) | (0<<UDRIE1) | (0<<RXEN1) | (0<<TXEN1) | (0<<UCSZ12) | (0<<RXB81) | (0<<TXB81);

// Analog Comparator initialization
// Analog Comparator: Off
// The Analog Comparator's positive input is
// connected to the AIN0 pin
// The Analog Comparator's negative input is
// connected to the AIN1 pin
    ACSR=(1<<ACD) | (0<<ACBG) | (0<<ACO) | (0<<ACI) | (0<<ACIE) | (0<<ACIC) | (0<<ACIS1) | (0<<ACIS0);
    SFIOR=(0<<ACME);

// ADC initialization
// ADC disabled
    ADCSRA=(0<<ADEN) | (0<<ADSC) | (0<<ADFR) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (0<<ADPS1) | (0<<ADPS0);

// SPI initialization
// SPI disabled
    SPCR=(0<<SPIE) | (0<<SPE) | (0<<DORD) | (0<<MSTR) | (0<<CPOL) | (0<<CPHA) | (0<<SPR1) | (0<<SPR0);

// TWI initialization
// TWI disabled
    TWCR=(0<<TWEA) | (0<<TWSTA) | (0<<TWSTO) | (0<<TWEN) | (0<<TWIE);

// Graphic Display Controller initialization
// The PCD8544 connections are specified in the
// Project|Configure|C Compiler|Libraries|Graphic Display menu:
// SDIN - PORTC Bit 7
// SCLK - PORTA Bit 7
// D /C - PORTC Bit 6
// /SCE - PORTC Bit 5
// /RES - PORTC Bit 4

// Specify the current font for displaying text
    glcd_init_data.font=font5x7;
// No function is used for reading
// image data from external memory
    glcd_init_data.readxmem=NULL;
// No function is used for writing
// image data to external memory
    glcd_init_data.writexmem=NULL;
// Set the LCD temperature coefficient
    glcd_init_data.temp_coef=90;
// Set the LCD bias
    glcd_init_data.bias=3;
// Set the LCD contrast control voltage VLCD
    glcd_init_data.vlcd=55;

    glcd_init(&glcd_init_data);

// Global enable interrupts
#asm("sei")  
	}
	
	glcd_outtext("Booting..\r\n");

	delay_ms(10000);
	
	//reset mang buff ve rong
	memset(buff, '\0', 260);
	
	put_string("AT\r\n");
	delay_ms(2000);
	
	//sau 2s, neu trong buff co gi do => ESP8266 da tra loi => co the chay duoc. Neu trong buff rong => ESP8266 khong tra ve gi => bo qua khong chay
	if (buff[0] != '\0') {
		put_string("AT+CIPSTATUS\r\n");
		wait_until("OK", 2);
		refresh(0);
		
		if (strstr(buff, "STATUS:2")) {
			glcd_outtext("Wifi Connected\r\n");
		} else {
			put_string("AT+CWMODE=1\r\n");
			wait_until("OK", 2);
			refresh(0);
			
			put_string("AT+CWJAP=\"Thay_Thao_deo_giai\",\"chinhxac\"\r\n");
			wait_until("OK", 10);
			refresh(0);
			
			glcd_outtext("Wifi Connected\r\n");
		}
		
		read_and_send("MKUPZUITP8BJB6VB");
	} else {
		module_response = false;
	}
	
	
}
