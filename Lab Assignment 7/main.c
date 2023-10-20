#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

uint8_t time_arr[300]; // Creates an empty character array that's used to store the input starting time values
volatile int ind = 0; // Creates a volatile integer that would track the index within the time array at which the next byte needs to be received
volatile int mode = 0; // Creates a volatile integer that either takes on a value of 0 or 1.  If it's equal to zero, the program requests a specific number of seconds; whereas, if it's equal to one, the program moves the motor and uses it as an egg timer
volatile int start = 0; // Creates a boolean that represents whether or not the user wants to start the egg timer
volatile float time_sec = 0; // Creates a volatile floating point number that will eventually store the number of seconds inputted by the user
volatile int ticks; // Creates a volatile integer that stores the total number of ticks the motor must move in order to reach its starting position and in order to reach back to the home position for each use of the timer
volatile int counter1 = 0; // Creates a volatile integer that stores the number of ticks that the motor has moved counterclockwise
volatile int counter2 = 0; // Creates a volatile integer that stores the number of ticks that the motor has moved clockwise
volatile int intro = 0; // Creates a volatile integer that determines if the question requesting the number of seconds that the user should input was prompted to the terminal

#define timerA_divider   TIMER_A_CLOCKSOURCE_DIVIDER_64   // Means counter is incremented at 3E+6/64 = 46875 Hz
#define timerA_period    1000
#define timerA_period2   5493.164063 // Defined such that the motor moves clockwise at a rate of 1 degree per second (46875/5493.164063 ticks/sec)*(360 deg/512 ticks) = 6 deg/sec

// Timer for moving the motor counterclockwise
const Timer_A_UpModeConfig upConfig_0 =
{
     TIMER_A_CLOCKSOURCE_SMCLK,      // Tie Timer A to SMCLK
     timerA_divider,     // Increment counter every 64 clock cycles
     timerA_period,                          // Period of Timer A (this value placed in TAxCCR0)
     TIMER_A_TAIE_INTERRUPT_DISABLE,     // Disable Timer A rollover interrupt
     TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE, // Enable Capture Compare interrupt
     TIMER_A_DO_CLEAR            // Clear counter upon initialization
};


// Timer for moving the motor clockwise
const Timer_A_UpModeConfig upConfig_1 =
{
     TIMER_A_CLOCKSOURCE_SMCLK,      // Tie Timer A to SMCLK
     timerA_divider,     // Increment counter every 64 clock cycles
     timerA_period2,                          // Period of Timer A (this value placed in TAxCCR0)
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

    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION); // Sets up the UART receiving pin
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION); // Sets up the UART transmitting pin

    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7); // Sets Pins 2.4 to 2.7 as output pins
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7); // Sets the output signal on Pins 2.4 to 2.7 to low

    // Setting up the clock:
    unsigned int dcoFrequency = 3E+6; // Initializes the DCO Frequency
    CS_setDCOFrequency(dcoFrequency); // Sets the DCO Frequency
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1); // Initializes the clock signal

    Interrupt_disableMaster(); // Disables all interrupts in NVIC
    UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears all interrupt flags in the IFG register
    UART_initModule(EUSCI_A0_BASE, &UART_init); // Initializes the UART Module
    UART_enableModule(EUSCI_A0_BASE); // Enables the UART Module
    UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT); // Enables the interrupt
    Interrupt_enableInterrupt(INT_EUSCIA0); // Enables the corresponding interrupt

    Timer_A_configureUpMode(TIMER_A0_BASE, &upConfig_0); // Configure Timer A0 using above structure
    Timer_A_configureUpMode(TIMER_A1_BASE, &upConfig_1); // Configure Timer A1 using above structure
    Interrupt_enableMaster(); // Enables all interrupts in NVIC

    while(1) // Infinite while loop for the program to run indefinitely
    {
        if(mode == 0)
        {
            Interrupt_disableInterrupt(INT_TA0_0); // Disables the Timer A0 interrupt
            Interrupt_disableInterrupt(INT_TA1_0); // Disables the Timer A1 interrupt
            Interrupt_enableInterrupt(INT_EUSCIA0); // Enables the corresponding interrupt
            if(intro == 0) // Determines if the introduction was already prompted to the terminal
            {
                char seconds_ascii[100]; // Creates an empty character array that will eventually contain each character of the following string
                uint8_t numchar = 0; // Creates a variable that will eventually store the number of characters in the following string

                numchar = sprintf(seconds_ascii, "Please enter number of seconds: "); // Stores each character of the string into the seconds_ascii character array and stores the number of characters of the string in numchar
                int k; // Initializes the integer variable that's used in the for loop directly below
                for(k = 0; k < numchar; k++) // for loop used to transmit each character of each temperature value to the terminal
                {
                    while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                    UART_transmitData(EUSCI_A0_BASE, seconds_ascii[k]); // Transmits the data one character at a time
                }
                intro = 1; // Indicates that the introduction had already been prompted to the terminal
            }
        }
        else if(mode == 1) // Mode that includes moving the motors
        {
            Interrupt_disableMaster(); // Disables all interrupts in NVIC
            Interrupt_enableInterrupt(INT_TA0_0); // Enable Timer A0 interrupt
            Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);  // Start Timer A0
            Interrupt_enableMaster(); // Enables all interrupts
            Interrupt_disableInterrupt(INT_EUSCIA0); // Disables the UART interrupt
            Interrupt_disableInterrupt(INT_TA1_0); // Disables the Timer A1 interrupt

            ticks = (time_sec/60)*512; // Calculates the integer number of ticks required to move the motor to the starting position
            while(counter1 < ticks){}; // Infinite while loop that stalls the program as it waits for the motor to rotate to its specified position
            Interrupt_disableInterrupt(INT_TA0_0); // Disables the Timer A0 interrupt

            Interrupt_enableInterrupt(INT_EUSCIA0); // Enables the UART interrupt

            // The next several lines of code simply prints "Start?" to the terminal
            char start_ascii[10];
            uint8_t numchar;
            numchar = sprintf(start_ascii, "Start?");
            int m; // Initializes the integer variable that's used in the for loop directly below
            for(m = 0; m < numchar; m++) // for loop used to transmit each character of each temperature value to the terminal
            {
                while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                UART_transmitData(EUSCI_A0_BASE, start_ascii[m]); // Transmits the data one character at a time
            }
            while(start == 0){}; // Infinite while loop that stalls the program as it waits for the user to start the motor timer

            Interrupt_disableMaster(); // Disables all interrupts in NVIC
            Interrupt_enableInterrupt(INT_TA1_0); // Re-enables the TimerA Interrupt in order for the motor to rotate back to its home position
            Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);  // Start Timer A
            Interrupt_enableMaster(); // Enables all interrupts
            Interrupt_disableInterrupt(INT_EUSCIA0); // Disables the UART interrupt
            Interrupt_disableInterrupt(INT_TA0_0); // Disables the Timer A interrupt

            while(counter2 < ticks){}; // Infinite while loop that stalls the program as it waits for the motor to rotate back to its home position

            ind = 0; // Resets the index of the character array to 0
            mode = 0; // Resets the mode back to 0
            start = 0; // Resets the start boolean to 0
            time_sec = 0; // Resets the input amount of time to 0
            ticks = 0; // Resets the number of ticks to 0
            counter1 = 0; // Resets counter1 to 0
            counter2 = 0; // Resets counter2 to 0
            intro = 0; // Resets the intro boolean
        }
    }
}

void TA0_0_IRQHandler()
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clears the Timer A0 interrupt

    // Moves the motor in a counterclockwise direction:
    if((counter1%4) == 0) // Moves coil A1
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN4);
    }
    else if((counter1%4) == 1) // Moves coil B1
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN6|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN5);
    }
    else if((counter1%4) == 2) // Moves coil A2
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN6);
    }
    else if((counter1%4) == 3) // Moves coil B2
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN6);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN7);
    }

    counter1++; // Increments the counter by 1
}

void TA1_0_IRQHandler()
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clears the Timer A1 interrupt

    // Moves the motor in a counterclockwise direction:
    if((counter2%4) == 3) // Moves coil A1
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN4);
    }
    else if((counter2%4) == 2) // Moves coil B1
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN6|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN5);
    }
    else if((counter2%4) == 1) // Moves coil A2
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN7);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN6);
    }
    else if((counter2%4) == 0) // Moves coil B2
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN4|GPIO_PIN5|GPIO_PIN6);
        GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN7);
    }

    counter2++; // Increments the counter by 1
}

void EUSCIA0_IRQHandler(void)
{
    if(mode == 0)
    {
        uint8_t char_cur; // Initializes a uint8_t variable that stores each character received from the terminal
        char_cur = UART_receiveData(EUSCI_A0_BASE); // Receives a character from the terminal

        if(char_cur != 0x0D) // Determines if a carriage return has been received
        {
            time_arr[ind] = char_cur; // Stores the character in the character array
            ind++; // Increments the index in the character array by 1
        }
        else
        {
            // The following lines of code are utilized to convert the number of seconds in ASCII form to a floating point number form
            if(ind == 1) // If the number of digits entered is equal to 1
            {
                char first_dig = time_arr[0]; // Receives the first digit in the array
                time_sec = first_dig - '0'; // Calculates the number of seconds in floating point number form
            }
            else if(ind == 2) // If the number of digits entered is equal to 2
            {
                char first_dig = time_arr[0]; // Receives the first digit in the array
                char second_dig = time_arr[1]; // Receives the second digit in the array
                float first_digit = first_dig - '0'; // Converts the first digit to floating point number form
                float second_digit = second_dig - '0'; // Converts the second digit to floating point number form

                time_sec = (10 * first_digit) + second_digit; // Calculates the number of seconds in floating point number form
            }

            int j; // Initializes the integer used in the for loop
            for(j = 0; j < ind; j++) // for loop used to print out all characters in the buffer
            {
                while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                UART_transmitData(EUSCI_A0_BASE, time_arr[j]); // Transmits the data one character at a time
            }

            // The following lines of code are used to print out the inputted number of seconds back to the terminal:
            char seconds_ascii[100];
            uint8_t numchar = 0;

            numchar = sprintf(seconds_ascii, " seconds");
            int k; // Initializes the integer variable that's used in the for loop directly below
            for(k = 0; k < numchar; k++) // for loop used to transmit each character of each temperature value to the terminal
            {
                while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
                UART_transmitData(EUSCI_A0_BASE, seconds_ascii[k]); // Transmits the data one character at a time
            }

            // End the line in terminal window
            while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
            UART_transmitData(EUSCI_A0_BASE, 0x0D); // Write carriage return '\r'

            while((UCA0IFG & 0x0002) == 0x0000){} ; // Allows time for the TXIFG bit to become set
            UART_transmitData(EUSCI_A0_BASE, 0x0A); // Write newline '\n'

            mode = 1; // Turns the mode of the program to mode 1
            UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears the interrupt flag in order to resume the main method
        }
    }
    else if(mode == 1)
    {
        uint8_t char_cur; // Initializes a uint8_t variable that stores each character received from the terminal
        char_cur = UART_receiveData(EUSCI_A0_BASE); // Receives a character from the terminal
        if(char_cur == 0x0D) // Determines if a carriage return has been received
        {
            start = 1; // Sets the start boolean equal to 1 in order to indicate that the user has requested to start the timer

            // End the line in terminal window
            while((UCA0IFG & 0x0002) == 0x0000){}; // Allows time for the TXIFG bit to become set
            UART_transmitData(EUSCI_A0_BASE, 0x0D); // Write carriage return '\r'

            while((UCA0IFG & 0x0002) == 0x0000){} ; // Allows time for the TXIFG bit to become set
            UART_transmitData(EUSCI_A0_BASE, 0x0A); // Write newline '\n'

            UART_clearInterruptFlag(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG); // Clears the interrupt flag in order to resume the main method
        }
    }
}
