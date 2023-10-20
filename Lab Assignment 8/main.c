#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

#define pressed          GPIO_INPUT_PIN_LOW
#define not_pressed      GPIO_INPUT_PIN_HIGH
volatile float ADCresult = 0; // Volatile variable that's used to store the instantaneous value of the ADC output at any given time

void main(void)
{
    WDT_A_holdTimer(); // Stops watchdog timer
    FPU_enableModule(); // Enables the floating point module

    P2SEL0 |= 0x10 ;    // Set bit 4 of P2SEL0 to enable TA0.1 functionality on P2.4
    P2SEL1 &= ~0x10 ;   // Clear bit 4 of P2SEL1 to enable TA0.1 functionality on P2.4
    P2DIR |= 0x10 ;     // Set pin 2.4 as an output pin

    P2SEL0 |= 0x20 ;    // Set bit 4 of P2SEL0 to enable TA0.1 functionality on P2.5
    P2SEL1 &= ~0x20 ;   // Clear bit 4 of P2SEL1 to enable TA0.1 functionality on P2.5
    P2DIR |= 0x20;     // Set pin 2.5 as an output pin

    // Setting up the clock:
    unsigned int dcoFrequency = 3E+6; // Initializes the DCO Frequency
    CS_setDCOFrequency(dcoFrequency); // Sets the DCO Frequency
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1); // Initializes the clock signal

    // Configure PWM signal for period of 0.333 ms, (initial) duty cycle of 20%
    TA0CCR0 = 10000;                           // PWM Period
    TA0CCR1 = 0;                            // PWM duty cycle for one Timer A module
    TA0CCR2 = 0;                            // PwM duty cycle for another Timer A module
    TA0CCTL1 = OUTMOD_7;                      // Reset/set output mode for Pin 2.4
    TA0CCTL2 = OUTMOD_7;                      // Reset/set output mode for Pin 2.5
    TA0CTL = TASSEL__SMCLK | MC__UP | TACLR;  // SMCLK, up mode, clear TAR

    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1); // Configures Switch S1 to be an input button
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN4); // Configures Switch S2 to be an input button
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5, GPIO_PIN5, GPIO_TERTIARY_MODULE_FUNCTION); // Configures P5.5 as the input pin for the ADC Module

    // Configures LED1 and sets its initial state to off
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);

    // Configures the blue LED of LED2 and sets its initial state to off
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN2);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN2);

    ADC14_enableModule(); // Enables the ADC Module
    ADC14_setResolution(ADC_10BIT); // Sets the resolution of the ADC Module
    ADC14_initModule(ADC_CLOCKSOURCE_SMCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1, false); // Initializes the ADC Module
    ADC14_configureSingleSampleMode(ADC_MEM0, false); // Configures the single sample mode of the ADC

    // Sets and enables the voltage range from 0V to 2.5V (which is equal to the range of the possible voltage values across the potentiometer)
    REF_A_setReferenceVoltage(REF_A_VREF2_5V);
    REF_A_enableReferenceVoltage();

    ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_INTBUF_VREFNEG_VSS, ADC_INPUT_A0, false); // Configures the conversion memory of the ADC module
    ADC14_enableSampleTimer(ADC_MANUAL_ITERATION); // Enables each conversion to occur manually

    // Starts the conversion
    ADC14_enableConversion();
    ADC14_toggleConversionTrigger();

    unsigned int usiButton1 = 1; // Initializes the input value of Switch S1
    unsigned int usiButton1_prev = 1; // Initializes the previous input value of Switch S1
    unsigned int usiButton2 = 1; // Initializes the input value of Switch S2
    unsigned int usiButton2_prev = 1; // Initializes the previous input value of Switch S2
    unsigned int mode1 = 0; // Initializes a boolean that determines if the motor should spin in one direction (Mode 1)
    unsigned int mode2 = 0; // Initializes a boolean that determines if the motor should spin in the other direction (Mode 2)
    unsigned int order = 0; // Initializes an integer that determines which mode was activated first, which is to be used if both modes are accidentally active at the same time

    while(1)
    {
        usiButton1 = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN1); // Reads Switch S1
        usiButton2 = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4); // Reads Switch S2

        // Configuring Modes 1 and 2
        if((usiButton1 == not_pressed) & (usiButton1_prev == pressed)) // Determines if Switch S1 had just been pressed
        {
            // Toggles Mode 1 each time Switch S1 is pressed
            if(mode1 == 0) // Determines if Mode 1 was off
            {
                mode1 = 1; // Turns Mode 1 on
            }
            else if(mode1 == 1) // Determines if Mode 1 was on
            {
                mode1 = 0; // Turns Mode 1 off
            }
        }

        if((usiButton2 == not_pressed) & (usiButton2_prev == pressed)) // Determines if Switch S2 had just been pressed
        {
            // Toggles Mode 2 each time Switch S2 is pressed
            if(mode2 == 0) // Determines if Mode 2 was off
            {
                mode2 = 1; // Turns Mode 2 on
            }
            else if(mode2 == 1) // Determines if Mode 2 was on
            {
                mode2 = 0; // Turns Mode 2 off
            }
        }

        // Determining the function of the program based on the boolean values of the modes
        if((mode1 == 0) & (mode2 == 0)) // Determines if both modes are off
        {
            GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns LED1 off
            GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns LED2 off
            TA0CCR1 = 0; // Turns the PWM of Pin 2.4 off
            TA0CCR2 = 0; // Turns the PWM of Pin 2.5 off
        }
        else if((mode1 == 1) & (mode2 == 0)) // Determines if Mode 1 is on while Mode 2 is off
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns LED1 on
            GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns LED2 off
            order = 1; // Signifies that Mode 1 was turned on first

            ADC14_toggleConversionTrigger(); // Starts another conversion for the ADC
            while(ADC14_isBusy()){}; // Allows time to pass for the ADC to finish its conversion
            ADCresult = (uint16_t) ADC14_getResult(ADC_MEM0); // Obtains the ADC result of the potentiometer voltage

            TA0CCR2 = 0; // Turns the PWM of Pin 2.5 off
            TA0CCR1 = (ADCresult/1023)*10000; // Turns the PWM of Pin 2.4 on using the voltage across the potentiometer as an input for the duty cycle
        }
        else if((mode2 == 1) & (mode1 == 0)) // Determines if Mode 2 is on while Mode 1 is off
        {
            GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN2); // Turns LED2 on
            GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns LED1 off
            order = 2; // Signifies that Mode 2 was turned on first

            ADC14_toggleConversionTrigger(); // Starts another conversion for the ADC
            while(ADC14_isBusy()){}; // Allows time to pass for the ADC to finish its conversion
            ADCresult = (uint16_t) ADC14_getResult(ADC_MEM0); // Obtains the ADC result of the potentiometer voltage

            TA0CCR1 = 0; // Turns the PWM of Pin 2.4 off
            TA0CCR2 = (ADCresult/1023)*10000; // Turns the PWM of Pin 2.5 on using the voltage across the potentiometer as an input for the duty cycle
        }
        else if((mode1 == 1) & (mode2 == 1)) // Determines if Mode 1 and Mode 2 are both on
        {
            if(order == 1) // Determines if Mode 1 was activated first
            {
                mode2 = 0; // Turns off Mode 2
            }
            else if(order == 2) // Determines if Mode 2 was activated first
            {
                mode1 = 0; // Turns off Mode 1
            }
        }

        usiButton1_prev = usiButton1; // Sets the previous input value of Switch S1
        usiButton2_prev = usiButton2; // Sets the previous input value of Switch S2
    }

}
