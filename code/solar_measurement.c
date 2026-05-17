// PIC16F877A Configuration Bit Settings
#pragma config FOSC = HS        // HS oscillator (20 MHz)
#pragma config WDTE = OFF       // Watchdog Timer disabled
#pragma config PWRTE = OFF      // Power-up Timer disabled
#pragma config BOREN = ON       // Brown-out Reset enabled
#pragma config LVP = OFF        // Low Voltage Programming off
#pragma config CPD = OFF        // Data EEPROM Code Protection off
#pragma config WRT = OFF        // Flash Program Memory Write off
#pragma config CP = OFF         // Flash Program Memory Code Protection off

#include <xc.h>
#include <stdio.h>
#include <string.h>

#define _XTAL_FREQ 20000000      // 20 MHz system clock

// LCD pin connections
#define RS RD2
#define EN RD3
#define D4 RD4
#define D5 RD5
#define D6 RD6
#define D7 RD7
#define buz RD1
#define led RD0

#define EEPROM_ADDRESS 0x00      // EEPROM address for max voltage

// Function prototypes
void Lcd_Port(char a);
void Lcd_Cmd(char a);
void Lcd_Clear(void);
void Lcd_Set_Cursor(char a, char b);
void Lcd_Write_Char(char a);
void Lcd_Write_String(char *a);
void FloatToStr(float num, char *str, int decimal_places);
void Display_float(float num, int decimal_places);
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);
void EEPROM_Write(unsigned char address, unsigned char data);
unsigned char EEPROM_Read(unsigned char address);
void UART_Init(void);
void UART_Write(char data);
void UART_Write_Text(char *text);
void GSM_Init(void);
void GSM_SendSMS(char *number, char *message);
void delay_ms(unsigned int ms);

// ========== DELAY FUNCTION ==========
void delay_ms(unsigned int ms) {
    for(unsigned int i = 0; i < ms; i++)
        __delay_ms(1);
}

// ========== LCD 4-BIT MODE FUNCTIONS ==========
void Lcd_Port(char a) {
    if(a & 1) D4 = 1; else D4 = 0;
    if(a & 2) D5 = 1; else D5 = 0;
    if(a & 4) D6 = 1; else D6 = 0;
    if(a & 8) D7 = 1; else D7 = 0;
}

void Lcd_Cmd(char a) {
    RS = 0;                 // Command mode
    Lcd_Port(a);
    EN = 1;
    __delay_us(40);
    EN = 0;
}

void Lcd_Clear(void) {
    Lcd_Cmd(0x01);
    __delay_ms(2);
}

void Lcd_Set_Cursor(char a, char b) {
    char temp, z, y;
    if(a == 1) {
        temp = 0x80 + b - 1;
        z = temp >> 4;
        y = temp & 0x0F;
        Lcd_Cmd(z);
        Lcd_Cmd(y);
    } else if(a == 2) {
        temp = 0xC0 + b - 1;
        z = temp >> 4;
        y = temp & 0x0F;
        Lcd_Cmd(z);
        Lcd_Cmd(y);
    }
}

void Lcd_Write_Char(char a) {
    char temp, y;
    temp = a & 0x0F;
    y = a & 0xF0;
    RS = 1;                 // Data mode
    Lcd_Port(y >> 4);
    EN = 1;
    __delay_us(40);
    EN = 0;
    Lcd_Port(temp);
    EN = 1;
    __delay_us(40);
    EN = 0;
}

void Lcd_Write_String(char *a) {
    for(int i = 0; a[i] != '\0'; i++)
        Lcd_Write_Char(a[i]);
}

// ========== FLOAT TO STRING CONVERSION ==========
void FloatToStr(float num, char *str, int decimal_places) {
    int integer_part = (int)num;
    float fraction_part = num - (float)integer_part;
    int i = 0;
    
    // Convert integer part
    if(integer_part == 0) {
        str[i++] = '0';
    } else {
        char temp[10];
        int idx = 0;
        while(integer_part > 0) {
            temp[idx++] = (integer_part % 10) + '0';
            integer_part /= 10;
        }
        for(int j = idx - 1; j >= 0; j--)
            str[i++] = temp[j];
    }
    
    str[i++] = '.';   // Decimal point
    
    // Convert fractional part
    for(int k = 0; k < decimal_places; k++) {
        fraction_part *= 10;
        int digit = (int)fraction_part;
        str[i++] = digit + '0';
        fraction_part -= digit;
    }
    str[i] = '\0';
}

void Display_float(float num, int decimal_places) {
    char buffer[20];
    FloatToStr(num, buffer, decimal_places);
    Lcd_Write_String(buffer);
}

// ========== ADC FUNCTIONS ==========
void ADC_Init(void) {
    ADCON0 = 0x81;      // Turn on ADC, select channel AN1
    ADCON1 = 0x80;      // Right justified, Fosc/8
}

unsigned int ADC_Read(unsigned char channel) {
    ADCON0 &= 0xC5;     // Clear channel selection bits (11000101 mask)
    ADCON0 |= (channel << 3);  // Select channel
    __delay_ms(2);      // Acquisition time
    GO_nDONE = 1;       // Start conversion
    while(GO_nDONE);    // Wait for completion
    return ((ADRESH << 8) + ADRESL);
}

// ========== EEPROM FUNCTIONS ==========
void EEPROM_Write(unsigned char address, unsigned char data) {
    while(WR);          // Wait for previous write
    EEADR = address;
    EEDATA = data;
    EECON1bits.EEPGD = 0;  // Access data EEPROM
    EECON1bits.WREN = 1;   // Enable writes
    // Required sequence
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;     // Begin write
    EECON1bits.WREN = 0;   // Disable writes
}

unsigned char EEPROM_Read(unsigned char address) {
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    return EEDATA;
}

// ========== UART (GSM) FUNCTIONS ==========
void UART_Init(void) {
    SPBRG = 31;         // 9600 baud for 20 MHz
    BRGH = 1;           // High speed
    SYNC = 0;           // Asynchronous
    SPEN = 1;           // Enable serial port
    TXEN = 1;           // Enable transmission
    CREN = 1;           // Enable reception
    TX9 = 0;            // 8-bit transmission
    RX9 = 0;            // 8-bit reception
}

void UART_Write(char data) {
    while(!TRMT);       // Wait for TX buffer empty
    TXREG = data;
}

void UART_Write_Text(char *text) {
    for(int i = 0; text[i] != '\0'; i++)
        UART_Write(text[i]);
}

void GSM_Init(void) {
    UART_Write_Text("AT\r");
    __delay_ms(1000);
    UART_Write_Text("AT+CMGF=1\r");   // Text mode
    __delay_ms(1000);
}

void GSM_SendSMS(char *number, char *message) {
    UART_Write_Text("AT+CMGS=\"");
    UART_Write_Text(number);
    UART_Write_Text("\"\r");
    __delay_ms(1000);
    UART_Write_Text(message);
    __delay_ms(1000);
    UART_Write(0x1A);      // CTRL+Z to send
}

// ========== MAIN FUNCTION ==========
void main(void) {
    int w = 0;
    unsigned int adc_value_voltage, adc_value_current;
    float voltage, current, max_voltage = 0.0;
    float reference_voltage = 5.0;      // ADC reference
    float sensitivity = 0.185;          // 185 mV/A for ACS712 5A
    
    TRISA = 0xFF;       // PORTA as input
    TRISD = 0x00;       // PORTD as output
    
    ADC_Init();
    
    // LCD Initialization (4-bit mode)
    __delay_ms(15);
    Lcd_Cmd(0x02);      // Initialize in 4-bit mode
    Lcd_Cmd(0x28);      // 2 lines, 5x7 matrix
    Lcd_Cmd(0x0C);      // Display ON, cursor OFF
    Lcd_Cmd(0x06);      // Increment cursor
    Lcd_Clear();
    
    // Test buzzer and LED
    buz = 1; led = 1; __delay_ms(100);
    buz = 0; led = 0; __delay_ms(200);
    buz = 1; led = 1; __delay_ms(100);
    buz = 0; led = 0;
    
    Lcd_Clear();
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String(" Solar Energy");
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String("Measurement System");
    __delay_ms(2000);
    
    Lcd_Clear();
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String(" Embedded");
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String(" Systems");
    __delay_ms(1000);
    
    Lcd_Clear();
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String(" 22UG10387");
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String(" 22UG10883");
    __delay_ms(1000);
    
    // Read stored max voltage from EEPROM
    max_voltage = (float)EEPROM_Read(EEPROM_ADDRESS) / 10.0;
    
    while(1) {
        // ===== READ VOLTAGE (AN1) =====
        adc_value_voltage = ADC_Read(1);
        voltage = (adc_value_voltage * reference_voltage) / 1023.0;
        
        // Voltage divider calculation (R1=30k, R2=7.5k)
        float R1 = 30000.0;
        float R2 = 7500.0;
        float Vin = voltage * (R1 + R2) / R2;
        
        // Update max voltage
        if(Vin > max_voltage) {
            max_voltage = Vin;
            EEPROM_Write(EEPROM_ADDRESS, (unsigned char)(max_voltage * 10));
        }
        
        // ===== READ CURRENT (AN0) =====
        adc_value_current = ADC_Read(0);
        float current_voltage = (adc_value_current * reference_voltage) / 1023.0;
        current = (current_voltage - (reference_voltage / 2.0)) / sensitivity;
        
        // ===== LOW VOLTAGE ALERT =====
        if(Vin < 3.70) {
            Lcd_Clear();
            Lcd_Set_Cursor(2, 1);
            Lcd_Write_String("Low voltage error");
            Lcd_Set_Cursor(1, 1);
            Lcd_Write_String(" Voltage < 3.7 ");
            __delay_ms(1000);
            
            if(w == 0) {
                buz = 1;
                __delay_ms(1000);
                buz = 0;
                UART_Init();
                GSM_Init();
                __delay_ms(5000);      // Wait for GSM to initialize
                GSM_SendSMS("0711099800", "The solar Voltage is less than 3.7V");
                w = 1;
            }
        } else if(Vin > 4.7) {
            w = 0;   // Reset alert flag when voltage recovers
        }
        
        // ===== LED INDICATOR BLINK =====
        led = 1;
        __delay_ms(50);
        led = 0;
        
        // ===== DISPLAY VOLTAGE =====
        Lcd_Clear();
        Lcd_Set_Cursor(1, 1);
        Lcd_Write_String("Voltage : ");
        Lcd_Set_Cursor(1, 12);
        Display_float(Vin, 2);
        Lcd_Set_Cursor(1, 16);
        Lcd_Write_String("V");
        
        // ===== DISPLAY CURRENT =====
        Lcd_Set_Cursor(2, 1);
        Lcd_Write_String("Current : ");
        Lcd_Set_Cursor(2, 11);
        Display_float(current * 1000, 2);   // Convert to mA
        Lcd_Set_Cursor(2, 15);
        Lcd_Write_String("mA");
        __delay_ms(1000);
        
        // ===== LED BLINK AGAIN =====
        led = 1;
        __delay_ms(50);
        led = 0;
        
        // ===== DISPLAY POWER =====
        Lcd_Clear();
        Lcd_Set_Cursor(1, 1);
        Lcd_Write_String("Power : ");
        Lcd_Set_Cursor(1, 11);
        Display_float(Vin * current, 2);
        Lcd_Set_Cursor(1, 16);
        Lcd_Write_String("W");
        __delay_ms(1000);
        
        // ===== LED BLINK =====
        led = 1;
        __delay_ms(50);
        led = 0;
        
        // ===== DISPLAY MAX VOLTAGE =====
        Lcd_Clear();
        Lcd_Set_Cursor(1, 1);
        Lcd_Write_String("Max Voltage:");
        Lcd_Set_Cursor(2, 1);
        Display_float(max_voltage, 2);
        __delay_ms(1000);
        
        // ===== DISPLAY TITLE AGAIN =====
        Lcd_Clear();
        Lcd_Set_Cursor(1, 1);
        Lcd_Write_String(" Solar Energy");
        Lcd_Set_Cursor(2, 1);
        Lcd_Write_String("Measurment System");
        __delay_ms(1000);
    }
}