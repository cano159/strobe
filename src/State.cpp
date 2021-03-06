#include "State.h"

#include <WProgram.h>

#include "Persist.h"
#include "Key.h"

State::State() {
    for (int i = 0; i < HardwareController::NUM_KEYS; i++) {
        keys[i] = new Key();
    }
    for (int i = 0; i < SCHEDULE_LENGTH; i++) {
        schedulePayload[i] = 0;
        scheduleOperation[i] = Operation::SET;
        scheduleTime[i] = 0;
    }
}

void State::handle(Route route, uint8_t payload, uint8_t depth = 0,
        bool up = false, bool down = false) {
    switch (route) {
        case Route::ANALOG:
            analogHandle(payload, depth);
            break;
        case Route::MOMENTARY:
            if (down) {
                schedule(payload, Operation::SET, 0);
            } else if (up) {
                schedule(payload, Operation::UNSET, 0);
            }
            break;
        case Route::TOGGLE:
            if (down) {
                schedule(payload, Operation::TOGGLE, 0);
            }
            break;
        case Route::SPECIAL:
            specialHandle(payload, depth);
            break;
    }
}

void State::analogHandle(uint8_t payload, uint8_t depth) {
    if (payload <= 0x0F) {
        mouseState.updateAnalog(payload, depth);
    } else if (payload >= 0x20 && payload <= 0x2F) {
        joystickState.updateAnalog(payload, depth);
    }
}

void State::schedule(uint8_t payload, Operation operation, uint16_t time) {
    uint16_t longestTime = 0;
    uint8_t longestID = 0;
    for (int i = 0; i < SCHEDULE_LENGTH; i++) {
        if (schedulePayload[i] == 0) {
            schedulePayload[i] = payload;
            scheduleOperation[i] = operation;
            scheduleTime[i] = time;
            return;
        } else {
            // Log the longest time incase the schedule is full
            if (scheduleTime[i] > longestTime) {
                longestID = i;
                longestTime = scheduleTime[i];
            }
        }
    }
    // If the schedule is full and this should come earlier than the longest
    // scheduled item, just replace it with this
    if (time < longestTime) {
        schedulePayload[longestID] = payload;
        scheduleOperation[longestID] = operation;
        scheduleTime[longestID] = time;
    }
}

void State::scanKeys() {
    for (int i = 0; i < HardwareController::NUM_KEYS; i++) {
        Route route;
        uint8_t payload = 0;
        // Get the key action based on active layers
        for (int l = Persist::getSetting(Setting::LAYER_COUNT) - 1; l >= 0; l--) {
            if (layerState.isActive(l)) {
                route = Persist::getRoute(i, l);
                payload = Persist::getPayload(i, l);
                break;
            }
        }
        if (payload != 0 && (i == 0 || i == 1)) {
            // Hysteresis for determining if key is pressed
            bool up = false, down = false;
            // If key was pressed last iteration
            if (keys[i]->pressed) {
                // and it has dropped below threshold, set to not pressed
                if (keys[i]->depth < Persist::getSetting(Setting::MIN_THRESHOLD)) {
                    keys[i]->pressed = false;
                    up = true;
                }
            // or if it wasn't pressed
            } else {
                // and it has risen above threshold, set to pressed
                if (keys[i]->depth > Persist::getSetting(Setting::MAX_THRESHOLD)) {
                    keys[i]->pressed = true;
                    down = true;
                }
            }
            handle(route, payload, keys[i]->depth, up, down);
        }
    }
}

void State::updateState() {
    scanKeys();
    uint16_t timeStep = sinceLastUpdate;
    for (int i = 0; i < SCHEDULE_LENGTH; i++) {
        if (schedulePayload[i] != 0) {
            if (scheduleTime[i] <= timeStep) {
                dispatchPayload(schedulePayload[i], scheduleOperation[i]);
                schedulePayload[i] = 0;
            } else {
                scheduleTime[i] -= timeStep;
            }
        }
    }
    keyboardState.send();
    //mouseState.send();
    //joystickState.send();
    mouseState.resetAnalog();
    joystickState.resetAnalog();
    sinceLastUpdate = elapsedMillis();
}


void State::specialHandle(uint8_t address, uint8_t depth) {
    // read the address and get the actions
}

void State::dispatchPayload(uint8_t payload, Operation operation) {
    if ((payload <= 0xA4) || (payload >= 0xE0 && payload <= 0xE7)) {
        keyboardState.update(payload, operation);
    } else if (payload >= 0xD0 && payload <= 0xDF) { // Toggle layer
        layerState.update(payload - 0xD0, operation);
        // All keys need to be unset so they don't get stuck on
        keyboardState.initState();
        joystickState.initState();
        mouseState.initState();
        // increment decrement can be set up through setting different
        // functions for a single key for each layer (eg: layer 1 function is
        // to go to layer 2, layer 2 function is to go to layer 3... etc).
    } else if (payload >= 0xB0 && payload <= 0xCF) { // Joystick keys
        joystickState.update(payload - 0xB0, operation);
    } else if (payload >= 0xA5 && payload <= 0xAA) { // Mouse buttons
        mouseState.update(payload - 0xA5, operation);
    } else if (payload == 0xFF) { // Keyboard lock
        // 0xFF keyboard lock
    }
}
