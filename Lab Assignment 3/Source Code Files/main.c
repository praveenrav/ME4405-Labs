#include "msp.h"
#include "driverlib.h"
#include "driver.h"

/**
 * main.c
 */

void turnOffLED2()
{
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN0); // Turns off the red LED of LED2
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN1); // Turns off the green LED of LED2
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN2); // Turns off the blue LED of LED2
}

void main(void)
{
    WDT_A_holdTimer(); // Stops the Watchdog Timer

    GPIO_setAsInputPinWithPullUpResistor(PORT1, GPIO_PIN1); // Sets Switch S1 as an input button
    GPIO_setAsInputPinWithPullUpResistor(PORT1, GPIO_PIN4); // Sets Switch S2 as an input button

    // Configures LED1 and sets its initial state to off
    GPIO_setAsOutputPin(PORT1, GPIO_PIN0);
    GPIO_setOutputLowOnPin(PORT1, GPIO_PIN0);

    // Configures LED2 and sets its initial state off to each of its LEDs
    GPIO_setAsOutputPin(PORT2, GPIO_PIN0); // Configures the red LED of LED2
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN0); // Turns the initial state of the red LED to off

    GPIO_setAsOutputPin(PORT2, GPIO_PIN1); // Configures the green LED of LED2
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN1); // Turns the initial state of the green LED to off

    GPIO_setAsOutputPin(PORT2, GPIO_PIN2); // Configures the blue LED of LED2
    GPIO_setOutputLowOnPin(PORT2, GPIO_PIN2); // Turns the initial state of the blue LED to off

    int i = 1; // Initializes the counting variable to be used to determine which LED of LED2 should be turned on
    int old_i = 0; // Initializes an additional counting variable that's used to ensure that different colors of LED2 will appear with consecutive activations of Switch S2

    unsigned int usiButton1 = 0; // Initializes the input value of Switch S1
    unsigned int usiButton2 = 0; // Initializes the input value of Switch S1

    while(1) // Runs an infinite loop for this program
    {
        usiButton1 = GPIO_getInputPinValue(PORT1, GPIO_PIN1); // Reads Switch S1
        usiButton2 = GPIO_getInputPinValue(PORT1, GPIO_PIN4); // Reads Switch S2

        if(usiButton1 == pressed) // Determines if Switch S1 is pressed
        {
            GPIO_setOutputHighOnPin(PORT1, GPIO_PIN0); // Turns LED1 on

        }
        else if(usiButton1 == not_pressed) // Determines if Switch S1 is not pressed
        {
            GPIO_setOutputLowOnPin(PORT1, GPIO_PIN0); // Turns LED1 off
        }

        if(usiButton2 == pressed) // Determines if Switch S2 is pressed
        {
            if(i == 1) // Determines if the value of i equals 1
            {
                GPIO_setOutputHighOnPin(PORT2, GPIO_PIN0); // Displays the red LED of LED2
                old_i = i; // Sets the old value of i equal to the current value of i
            }
            else if(i == 2) // Determines if the value of i equals 2
            {
                GPIO_setOutputHighOnPin(PORT2, GPIO_PIN1); // Displays the green LED of LED2
                old_i = i; // Sets the old value of i equal to the current value of i
            }
            else if(i == 3) // Determines if the value of i equals 3
            {
                GPIO_setOutputHighOnPin(PORT2, GPIO_PIN2); // Displays the blue LED of LED2
                old_i = i; // Sets the old value of i equal to the current value of i
            }
        }
        else if(usiButton2 == not_pressed)
        {
            turnOffLED2(); // Turns off all LED colors of LED2
            if(i == old_i) // Determines if the current value of i is equal to the old value of i
            {
                if(i < 3) // Determines if the current value of i is less than 3
                {
                    i++; // Increments i by 1
                }
                else if(i == 3) // Determines if the current value of i is equal to 3
                {
                    i = 1; // Resets the value of i to equal 1
                }
            }
        }
    }
}
