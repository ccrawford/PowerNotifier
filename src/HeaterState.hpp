#include <Arduino.h>

// #define HOME_TESTING 0

#ifdef HOME_TESTING

#define STARTUP_TIMEOUT_MS 20 * 1000   // 20 seconds  It's hard to tell what's going on at startup, but if no power assume cool
#define OFF_TO_WARM_MS 0 * 1000        // 0 seconds
#define WARM_TO_HOT_MS 10 * 1000      // 10 seconds. 
#define HOT_TO_WARM_MS  60 * 1000  // 1 minutes. Takes a long time!
#define WARM_TO_COOL_MS 30  * 1000 // 30 sec. Estimation. NEed to measure.
#define COOL_TO_OFF_MS 15 * 60 * 1000  // 15 minutes
#define LOST_CONNECTION_MS 10 * 1000   // 10 seconds  We haven't gotten a plug update in 10 seconds. Normal is 1 per sec

// LOWER bounds of current readings for each state
#define OFF_CURRENT_A 0.0
#define IDLE_CURRENT_A 0.15
#define HEATING_CURRENT_A .33    // I think it's around 12.4, so over this and under HOT is warming.
#define MAINTAINING_CURRENT_A .10 // I think it's around 7.6, so over this and under WARMING is maintaining.

#else

#define STARTUP_TIMEOUT_MS 20 * 1000   // 20 seconds  It's hard to tell what's going on at startup, but if no power assume cool
#define OFF_TO_WARM_MS 0 * 1000        // 0 seconds
#define WARM_TO_HOT_MS 100 * 1000      // 100 seconds. Measured.
#define HOT_TO_WARM_MS 72 * 60 * 1000  // 72 minutes. Takes a long time!
#define WARM_TO_COOL_MS 30 * 60 * 1000 // 30 minutes. Estimation. NEed to measure.
#define COOL_TO_OFF_MS 15 * 60 * 1000  // 15 minutes
#define LOST_CONNECTION_MS 10 * 1000   // 10 seconds  We haven't gotten a plug update in 10 seconds. Normal is 1 per sec

// LOWER bounds of current readings for each state
#define OFF_CURRENT_A 0.0
#define HEATING_CURRENT_A 12.0    // I think it's around 12.4, so over this and under HOT is warming.
#define MAINTAINING_CURRENT_A 0.05 // I think it's around 7.6, so over this and under WARMING is maintaining.

#endif

enum class HeaterTrend
{
    HEATING,
    COOLING,
    MAINTAINING,
    IDLE,
    UNKNOWN,
    STARTUP
};

enum class HeaterState
{
    STARTUP,
    COOL,
    OFF,
    WARM,
    HOT,
    UNKNOWN
};


class HeaterMonitor
{
private:
    HeaterState _currentState;
    HeaterTrend _heaterTrend;
    unsigned long lastStateChangeTime;
    unsigned long lastTrendChangeTime;
    float lastPowerReading;
    bool unknownFlag;

public:
    HeaterMonitor() : _currentState(HeaterState::STARTUP), lastStateChangeTime(0), lastPowerReading(0), unknownFlag(false)
    {
        _heaterTrend = HeaterTrend::UNKNOWN;
    }

    long secondsSinceLastStateChange()
    {
        return (millis() - lastStateChangeTime) / 1000;
    }

    long secondsSinceLastTrendChange()
    {
        return (millis() - lastTrendChangeTime) / 1000;
    }

    // powerReading is the raw reading from the current monitor.
    // updateTime is the time the reading was last sent from the monitor
    void update(float powerReading, unsigned long updateTime)
    {
        // Check for unknown state. Set values and return if unknown.
        if (millis() - updateTime > LOST_CONNECTION_MS)
        {
            setState(HeaterState::UNKNOWN, updateTime);
            setTrend(HeaterTrend::UNKNOWN);
            unknownFlag = true;
            return;
        }
        else
        {
            unknownFlag = false;
        }

        // Set the trend: heating, cooling, maintaining, or idle.
        if (powerReading >= HEATING_CURRENT_A)
        {
            setTrend(HeaterTrend::HEATING);
        }
        else if (powerReading >= MAINTAINING_CURRENT_A)
        {
            setTrend(HeaterTrend::MAINTAINING);
        }
        else
        {
            if (getState() == HeaterState::COOL || getState() == HeaterState::OFF)  // Note that this will be the state in the last loop.
                setTrend(HeaterTrend::IDLE);
            else
                setTrend(HeaterTrend::COOLING);
        }

        // State machine logic
        switch (_currentState)
        {
        // If you see power during startup, go to hot. Else if nothing for (hysteresis time) go to cool.
        case HeaterState::STARTUP:
            if (powerReading > OFF_CURRENT_A) // If you see any current during startup, go to hot.
            {
                setState(HeaterState::HOT, updateTime);
            }
            else if (updateTime - lastStateChangeTime > STARTUP_TIMEOUT_MS)
            {
                setState(HeaterState::OFF);
            }
            break;
        // If it's currently cool, and we see power, go to warming. If it stays off for x seconds, dim the display
        case HeaterState::OFF:
            // Fall through.
        case HeaterState::COOL:
            if (powerReading > HEATING_CURRENT_A)
            {
                setState(HeaterState::WARM, updateTime);
            }
            else if (powerReading >= MAINTAINING_CURRENT_A) // Theoretically should not see this.
            {
                setState(HeaterState::HOT, updateTime);
                Serial.printf("Unexpected power reading in state %d: %f\n", (int)_currentState, powerReading);
            }
            else if (powerReading < MAINTAINING_CURRENT_A)
            {
                if (updateTime - lastStateChangeTime > COOL_TO_OFF_MS) // Eventually set state to off if it's been cool for a while.
                {
                    setState(HeaterState::OFF, updateTime);
                }
                break;
            // If it's warming and it goes off, assume hot. If it's been warming for 2 minutes, go to hot.
            // This could mean if it's turned on accidentally and immediately turned off, it will incorrectly go to hot.
            case HeaterState::WARM:
                if (getTrend() == HeaterTrend::MAINTAINING)  // Heater is maintaining...hot, but should have already been there.
                {
                    setState(HeaterState::HOT, updateTime);
                }
                else if (getTrend() == HeaterTrend::HEATING && lastStateChangeTime > WARM_TO_HOT_MS)
                {
                    setState(HeaterState::HOT, updateTime);
                }
                else if (getTrend() == HeaterTrend::COOLING && lastStateChangeTime > WARM_TO_COOL_MS)
                {
                    setState(HeaterState::COOL, updateTime);
                }
                break;

            case HeaterState::HOT:
                if (getTrend() == HeaterTrend::COOLING && updateTime - lastStateChangeTime > HOT_TO_WARM_MS)
                {
                    setState(HeaterState::WARM, updateTime);
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
            if (unknownFlag && _currentState != HeaterState::UNKNOWN)
            {
                setState(HeaterState::UNKNOWN, updateTime);
            }
        }
    }

    HeaterState getState() const
    {
        return _currentState;
    }

    HeaterTrend getTrend() const
    {
        return _heaterTrend;
    }

    String updateSignage()
    {
        // Implement your signage update logic here
        switch (_currentState)
        {
        case HeaterState::STARTUP:
            return "Signage: Starting up";
            break;
        case HeaterState::COOL:
            return "Signage: Cool";
            break;
        case HeaterState::WARM:
            return "Signage: Warming";
            break;
        case HeaterState::HOT:
            return "Signage: Hot";
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
        if (newState != _currentState)
        {
            _currentState = newState;
            lastStateChangeTime = updateTime;
            Serial.println(updateSignage() + " @ " + String(lastStateChangeTime));
        }
    }
    void setTrend(HeaterTrend newTrend)
    {
        if (newTrend != _heaterTrend)
        {
            lastTrendChangeTime = millis();
            _heaterTrend = newTrend;
            Serial.println("Trend: " + String((int)_heaterTrend) + " @ " + String(lastTrendChangeTime));
        }
    }
};