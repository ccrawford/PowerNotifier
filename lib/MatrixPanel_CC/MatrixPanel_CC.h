#ifndef CC_ESP_HUB75_MatrixPanel_h
#define CC_ESP_HUB75_MatrixPanel_h

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Add some utility functions. This works, but is sloppy. CAC

#define MAX_SCROLL_MSG_LEN 256

class MatrixPanel_CC : public MatrixPanel_I2S_DMA
{
private:
    static MatrixPanel_CC *instance;
    char _scrollerMessage[MAX_SCROLL_MSG_LEN] = "";
    int _scrollMessageY = 31;
    int _scrollMs = 50;
    int _scrollOffset = 63; // Screen width
    unsigned long _lastScrollUpdate = 0;

    // Private constructor
    MatrixPanel_CC(const HUB75_I2S_CFG &opts)
        : MatrixPanel_I2S_DMA{opts}
    {
    }

public:
    // Delete copy constructor and assignment operator
    MatrixPanel_CC(const MatrixPanel_CC &) = delete;
    void operator=(const MatrixPanel_CC &) = delete;

    // Static method to get the instance
    static MatrixPanel_CC *getInstance(const HUB75_I2S_CFG &opts)
    {
        if (!instance)
            instance = new MatrixPanel_CC(opts);
        return instance;
    }

    // static MatrixPanel_CC* getInstance()
    // {
    //     if (!instance)
    //     {
    //         return nullptr;
    //         Serial.println("MatrixPanel_CC::getInstance() called without setup.");
    //     }
    //     return instance;
    // }

    int setScrollMessage(const char *msg)
    {
        if (!strncmp(_scrollerMessage, msg, MAX_SCROLL_MSG_LEN))
            return 0;
        strncpy(_scrollerMessage, msg, MAX_SCROLL_MSG_LEN);
        _scrollOffset = 63;
        _lastScrollUpdate = 0;
        Serial.printf("Msg now: %s", msg);
        return strlen(_scrollerMessage);
    }

    void setScrollMessageLine(int y)
    {
        _scrollMessageY = y;
    }

    int scrollText()
    {
        if (millis() - _lastScrollUpdate < _scrollMs)
            return 0;
        _lastScrollUpdate = millis();

        if (!strlen(_scrollerMessage))
            return 0;

        int msgLen = strlen(_scrollerMessage);

        setTextWrap(false);
        setCursor(_scrollOffset, _scrollMessageY);
        writeFillRect(0, _scrollMessageY - 6, 64, 7, 0);
        setTextColor(color444(15, 15, 15));

        printf(_scrollerMessage);
        _scrollOffset--;
        // 16 extra characters. 4 pixel font width typical. screen max x 63
        // Should use get text bounds, but that feels expensive.
        if (_scrollOffset < -((msgLen + 16) * 4))
            _scrollOffset = 63;

        return _scrollOffset - ((msgLen + 16) * 4); // return pixels left to scroll
    }

    size_t printAt(int16_t x, int16_t y, const char *format, ...)
    {
        setCursor(x, y);
        va_list argp;
        va_start(argp, format);
        size_t retval = vprintf(format, argp);
        va_end(argp);
        // Serial.print("P:%s %s", format, argp);
        // Serial.printf("argp: %s", argp);
        return (retval);
    }
    size_t printAt(int16_t x, int16_t y, uint16_t color, const char *format, ...)
    {
        setCursor(x, y);
        setTextColor(color);
        va_list argp;
        va_start(argp, format);
        size_t retval = vprintf(format, argp);
        va_end(argp);
        // Serial.printf("P:%s %s", format, argp);
        // Serial.printf("argp: %s", argp);
        return (retval);
    }

    size_t vprintf(const char *format, va_list arg)
    {
        char loc_buf[128];
        char *temp = loc_buf;
        int len = vsnprintf(temp, sizeof(loc_buf), format, arg);
        if (len < 0)
        {
            return 0;
        };
        if (len >= sizeof(loc_buf))
        {
            temp = (char *)malloc(len + 1);
            if (temp == NULL)
            {
                return 0;
            }
            len = vsnprintf(temp, len + 1, format, arg);
        }
        len = write((uint8_t *)temp, len);
        if (temp != loc_buf)
        {
            free(temp);
        }
        return len;
    }

    size_t printRight(uint16_t x, uint16_t y, const char *format, ...)
    {
        char buf[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, 127, format, args);
        va_end(args);

        int16_t x1, y1;
        uint16_t w, h;
        const String text = buf;
        getTextBounds(text, 0, 12, &x1, &y1, &w, &h);
        setCursor(x - w, y);
        size_t retval = print(buf);
        return (retval);
    }

    size_t printRight(uint16_t x, uint16_t y, uint16_t color, const char *format, ...)
    {
        char buf[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, 127, format, args);
        va_end(args);

        setTextColor(color);
        int16_t x1, y1;
        uint16_t w, h;
        const String text = buf;
        getTextBounds(text, 0, 12, &x1, &y1, &w, &h);
        setCursor(x - w, y);
        size_t retval = print(buf);
        return (retval);
    }

    size_t printCenter(uint16_t x, uint16_t y, const char *format, ...)
    {
        char buf[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, 127, format, args);
        va_end(args);

        int16_t x1, y1;
        uint16_t w, h;
        const String text = buf;
        getTextBounds(text, 0, 12, &x1, &y1, &w, &h);
        setCursor(x - (w / 2), y);
        size_t retval = print(buf);
        return (retval);
    }

    size_t printCenter(uint16_t x, uint16_t y, uint16_t color, const char *format, ...)
    {
        char buf[128];
        va_list args;
        va_start(args, format);
        setTextColor(color);
        vsnprintf(buf, 127, format, args);
        va_end(args);
        int16_t x1, y1;
        uint16_t w, h;
        const String text = buf;
        getTextBounds(text, 0, 12, &x1, &y1, &w, &h);
        setCursor(x - (w / 2), y);
        size_t retval = print(buf);
        return (retval);
    }

    // All of a sudden this became necessary. https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/20
    // Not sure what changed. But without this, the panel doesn't start right coming out of flash.
    void resetPanel(HUB75_I2S_CFG::i2s_pins pins)
    {
        int MaxLed = 64;

        int C12[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int C13[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};

        pinMode(pins.r1, OUTPUT);
        pinMode(pins.g1, OUTPUT);
        pinMode(pins.b1, OUTPUT);
        pinMode(pins.r2, OUTPUT);
        pinMode(pins.g2, OUTPUT);
        pinMode(pins.b2, OUTPUT);
        pinMode(pins.a, OUTPUT);
        pinMode(pins.b, OUTPUT);
        pinMode(pins.c, OUTPUT);
        pinMode(pins.d, OUTPUT);
        pinMode(pins.e, OUTPUT);
        pinMode(pins.clk, OUTPUT);
        pinMode(pins.lat, OUTPUT);
        pinMode(pins.oe, OUTPUT);

        // Send Data to control register 11
        digitalWrite(pins.oe, HIGH); // Display reset
        digitalWrite(pins.lat, LOW);
        digitalWrite(pins.clk, LOW);
        for (int l = 0; l < MaxLed; l++)
        {
            int y = l % 16;
            digitalWrite(pins.r1, LOW);
            digitalWrite(pins.g1, LOW);
            digitalWrite(pins.b1, LOW);
            digitalWrite(pins.r2, LOW);
            digitalWrite(pins.g2, LOW);
            digitalWrite(pins.b2, LOW);
            if (C12[y] == 1)
            {
                digitalWrite(pins.r1, HIGH);
                digitalWrite(pins.g1, HIGH);
                digitalWrite(pins.b1, HIGH);
                digitalWrite(pins.r2, HIGH);
                digitalWrite(pins.g2, HIGH);
                digitalWrite(pins.b2, HIGH);
            }
            if (l > MaxLed - 12)
            {
                digitalWrite(pins.lat, HIGH);
            }
            else
            {
                digitalWrite(pins.lat, LOW);
            }
            digitalWrite(pins.clk, HIGH);
            digitalWrite(pins.clk, LOW);
        }
        digitalWrite(pins.lat, LOW);
        digitalWrite(pins.clk, LOW);
        // Send Data to control register 12
        for (int l = 0; l < MaxLed; l++)
        {
            int y = l % 16;
            digitalWrite(pins.r1, LOW);
            digitalWrite(pins.g1, LOW);
            digitalWrite(pins.b1, LOW);
            digitalWrite(pins.r2, LOW);
            digitalWrite(pins.g2, LOW);
            digitalWrite(pins.b2, LOW);
            if (C13[y] == 1)
            {
                digitalWrite(pins.r1, HIGH);
                digitalWrite(pins.g1, HIGH);
                digitalWrite(pins.b1, HIGH);
                digitalWrite(pins.r2, HIGH);
                digitalWrite(pins.g2, HIGH);
                digitalWrite(pins.b2, HIGH);
            }
            if (l > MaxLed - 13)
            {
                digitalWrite(pins.lat, HIGH);
            }
            else
            {
                digitalWrite(pins.lat, LOW);
            }
            digitalWrite(pins.clk, HIGH);
            digitalWrite(pins.clk, LOW);
        }
        digitalWrite(pins.lat, LOW);
        digitalWrite(pins.clk, LOW);
    }
};

#endif