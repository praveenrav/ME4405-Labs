#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

volatile uint8_t temp_arr[3000]; // Creates an empty array that will eventually store the temperature values
volatile uint8_t temp_current; // Creates a variable that will store each present value of the temperature of the system
volatile int counter = 0; // Creates a counter variable that will determine the index at which each temperature value should be stored in its corresponding array
volatile int first = 0; // Creates a boolean variable that will allow the MSP to transmit a 0x00 bit to the slave only at the beginning of data collection

#define timerA_divider   TIMER_A_CLOCKSOURCE_DIVIDER_64   // Means counter is incremented at 3E+6/64 = 46875 Hz
#define timerA_period    46875

/* I2C Master Configuration Parameter */
const eUSCI_I2C_MasterConfig i2cConfig =
{
        EUSCI_B_I2C_CLOCKSOURCE_SMCLK, // SMCLK Clock Source
        3000000, // SMCLK = 3MHz
        EUSCI_B_I2C_SET_DATA_RATE_100KBPS, // Desired I2C Clock of 100khz
        0, // No byte counter threshold
        EUSCI_B_I2C_NO_AUTO_STOP // No Autostop
};

const Timer_A_UpModeConfig upConfig_3 =
{
     TIMER_A_CLOCKSOURCE_SMCLK,      // Tie Timer A to SMCLK
     timerA_divider,     // Increment counter every 64 clock cycles
     timerA_period,                          // Period of Timer A (this value placed in TAxCCR0)
     TIMER_A_TAIE_INTERRUPT_DISABLE,     // Disable Timer A rollover interrupt
     TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE, // Enable Capture Compare interrupt
     TIMER_A_DO_CLEAR            // Clear counter upon initialization
};

void main(void)
{
    WDT_A_holdTimer(); // Stops the watchdog timer
    FPU_enableModule(); // Enables the floating point module
    Interrupt_disableMaster(); // Disables all interrupts in NVIC

    // Setting up the clock:
    unsigned int dcoFrequency = 3E+6; // Initializes the DCO Frequency
    CS_setDCOFrequency(dcoFrequency); // Sets the DCO Frequency
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1); // Initializes the clock signal

    P2SEL0 |= 0x10 ;    // Set bit 4 of P2SEL0 to enable TA0.1 functionality on P2.4
    P2SEL1 &= ~0x10 ;   // Clear bit 4 of P2SEL1 to enable TA0.1 functionality on P2.4
    P2DIR |= 0x10 ;     // Set pin 2.4 as an output pin

    // Configure PWM signal for period of 0.333 ms, (initial) duty cycle of 0%
    TA0CCR0 = 3000;                           // PWM Period
    TA0CCR1 = 0;                            // PWM duty cycle set at 0%
    TA0CCTL1 = OUTMOD_7;                      // Reset/set output mode for Pin 2.4
    TA0CTL = TASSEL__SMCLK | MC__UP | TACLR;  // SMCLK, up mode, clear TAR

    // Enabling the I2C GPIO Pins
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6, GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION); // Initializes SDA
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6, GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION); // Initializes SCL

    I2C_initMaster(EUSCI_B1_BASE, &i2cConfig); // Initializing I2C Master to SMCLK at 100 kbs with no autostop
    I2C_setSlaveAddress(EUSCI_B1_BASE, 0x48); // Specify slave address
    I2C_enableModule(EUSCI_B1_BASE); // Enable I2C Module to start operations

    //Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clear the timer A0 interrupt
    Timer_A_configureUpMode(TIMER_A3_BASE, &upConfig_3);  // Configure Timer A using above structure
    Interrupt_enableInterrupt(INT_TA3_0);   // Enable Timer A interrupt
    Timer_A_startCounter(TIMER_A3_BASE, TIMER_A_UP_MODE);  // Start Timer A
    Interrupt_enableMaster(); // Enables all interrupts

    while(1){}; // Infinite while loop for the program to run indefinitely
}

void TA3_0_IRQHandler()
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clear the timer A0 interrupt

    if(first == 0) // Determines if the program had just been started
    {
        I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_TRANSMIT_MODE); // Set Master in transmit mode
        I2C_masterSendSingleByte(EUSCI_B1_BASE, 0x00); // Transmit from Master 0x00
        first = 1; // Ensures that the MSP doesn't go into transmit mode for the remainder of the program
    }

    I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_RECEIVE_MODE); // Set Master in receive mode


    if(counter == 120) // Determines if 120 seconds have passed
    {
        TA0CCR1 = 1500;                                // Sets the duty cycle to 50%
    }

    temp_current = I2C_masterReceiveSingleByte(EUSCI_B1_BASE); // Receives the current temperature of the system
    temp_arr[counter] = temp_current; // Stores each temperature in the temperature array
    counter++; // Increments the counter
    printf("%d\r\n", temp_current); // Prints each temperature value
}
