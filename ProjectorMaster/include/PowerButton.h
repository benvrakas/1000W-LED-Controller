#pragma once

#include "BoardPins.h"

volatile bool abortRequested;

//Function Prototypes

//ISR
void handleButtonInterrupt();