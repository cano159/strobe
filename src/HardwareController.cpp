#include "HardwareController.h"

#include "ADC.h"

/*** Setup all the pins and ADC in constructor ***/

HardwareController::HardwareController() {

    // LED pin
    pinMode(PIN_LED, OUTPUT);
    // Analog in pin
    pinMode(PIN_ROW_READ, INPUT);
    // Digital multiplexer controller pins
    pinMode(PIN_MUX_CONTROL[0], OUTPUT);
    pinMode(PIN_MUX_CONTROL[1], OUTPUT);
    pinMode(PIN_MUX_CONTROL[2], OUTPUT);
    // Column pins
    for (int i = 0; i < NUM_COLS; i++) {
        pinMode(PIN_COL[i], OUTPUT);
    }

    // Row ADC
    adc = new ADC();
    adc->setAveraging(1);
    adc->setResolution(8); // returns a byte (0-255)
    adc->setConversionSpeed(ADC_HIGH_SPEED);
    adc->setSamplingSpeed(ADC_HIGH_SPEED);
    adc->setReference(ADC_REF_3V3);

}


/*** Reading functions ***/

uint8_t HardwareController::strobeRead(uint8_t col) {
    uint8_t value;
    // Make sure we have waited enough time
    if (timeSinceLastStrobe < ROW_RELAX_TIME) {
        delayMicroseconds(ROW_RELAX_TIME - timeSinceLastStrobe);
    }

    // Time sensitive part - interrupts can affect delayMicroseconds
    noInterrupts();
    // Set column high ("strobe")
    controller->setColHigh(col);
    // Wait for amplifier to catch up
    delayMicroseconds(3);
    // Read the row value
    value = controller->readRow();
    // Set column low
    controller->setColLow(col);
    // Turn back on interrupts and wait for row to relax to 0V
    interrupts();

    timeSinceLastStrobe = elapsedMicros();
    return value;
}

void HardwareController::selectRow(uint8_t row) const {
    // Get binary representation using bitwise operations
    digitalWrite(PIN_MUX_CONTROL[0], (row) & 1);
    digitalWrite(PIN_MUX_CONTROL[1], (row >> 1) & 1);
    digitalWrite(PIN_MUX_CONTROL[2], (row >> 2) & 1);
    delayMicroseconds(ROW_RELAX_TIME);
}


/*** Row functions ***/

uint8_t HardwareController::readRow() const {
    return adc->analogRead(PIN_ROW_READ, ADC_0);
}


/*** Col functions ***/

void HardwareController::setColHigh(uint8_t col) const {
    digitalWrite(PIN_COL[col], HIGH);
}

void HardwareController::setColLow(uint8_t col) const {
    digitalWrite(PIN_COL[col], LOW);
}


/*** LED functions ***/

void HardwareController::turnOnLED() const {
    digitalWrite(PIN_LED, HIGH);
}

void HardwareController::turnOffLED() const {
    digitalWrite(PIN_LED, LOW);
}


// Global definition
HardwareController* controller = new HardwareController();
