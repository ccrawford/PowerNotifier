#include <Arduino.h>

enum class HeaterState
{
    STARTUP,
    COOL,
    OFF,
    WARMING,
    HOT,
    COOLING,
    UNKNOWN
};

class HeaterMonitor
{
private:
    HeaterState currentState;
    unsigned long lastStateChangeTime;
    float lastPowerReading;
    bool unknownFlag;

public:
    HeaterMonitor() : currentState(HeaterState::STARTUP), lastStateChangeTime(0), lastPowerReading(0), unknownFlag(false) {}

    // powerReading is the raw reading from the current monitor. 
    // updateTime is the time the reading was last sent from the monitor
    void update(float powerReading, unsigned long updateTime)
    {
        // Check for unknown state
        if (millis() - updateTime > 10000)
        {
            setState(HeaterState::UNKNOWN, updateTime);
            unknownFlag = true;
        }
        else
        {
            unknownFlag = false;
        }

        // Serial.println("Time since update: " + String(millis() - updateTime));

        // State machine logic
        switch (currentState)
        {
        // If you see power during startup, go to hot. Else if nothing for (hysteresis time) go to cool.
        case HeaterState::STARTUP:      
            if (powerReading > 0)
            {
                setState(HeaterState::HOT, updateTime);
            }
            else if (updateTime - lastStateChangeTime > 20000)
            {
                setState(HeaterState::COOL);
            }
            break;
        // If it's currently cool, and we see power, go to warming. If it stays off for x seconds, dim the display
        case HeaterState::OFF:
            // Fall through.
        case HeaterState::COOL:
            if (powerReading > 0)
            {
                setState(HeaterState::WARMING, updateTime);
            }
            if (updateTime - lastStateChangeTime > 10000)
            {
                // Transition to COOL if in COOL state for 20 seconds
                setState(HeaterState::OFF, updateTime);
            }
            break;
        // If it's warming and it goes off, assume hot. If it's been warming for 2 minutes, go to hot.
        // This could mean if it's turned on accidentally and immediately turned off, it will incorrectly go to hot.
        case HeaterState::WARMING:
            if (powerReading == 0)
            {
                setState(HeaterState::HOT, updateTime);
            }
            else if (updateTime - lastStateChangeTime > 120000)
            {
                // Transition to HOT if in WARMING state for 2 minutes
                setState(HeaterState::HOT, updateTime);
            }
            break;

        // The 90 second delay is because it stays hot for a long time.
        case HeaterState::HOT:
            if (powerReading == 0 && updateTime - lastStateChangeTime > 90000)
            {
                setState(HeaterState::COOLING, updateTime);
            }
            break;

        case HeaterState::COOLING:
            if (powerReading > 0)
            {
                setState(HeaterState::WARMING, updateTime);
            }
            else if (updateTime - lastStateChangeTime > 120000)
            {
                setState(HeaterState::COOL, updateTime);
            }
            break;

        case HeaterState::UNKNOWN:
            if (!unknownFlag)
            {
                // Transition back to startup State
                setState(HeaterState::STARTUP, updateTime);
            }
            break;
        }

        // Check for unknown state after state machine logic
        if (unknownFlag && currentState != HeaterState::UNKNOWN)
        {
            setState(HeaterState::UNKNOWN, updateTime);
        }
    }

    HeaterState getState() const
    {
        return currentState;
    }

    String updateSignage()
    {
        // Implement your signage update logic here
        switch (currentState)
        {
        case HeaterState::STARTUP:
            return "Signage: Starting up";
            break;
        case HeaterState::COOL:
            return "Signage: Cool";
            break;
        case HeaterState::WARMING:
            return "Signage: Warming";
            break;
        case HeaterState::HOT:
            return "Signage: Hot";
            break;
        case HeaterState::COOLING:
            return "Signage: Cooling";
            break;
        case HeaterState::UNKNOWN:
            return "Signage: Unknown";
            break;
        }
        return "Signage: Unknown";
    }

private:
    void setState(HeaterState newState, unsigned long updateTime = millis())
    {
        if (newState != currentState)
        {
            currentState = newState;
            lastStateChangeTime = updateTime;
            // Here you could add code to update your signage
            Serial.println(updateSignage() + " @ " + String(lastStateChangeTime));
        }
    }
};