/*
 * driver.h
 *
 *  Created on: Sep 15, 2019
 *      Author: praveenrav702
 */

#include "driverlib.h"
#ifndef DRIVER_H_
#define DRIVER_H_

#define PORT1 GPIO_PORT_P1
#define PORT2 GPIO_PORT_P2
#define pressed GPIO_INPUT_PIN_LOW
#define not_pressed GPIO_INPUT_PIN_HIGH

void turnOffLED2();

#endif /* DRIVER_H_ */
