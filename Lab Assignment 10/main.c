#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

volatile uint8_t temp_current; // Creates a variable that will store each present value of the temperature of the system
volatile uint8_t old_temp = 22; // Creates a variable that will store each previous value of the temperature of the system
volatile int counter = 0; // Creates a counter variable that will determine the index at which each temperature value should be stored in its corresponding array
volatile int first = 0; // Creates a boolean variable that will allow the MSP to transmit a 0x00 bit to the slave only at the beginning of data collection
volatile int counter_print = 0; // Creates a counter variable that assists in allowing the temperature values to print every one second
const int K_p = 50; // Proportional constant for PID control
const int K_i = 5; // Integral constant for PID control
const int K_d = 0; // Derivative constant for PID control
const int temp_desired = 32; // Ambient temperature of 22 degrees C causing T2 to be equal to 10 degrees C
const int time_step = 0.1; // Period of the timer for PID control
volatile float error_temp = 0; // Creates a variable that contains the difference between the current temperature of the system and the desired steady-state temperature of the system
volatile float error_int = 0; // Creates a variable that contains the current integral of the system at each time step for the PID control
volatile float old_error_int = 0; // Creates a variable that contains the previous integral of the system at each time step for the PID control
volatile float error_deriv = 0; // Creates a variable that contains the derivative of the system at each time step for the PID control
volatile float prop_err = 0; // Creates a variable that contains the proportional error for the PID control
volatile float integ_err = 0; // Creates a variable that contains the integral error for the PID control
volatile float deriv_err = 0; // Creates a variable that contains the derivative error for the PID control
volatile float duty_cycle = 0; // Initializes the duty cycle variable for the PID control

#define timerA_divider   TIMER_A_CLOCKSOURCE_DIVIDER_64   // Means counter is incremented at 3E+6/64 = 46875 Hz
#define timerA_period    4687.5 // Timer configured at 10 Hz

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
    TA0CCR0 = 1000;                           // PWM Period
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
        I2C_masterSendSingleByte(EUSCI_B1_BASE, 0x00); // Transmit from Master 0x00 to access the temperature register of the temperature sensor
        first = 1; // Ensures that the MSP doesn't go into transmit mode for the remainder of the program
    }

    I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_RECEIVE_MODE); // Set Master in receive mode
    temp_current = I2C_masterReceiveSingleByte(EUSCI_B1_BASE); // Receives the current temperature of the system at each time step

    error_temp = temp_desired - temp_current; // Calculates the difference between the new temperature and the desired temperature at each time step

    /* Anti-Windup Method */
    if(((duty_cycle == 100) || (duty_cycle == 0)) && (error_temp * error_int > 0))
    {
        error_int = old_error_int; // Clamp error
    }
    else
    {
        error_int = old_error_int + ((error_temp) * time_step); // Calculates the total integral value of the system at each time step
    }
    error_deriv = (temp_current - old_temp)/(time_step); // Calculates the instantaneous derivative of the system at each time step

    prop_err = (K_p * error_temp); // Calculates the proportional error
    deriv_err = (K_d * error_deriv); // Calculates the integral error
    integ_err = (K_i * error_int); // Calculates the derivative error

    duty_cycle = duty_cycle + prop_err - deriv_err + integ_err; // Calculates the duty cycle that's used to test for clamping

    // Saturates the duty cycle
    if(duty_cycle > 100) // Determines if the calculated duty cycle is greater than 100%
    {
        duty_cycle = 100; // Sets the higher limit of the duty cycle at 100%
    }
    else if(duty_cycle < 0) // Determines if the calculated duty cycle is less than 0%
    {
        duty_cycle = 0; // Sets the lower limit of the duty cycle at 0%
    }

    TA0CCR1 = (int) (1000 * (duty_cycle/100)); // Applies the new PWM signal
    old_temp = temp_current; // Updates the previous temperature value
    old_error_int = error_int; // Updates the previous integral error value

    counter++; // Increments the counter
    counter_print++; // Increments the printing counter
    if(counter_print == 10) // Determines if one second has passed
    {
        counter_print = 0; // Resets the printing counter
        printf("%d\r\n", temp_current); // Prints the temperature value every second
    }
}
