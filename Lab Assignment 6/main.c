#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

#define MAIN_1_SECTOR_31 0x0003F000 // Defining the location of Sector 31 of Bank 1 of the main flash memory

uint32_t SMCLK_divider = CS_CLOCK_DIVIDER_1;
#define timerA_divider   TIMER_A_CLOCKSOURCE_DIVIDER_64   // Means counter is incremented at 3E+6/64 = 46875 Hz
#define timerA_period    46875
#define pressed          GPIO_INPUT_PIN_LOW
#define not_pressed      GPIO_INPUT_PIN_HIGH

volatile float ADCresult = 0; // Volatile variable that's used to store the instantaneous value of the ADC output at any given time
volatile float temp_arr[30]; // Volatile array that's used to store the values from the temperature sensor
volatile int counter = 0; // Volatile integer that's used to determine the index in the temperature array at which to store each instantaneous value

const Timer_A_UpModeConfig upConfig_0 =
{
     TIMER_A_CLOCKSOURCE_SMCLK,      // Tie Timer A to SMCLK
     timerA_divider,     // Increment counter every 64 clock cycles
     timerA_period,                          // Period of Timer A (this value placed in TAxCCR0)
     TIMER_A_TAIE_INTERRUPT_DISABLE,     // Disable Timer A rollover interrupt
     TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE, // Enable Capture Compare interrupt
     TIMER_A_DO_CLEAR            // Clear counter upon initialization
};

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
    WDT_A_holdTimer(); // Stops watchdog timer
    FPU_enableModule(); // Enables the floating point module

    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION); // Sets up the UART transmitting pin
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5, GPIO_PIN5, GPIO_TERTIARY_MODULE_FUNCTION); // Configures P5.5 as the input pin for the ADC Module
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1); // Configures Switch S1 to be an input button
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN4); // Configures Switch S2 to be an input button

    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0); // Configures LED1 to be an output
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns off LED1
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN1); // Configures the green LED of LED2 to be an output
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN1); // Turns off the green LED of LED2
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN2); // Configures the blue LED of LED2 to be an output
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns off the blue LED of LED2

    int mode = -1; // Creates an integer whose value represents whether the data is written to flash memory, whether the data is read from flash memory, or neither
    /*
    mode == -1: Do nothing
    mode == 0: Write data to flash memory
    mode == 1: Read data from flash memory
    */

    // Setting up the clock:
    CS_setDCOFrequency(3E+6); // Sets the DCO Frequency
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, SMCLK_divider); // Initializes the clock signal

    Interrupt_disableMaster(); // Disables all interrupts in NVIC
    UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears all interrupt flags in the IFG register
    UART_initModule(EUSCI_A0_BASE, &UART_init); // Initializes the UART Module
    UART_enableModule(EUSCI_A0_BASE); // Enables the UART Module

    ADC14_enableModule(); // Enables the ADC Module
    ADC14_setResolution(ADC_10BIT); // Sets the resolution of the ADC Module
    ADC14_initModule(ADC_CLOCKSOURCE_SMCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1, false); // Initializes the ADC Module
    ADC14_configureSingleSampleMode(ADC_MEM0, false); // Configures the single sample mode of the ADC

    // Sets and enables the voltage range from 0V to 2.5V
    REF_A_setReferenceVoltage(REF_A_VREF2_5V);
    REF_A_enableReferenceVoltage();

    ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_INTBUF_VREFNEG_VSS, ADC_INPUT_A0, false); // Configures the conversion memory of the ADC module
    ADC14_enableSampleTimer(ADC_MANUAL_ITERATION); // Enables each conversion to occur manually
    Timer_A_configureUpMode(TIMER_A0_BASE, &upConfig_0);  // Configure Timer A using above structure

    // Starts the conversion
    ADC14_enableConversion();
    ADC14_toggleConversionTrigger();

    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0); // Configures LED1 as an output
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns off LED1

    unsigned int usiButton1 = not_pressed; // Initializes the current input value of Switch S1
    unsigned int usiButton1_prev = not_pressed;  // Initializes the previous input value of Switch S1
    unsigned int usiButton2 = not_pressed; // Initializes the input value of Switch S2
    unsigned int usiButton2_prev = not_pressed; // Initializes the previous input value of Switch S2

    while(1)
    {
        usiButton1 = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN1); // Receives the state of Switch S1
        usiButton2 = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4); // Receives the state of Switch S2

        if((usiButton1 == not_pressed) & (usiButton1_prev == pressed)) // Determines if Switch S1 had just been released
        {
            mode = 0;
        }
        else if((usiButton2 == not_pressed) & (usiButton2_prev == pressed)) // Determines if Switch S2 had just been released
        {
            mode = 1;
        }
        else
        {
            mode = -1;
        }

        if(mode == 0)
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN1); // Turns on the green LED of LED2
            Interrupt_enableInterrupt(INT_TA0_0);   // Enable Timer A interrupt
            Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);  // Start Timer A
            Interrupt_enableMaster(); // Enables all interrupts in NVIC

            while(counter != 30) // While loop to store 30 temperature values in the array
            {
                while(ADC14_isBusy()){}; // Allows time to pass for the ADC to finish its conversion
                ADCresult = (uint16_t) ADC14_getResult(ADC_MEM0);  // Obtains the ADC result
                ADC14_toggleConversionTrigger(); // Starts another conversion for the ADC
            }

            Interrupt_disableMaster(); // Disables all interrupts in NVIC
            counter = 0; // Resets the value of the counter

            FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, FLASH_SECTOR31); // Unprotects Sector 31 of Bank 1 of the main flash memory
            FlashCtl_eraseSector(MAIN_1_SECTOR_31); // Erases Sector 31 of Bank 1 of the main flash memory
            FlashCtl_programMemory(temp_arr, (void*) MAIN_1_SECTOR_31, 120); // Places the temperature values in Sector 31 of Bank 1 of the main flash memory
            FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1, FLASH_SECTOR31); // Protects Sector 31 of Bank 1 of the main flash memory

            GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns LED1 on to signify that the data has been collected and stored in the main flash memory
            GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN1); // Turns off the green LED of LED2
        }
        else if(mode == 1)
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns on the blue LED of LED2
            float *temp_arr_flash = (float*) MAIN_1_SECTOR_31; // Receives the temperature data from Sector 31 of Bank 1 of the main flash memory
            char temp_ascii[10]; // Creates a character array that will store the corresponding ASCII values of each temperature value
            uint8_t numchar = 0; // Creates a variable that will store the number of characters of each temperature value
            int i; // Initializes the integer variable that's used in the for loop directly below
            for(i = 0; i < 30; i++) // for loop used to send each temperature value to the terminal
            {
                numchar = sprintf(temp_ascii, "%.1f", temp_arr_flash[i]); // Stores each temperature value in its ASCII form in the character array

                int j; // Initializes the integer variable that's used in the for loop directly below
                for(j = 0; j < numchar; j++) // for loop used to transmit each character of each temperature value to the terminal
                {
                    while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                    UART_transmitData(EUSCI_A0_BASE, temp_ascii[j]); // Transmits the data one character at a time
                }

                while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                UART_transmitData(EUSCI_A0_BASE, 0x0D); // Write carriage return '\r'

                while((UCA0IFG & 0x0002) == 0x0000){} ; // Allows time for the TXIFG bit to become set
                UART_transmitData(EUSCI_A0_BASE, 0x0A); // Write newline '\n'
            }

            GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns LED1 off to signify that all of the data in the main flash memory has been transmitted to the terminal
            GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns off the blue LED of LED2
        }

        usiButton1_prev = usiButton1; // Setting the value of usiButton1_prev to equal the previous state of Switch S1
        usiButton2_prev = usiButton2; // Setting the value of usiButton2_prev to equal the previous state of Switch S2
    }
}

void TA0_0_IRQHandler()
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clear the timer A0 interrupt
    ADCresult = 2.5*ADCresult/1023 * (60.28454304); // Converts the ADC value to its corresponding temperature value in Fahrenheit
    temp_arr[counter] = ADCresult; // Stores each temperature value in the array containing all 30 temperature values
    printf("Temperature Reading #%i: %f\r\n", (counter+1), ADCresult); // Prints the temperature values obtained by the ADC
    counter++; // Increments the counter by 1
}


