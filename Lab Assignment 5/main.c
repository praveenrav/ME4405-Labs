#include "msp.h"
#include "driverlib.h"
#include <stdio.h>

uint32_t SMCLK_divider = CS_CLOCK_DIVIDER_1;
#define timerA_divider   TIMER_A_CLOCKSOURCE_DIVIDER_64   // Means counter is incremented at 1E+6/64 = 15625 Hz
#define timerA_period    15625

volatile float ADCresult = 0; // Volatile variable that's used to store the instantaneous value of the ADC output at any given time

const Timer_A_UpModeConfig upConfig_0 =
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
	CS_setDCOFrequency(1E+6); // Sets the DCO Frequency
	CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, SMCLK_divider); // Initializes the clock signal

	ADC14_enableModule(); // Enables the ADC Module

	GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5, GPIO_PIN5, GPIO_TERTIARY_MODULE_FUNCTION); // Configures P5.5 as the input pin for the ADC Module

	ADC14_setResolution(ADC_10BIT); // Sets the resolution of the ADC Module

	ADC14_initModule(ADC_CLOCKSOURCE_SMCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1, false); // Initializes the ADC Module

	ADC14_configureSingleSampleMode(ADC_MEM0, false); // Configures the single sample mode of the ADC

	// Sets and enables the voltage range from 0V to 2.5V
	REF_A_setReferenceVoltage(REF_A_VREF2_5V);
	REF_A_enableReferenceVoltage();

    ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_INTBUF_VREFNEG_VSS, ADC_INPUT_A0, false); // Configures the conversion memory of the ADC module

	ADC14_enableSampleTimer(ADC_MANUAL_ITERATION); // Enables each conversion to occur manually

	Timer_A_configureUpMode(TIMER_A0_BASE, &upConfig_0);  // Configure Timer A using above structure
    Interrupt_enableInterrupt(INT_TA0_0);   // Enable Timer A interrupt

    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);  // Start Timer A

    // Starts the conversion
	ADC14_enableConversion();
	ADC14_toggleConversionTrigger();

	Interrupt_enableMaster(); // Enables all interrupts

	GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0); // Configures LED1 as an output
	GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns off LED1

	while(1) // Infinite while loop in order for the program to run indefinitely
	{
        while(ADC14_isBusy()){}; // Allows time to pass for the ADC to finish its conversion
        ADCresult = (uint16_t) ADC14_getResult(ADC_MEM0);  // Obtains the ADC result
        ADC14_toggleConversionTrigger(); // Starts another conversion for the ADC
	}
}

void TA0_0_IRQHandler()
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0); // Clear the timer A0 interrupt
    ADCresult = 2.5*ADCresult/1023 * (60.28454304); // Converts the ADC value to its corresponding temperature value in Fahrenheit
    if(ADCresult >= 80) // if statement that determines whether or not the reading from the temperature sensor is above 80 degrees Fahrenheit
    {
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns on LED1 if the temperature reading is below 80 degrees Fahrenheit
    }
    else
    {
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0); // Turns off LED1 if the temperature reading is below 80 degrees Fahrenheit
    }
    printf("Temperature: %f\r\n", ADCresult); // Prints the temperature of the ADC
}

