#include "msp.h"
#include "driverlib.h"
#include "string.h"

/**
 * main.c
 */


uint8_t info_arr[300]; // Creates a global variable that would store all information received by and sent to the terminal
volatile int ind = 0; // Creates a volatile integer that would track the index within the information array at which the next byte needs to be received or transmitted

// Establish Baud Rate of 57600 for a clock frequency of 3.0 MHz
const eUSCI_UART_Config UART_init =
    {
     EUSCI_A_UART_CLOCKSOURCE_SMCLK,
     3,
     4,
     0,
     EUSCI_A_UART_NO_PARITY,
     EUSCI_A_UART_LSB_FIRST,
     EUSCI_A_UART_ONE_STOP_BIT,
     EUSCI_A_UART_MODE,
     EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION
    };

void main(void)
{
    WDT_A_holdTimer(); // Stops the watchdog timer

    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION); // Sets up the UART receiving pin
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION); // Sets up the UART transmitting pin

    // Setting up the clock:
    unsigned int dcoFrequency = 3E+6;
    CS_setDCOFrequency(dcoFrequency);
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    Interrupt_disableMaster(); // Disables all interrupts in NVIC
    UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears all interrupt flags in the IFG register

    UART_initModule(EUSCI_A0_BASE, &UART_init); // Initializes the UART Module
    UART_enableModule(EUSCI_A0_BASE); // Enables the UART Module

    UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT); // Enables the interrupt
    Interrupt_enableInterrupt(INT_EUSCIA0); // Enables the corresponding interrupt
    Interrupt_enableMaster(); // Enables all interrupts in NVIC

    while(1){} // Infinite loop used to run this program infinitely
}

void EUSCIA0_IRQHandler(void)
{

    uint8_t char_cur; // Initializes a uint8_t variable that stores each character received from the terminal

    char_cur = UART_receiveData(EUSCI_A0_BASE); // Receives a character from the terminal

    if(char_cur != 0x0D) // Determines if a carriage return has been received
    {
        info_arr[ind] = char_cur; // Stores the character in the character array
        ind++; // Increments the index in the character array by 1
    }
    else
    {
        int j; // Initializes the integer used in the for loop
        for(j = 0; j < ind; j++) // for loop used to print out all characters in the buffer
        {
            while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
            UART_transmitData(EUSCI_A0_BASE, info_arr[j]); // Transmits the data one character at a time
        }

        ind = 0; // Resets the index of the character array to 0

        // End the line in terminal window
        while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
        UART_transmitData(EUSCI_A0_BASE, 0x0D); // Write carriage return '\r'

        while((UCA0IFG & 0x0002) == 0x0000){} ; // Allows time for the TXIFG bit to become set
        UART_transmitData(EUSCI_A0_BASE, 0x0A); // Write newline '\n'

        UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears the interrupt flag in order to resume the main method
    }
}

