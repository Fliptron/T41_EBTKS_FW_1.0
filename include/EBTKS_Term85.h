#include <Arduino.h>

#define HP85_WIDTH (32U)
#define HP85_LINES (16U)
extern void DMA_Poke8(uint32_t addr, uint8_t);
extern void DMA_Poke16(uint32_t addr, uint16_t);
extern uint8_t DMA_Peek8(uint32_t addr);
extern volatile bool DMA_Active;
extern volatile bool DMA_Request;
extern int32_t DMA_Read_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);
extern int32_t DMA_Write_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);
extern void release_DMA_request(void);

#define CRTSAD (0177404) //  CRT START ADDRESS
#define CRTBAD (0177405) //  CRT BYTE ADDRESS
#define CRTSTS (0177406) //  CRT STATUS
#define CRTDAT (0177407) //  CRT DATA

enum
{
    SCROLL_UP,
    SCROLL_DOWN,
    SCROLL_LEFT,
    SCROLL_RIGHT
};

//
//  virtual terminal emulation.
//  we write to a virtual screen. The dimensions are set in the constructor
//

#define HORZ_CHARS (80U)
#define VERT_LINES (25U)

class Term85 : public Stream
{
public:
    Term85() {}

    void begin(uint8_t chars, uint8_t lines)
    {
        memset(_virtScreen, ' ', sizeof(_virtScreen));

        // make sure we don't exceed our limits
        _chars = chars > HORZ_CHARS ? HORZ_CHARS : chars;
        _lines = lines > VERT_LINES ? VERT_LINES : lines;
        setCursor(0, 0);
        Serial.printf("Term85 %d chars by %d lines\r\n", _chars, _lines);
    }
    virtual int available() { return 0; }
    virtual int read() { return 0; }
    virtual int peek() { return 0; }
    virtual void flush() {}
    virtual void clear(void) {}
    virtual size_t write(uint8_t c) { return pchar(c); }
    //virtual size_t write(const uint8_t *buffer, size_t size) { return usb_serial_write(buffer, size); }
    using Print::write;

    uint8_t getLines(void)
    {
        return _lines;
    }

    uint8_t getWidth(void)
    {
        return _chars;
    }

    int pchar(uint8_t ch)
    {
        uint8_t c = translate(ch);
        //Serial.printf("%c",ch); //diagnostics

        if (c) //if the char was not consumed by translation
        {
            putCh(_currCh, _currLine, c);
            incCursor();
        }
        return 1;
    }
    //
    //  return character if character was not consumed
    //
    uint8_t translate(uint8_t ch)
    {
        uint8_t used = ch;

        switch (_xlateState)
        {
        case 0: //test for chars that need translation
            switch (ch)
            {

            case 7: //BEL
                //bing!!
                break;

            case 8: //BS backspace
                if (_currCh)
                {
                    _currCh--;
                }
                else
                {
                    //backspace up a line
                    _currCh = _chars - 1;
                    if (_currLine)
                    {
                        _currLine--;
                    }
                    else
                    {
                        //we're at the top of the screen
                        _currLine = 0;
                        _currCh = 0;
                    }
                }
                used = 0;
                break;

            case 0x0d: //CR
                used = 0;
                _currCh = 0;
                break;

            case 0x0a: //LF
                _currLine++;
                if (_currLine >= _lines)
                {
                    scroll();
                    _currLine = _lines - 1;
                }
                used = 0;
                break;

            case '\t': //TAB
                // insert 4 chars
                used = 0;

            case 0x1a: //adm3a clear screen
                clearScreen();
                used = 0;
                break;

            case 0x1b: //ESC
                used = 0;
                _xlateState = 1; //expect another char
                break;

            case 0x1e: //adm3a home cursor
                used = 0;
                setCursor(0, 0);
                break;

            case '{':
                used = 0x80 | '(';  //translate squiggly brace to normal brace with underline (hp85 doesn't have  squiggly braces)
                break;

            case '}':
                used = 0x80 | ')'; 
                break;

            case 0x7f: //DEL
                break;
            }
            break;

        case 1: //previous char was ESC, process next
            used = 0;
            if (ch == '=') //adm3a cursor addressing
            {
                _xlateState = 2;
            }
            else if ((ch == 'B') || (ch == 'C'))
            {
                _xlateState = 4; //gobble up unimplemented commands
            }
            else
            {
                _xlateState = 0;
            }
            break;

        case 2: //get row
            if (ch >= 0x20)
            {
                _row = ch - 0x20;
            }
            used = 0;
            _xlateState = 3;
            break;

        case 3: //get col
            if (ch >= 0x20)
            {
                _col = ch - 0x20;
            }
            setCursor(_col, _row);
            _xlateState = 0;
            used = 0;
            break;

        case 4: //gobble unimplemented commands
            used = 0;
            _xlateState = 0;
            break;
        }

        return used;
    }
    uint8_t getCh(uint8_t h, uint8_t v)
    {
        uint32_t ndx = v * _chars + h;
        if (ndx > sizeof(_virtScreen))
        {
            Serial.printf("term85 error! %d\r\n", ndx);
        }
        return _virtScreen[ndx];
    }

    uint8_t getCursorCh(void)
    {
        return _currCh;
    }

    uint8_t getCursorLine(void)
    {
        return _currLine;
    }

private:
    uint8_t _virtScreen[HORZ_CHARS * VERT_LINES];
    uint8_t _chars;
    uint8_t _lines;
    uint8_t _currCh;
    uint8_t _currLine;
    uint32_t _xlateState;
    uint8_t _row;
    uint8_t _col;

    void putCh(uint8_t h, uint8_t v, uint8_t ch)
    {
        uint32_t ndx = v * _chars + h;
        if (ndx > sizeof(_virtScreen))
        {
            Serial.printf("term85 error! %d\r\n", ndx);
        }
        _virtScreen[ndx] = ch;
    }

    void clearLine(uint8_t v)
    {
        for (uint32_t a = 0; a < _chars; a++)
        {
            putCh(a, v, ' ');
        }
    }

    void charAtCursor(uint8_t ch)
    {
        putCh(_currCh, _currLine, ch);
        incCursor();
    }
    void clearScreen()
    {
        for (uint32_t a = 0; a < _lines; a++)
        {
            clearLine(a);
        }
    }

    void scroll()
    {
        for (uint32_t v = 1; v < _lines; v++)
        {
            for (uint32_t h = 0; h < _chars; h++)
            {
                putCh(h, v - 1, getCh(h, v));
            }
        }
        // clear bottom line
        clearLine(_lines - 1);
    }

    void setCursor(uint8_t h, uint8_t v)
    {
        _currCh = h;
        _currLine = v;
    }

    void incCursor(void)
    {
        _currCh++;
        if (_currCh >= _chars)
        {
            _currCh = 0; //wrap around
            _currLine++;
            if (_currLine >= _lines)
            {
                scroll();
                _currLine = _lines - 1;
            }
        }
    }
};

class MapScreen
{
public:
    MapScreen(Term85 *term)
    {
        _term = term;
        _startCh = 0;
        _startLine = 0;
        _enabled = false;
        _tick = millis();
    }

    // updates the HP85 display. call at a regular interval
    void poll()
    {
        if (_enabled == true)
        {
            if (millis() > (50U + _tick))
            {
                _tick = millis();
                update();
            }
        }
    }

    void enable(bool en)
    {
        _enabled = en;
    }

    void update()
    {
        uint8_t data;

        //copy a line at a time and dma into the hp85's video controller
        while (DMA_Peek8(CRTSTS) & 0x80)
        {
        };                     //wait until video controller is ready
        DMA_Poke16(CRTBAD, 0); //set the crt address to the beginning of the screen
        DMA_Poke16(CRTSAD, 0); //set the crt start address to the beginning of the screen

        //start dma
        DMA_Request = true;
        while (!DMA_Active)
        {
        }; // Wait for acknowledgement, and Bus ownership

        for (uint32_t line = 0; line < HP85_LINES; line++)
        {
            for (uint32_t ch = 0; ch < HP85_WIDTH; ch++)
            {
                data = 0x80;
                while (data & 0x80)
                {
                    DMA_Read_Block(CRTSTS, (uint8_t *)&data, 1);
                }; //wait until video controller is ready

                uint8_t c = _term->getCh(ch + _startCh, line + _startLine);
                //uint8_t c = 'A' + line;
                if ((line == ((uint32_t)_term->getCursorLine() - _startLine)) && (ch == ((uint32_t)_term->getCursorCh() - _startCh)))
                {
                    c |= 0x80; //add cursor
                }
                DMA_Write_Block(CRTDAT, &c, 1);
                //DMA_Poke8(CRTDAT,c);
            }
        }
        release_DMA_request();
        while (DMA_Active)
        {
        }; // Wait for release
        //stop dma
    }
    void updateLoop(void)
    {
        //update one byte at a time and only if the crt is ready
        if ((DMA_Peek8(CRTSTS) & 0x80) == 0) //ready??
        {
            switch (_updateState)
            {
            case 0:                    //send start addr
                DMA_Poke16(CRTBAD, 0); //set the crt address to the beginning of the screen
                _updateState = 1;
                _upCh = 0;
                _upLine = 0;
                break;

            case 1:
                uint8_t c = _term->getCh(_upCh + _startCh, _upLine + _startLine);

                if ((_upLine == ((uint32_t)_term->getCursorLine() - _startLine)) && (_upCh == ((uint32_t)_term->getCursorCh() - _startCh)))
                {
                    c |= 0x80; //add cursor
                }
                DMA_Poke8(CRTDAT, c);

                _upCh++;
                if (_upCh >= HP85_WIDTH)
                {
                    _upCh = 0;
                    _upLine++;
                }
                if (_upLine >= HP85_LINES)
                {
                    _upLine = 0;
                    _updateState = 0;
                }
                break;
            }
        }
    }

    void move(int scroll)
    {
        switch (scroll)
        {
        case SCROLL_UP:
            if (_startLine)
            {
                _startLine--;
            }
            break;

        case SCROLL_DOWN:
            _startLine++;
            if (_startLine >= (_term->getLines() - HP85_LINES))
            {
                _startLine = _term->getLines() - HP85_LINES;
            }
            break;

        case SCROLL_LEFT:
            if (_startCh)
            {
                _startCh--;
            }
            break;

        case SCROLL_RIGHT:
            _startCh++;
            if (_startCh >= (_term->getWidth() - HP85_WIDTH))
            {
                _startCh = _term->getWidth() - HP85_WIDTH;
            }
            break;
        }
    }

private:
    Term85 *_term;
    uint8_t _startCh;
    uint8_t _startLine;
    uint32_t _upCh;
    uint32_t _upLine;
    uint32_t _updateState;
    bool _enabled;
    uint32_t _tick;
};
