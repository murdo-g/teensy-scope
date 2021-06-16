#include "sh1106.h"

// constructor for hardware SPI - we indicate Reset, DataCommand and ChipSelect
// (HW_SPI used to differentiate constructor - reserved for future use)
SH1106::SH1106(bool HW_SPI, int sck, int mosi, int rst, int dc, int cs)
{
    myRST = rst;
    myDC = dc;
    myCS = cs;
    mySCK = sck;
    myMOSI = mosi;
}

void SH1106::init()
{
    pinMode(myDC, OUTPUT);
    pinMode(myCS, OUTPUT);

    SPI.begin();
    SPI.setMOSI(myMOSI);
    SPI.setSCK(mySCK);
    SPI.setClockDivider(SPI_CLOCK_DIV2);

    pinMode(myRST, OUTPUT);
    // Pulse Reset low for 10ms
    digitalWrite(myRST, HIGH);
    delay(1);
    digitalWrite(myRST, LOW);
    delay(10);
    digitalWrite(myRST, HIGH);
    sendInitCommands();
    resetDisplay();
}

void SH1106::resetDisplay(void)
{
    displayOff();
    clear();
    display();
    displayOn();
}

void SH1106::reconnect()
{
    SPI.begin();
}

void SH1106::displayOn(void)
{
    sendCommand(0xaf); //display on
}

void SH1106::displayOff(void)
{
    sendCommand(0xae); //display off
}

void SH1106::setContrast(char contrast)
{
    sendCommand(0x81);
    sendCommand(contrast);
}

void SH1106::flipScreenVertically()
{
    sendCommand(0xA0); //SEGREMAP   //Rotate screen 180 deg
    sendCommand(SETCOMPINS);
    sendCommand(0x22);
    sendCommand(COMSCANINC); //COMSCANDEC  Rotate screen 180 Deg
}

void SH1106::clear(void)
{
    memset(buffer, 0, (128 * 64 / 8));
}

void SH1106::display(void)
{
    sendCommand(COLUMNADDR);
    sendCommand(0x00);
    sendCommand(0x7F);

    sendCommand(PAGEADDR);
    sendCommand(0x0);
    sendCommand(0x7);

    sendCommand(SETSTARTLINE | 0x00);

    // for (uint8_t page = 0; page < 64/8; page++) {
    for (uint8_t page = 0; page < 8; page++)
    {
        sendCommand(2 & 0xF);
        sendCommand(PAGESTARTADDRESS + page);
        for (uint8_t col = 2; col < 128 + 2; col++)
        {
            // sendCommand(PAGESTARTADDRESS + page);
            // sendCommand(col & 0xF);
            sendCommand(SETHIGHCOLUMN | (col >> 4));
            digitalWrite(myCS, HIGH);
            digitalWrite(myDC, HIGH); // data mode
            digitalWrite(myCS, LOW);
            SPI.transfer(buffer[col - 2 + page * 128]);
        }
    }
    digitalWrite(myCS, HIGH);

    for (int i = 0; i < 1024; i++)
    {
        prevBuffer[i] = buffer[i];
    }
}

void SH1106::updateDisplay(void)
{

    sendCommand(COLUMNADDR);
    sendCommand(0x00);
    sendCommand(0x7F);

    sendCommand(PAGEADDR);
    sendCommand(0x0);
    sendCommand(0x7);

    sendCommand(SETSTARTLINE | 0x00);

    for (int i = 0; i < 1024; i++)
    {
        // If a byte has changed, update it
        if (buffer[i] != prevBuffer[i])
        {
            uint8_t page = i / 128;
            uint8_t col = i % 128 + 2;
            sendCommand(PAGESTARTADDRESS + page);
            sendCommand(col & 0xF);
            sendCommand(SETHIGHCOLUMN | (col >> 4));
            digitalWrite(myCS, HIGH);
            digitalWrite(myDC, HIGH); // data mode
            digitalWrite(myCS, LOW);
            SPI.transfer(buffer[col - 2 + page * 128]);
        }
        prevBuffer[i] = buffer[i];
    }
    digitalWrite(myCS, HIGH);
}

void SH1106::setPixel(int x, int y)
{
    if (x >= 0 && x < 128 && y >= 0 && y < 64)
    {

        switch (myColor)
        {
        case WHITE:
            buffer[x + (y / 8) * 128] |= (1 << (y & 7));
            break;
        case BLACK:
            buffer[x + (y / 8) * 128] &= ~(1 << (y & 7));
            break;
        case INVERSE:
            buffer[x + (y / 8) * 128] ^= (1 << (y & 7));
            break;
        }
    }
}

void SH1106::setChar(int x, int y, unsigned char data)
{
    for (int i = 0; i < 8; i++)
    {
        if (bitRead(data, i))
        {
            setPixel(x, y + i);
        }
    }
}

// Code form http://playground.arduino.cc/Main/Utf8ascii
byte SH1106::utf8ascii(byte ascii)
{
    if (ascii < 128)
    { // Standard ASCII-set 0..0x7F handling
        lastChar = 0;
        return (ascii);
    }

    // get previous input
    byte last = lastChar; // get last char
    lastChar = ascii;     // remember actual character

    switch (last) // conversion depnding on first UTF8-character
    {
    case 0xC2:
        return (ascii);
        break;
    case 0xC3:
        return (ascii | 0xC0);
        break;
    case 0x82:
        if (ascii == 0xAC)
            return (0x80); // special case Euro-symbol
    }

    return (0); // otherwise: return zero, if character has to be ignored
}

//Code form http : //playground.arduino.cc/Main/Utf8ascii
String
SH1106::utf8ascii(String s)
{
    String r = "";
    char c;
    for (int i = 0; i < s.length(); i++)
    {
        c = utf8ascii(s.charAt(i));
        if (c != 0)
            r += c;
    }
    return r;
}

void SH1106::drawString(int x, int y, String text)
{
    text = utf8ascii(text);
    unsigned char currentByte;
    int charX, charY;
    int currentBitCount;
    int charCode;
    int currentCharWidth;
    int currentCharStartPos;
    int cursorX = 0;
    int numberOfChars = pgm_read_byte(myFontData + CHAR_NUM_POS);
    // iterate over string
    int firstChar = pgm_read_byte(myFontData + FIRST_CHAR_POS);
    int charHeight = pgm_read_byte(myFontData + HEIGHT_POS);
    int currentCharByteNum = 0;
    int startX = 0;
    int startY = y;

    if (myTextAlignment == TEXT_ALIGN_LEFT)
    {
        startX = x;
    }
    else if (myTextAlignment == TEXT_ALIGN_CENTER)
    {
        int width = getStringWidth(text);
        startX = x - width / 2;
    }
    else if (myTextAlignment == TEXT_ALIGN_RIGHT)
    {
        int width = getStringWidth(text);
        startX = x - width;
    }

    for (int j = 0; j < text.length(); j++)
    {

        charCode = text.charAt(j) - 0x20;

        currentCharWidth = pgm_read_byte(myFontData + CHAR_WIDTH_START_POS + charCode);
        // Jump to font data beginning
        currentCharStartPos = CHAR_WIDTH_START_POS + numberOfChars;

        for (int m = 0; m < charCode; m++)
        {

            currentCharStartPos += pgm_read_byte(myFontData + CHAR_WIDTH_START_POS + m) * charHeight / 8 + 1;
        }

        currentCharByteNum = ((charHeight * currentCharWidth) / 8) + 1;
        // iterate over all bytes of character
        for (int i = 0; i < currentCharByteNum; i++)
        {

            currentByte = pgm_read_byte(myFontData + currentCharStartPos + i);
            //Serial.println(String(charCode) + ", " + String(currentCharWidth) + ", " + String(currentByte));
            // iterate over all bytes of character
            for (int bit = 0; bit < 8; bit++)
            {
                //int currentBit = bitRead(currentByte, bit);

                currentBitCount = i * 8 + bit;

                charX = currentBitCount % currentCharWidth;
                charY = currentBitCount / currentCharWidth;

                if (bitRead(currentByte, bit))
                {
                    setPixel(startX + cursorX + charX, startY + charY);
                }
            }
            yield();
        }
        cursorX += currentCharWidth;
    }
}

void SH1106::drawStringMaxWidth(int x, int y, int maxLineWidth, String text)
{
    int currentLineWidth = 0;
    int startsAt = 0;
    int endsAt = 0;
    int lineNumber = 0;
    char currentChar = ' ';
    int lineHeight = pgm_read_byte(myFontData + HEIGHT_POS);
    String currentLine = "";
    for (int i = 0; i < text.length(); i++)
    {
        currentChar = text.charAt(i);
        if (currentChar == ' ' || currentChar == '-')
        {
            String lineCandidate = text.substring(startsAt, i);
            if (getStringWidth(lineCandidate) <= maxLineWidth)
            {
                endsAt = i;
            }
            else
            {

                drawString(x, y + lineNumber * lineHeight, text.substring(startsAt, endsAt));
                lineNumber++;
                startsAt = endsAt + 1;
            }
        }
    }
    drawString(x, y + lineNumber * lineHeight, text.substring(startsAt));
}

int SH1106::getStringWidth(String text)
{
    text = utf8ascii(text);
    int stringWidth = 0;
    char charCode;
    for (int j = 0; j < text.length(); j++)
    {
        charCode = text.charAt(j) - 0x20;
        stringWidth += pgm_read_byte(myFontData + CHAR_WIDTH_START_POS + charCode);
    }
    return stringWidth;
}

void SH1106::setTextAlignment(int textAlignment)
{
    myTextAlignment = textAlignment;
}

void SH1106::setFont(const char *fontData)
{
    myFontData = fontData;
}

void SH1106::drawBitmap(int x, int y, int width, int height, const char *bitmap)
{
    for (int i = 0; i < width * height / 8; i++)
    {
        unsigned char charColumn = 255 - pgm_read_byte(bitmap + i);
        for (int j = 0; j < 8; j++)
        {
            int targetX = i % width + x;
            int targetY = (i / (width)) * 8 + j + y;
            if (bitRead(charColumn, j))
            {
                setPixel(targetX, targetY);
            }
        }
    }
}

void SH1106::setColor(int color)
{
    myColor = color;
}

void SH1106::drawLine(int x0, int y0, int x1, int y1)
{
    int t, distance;
    int xerr = 0, yerr = 0, delta_x, delta_y;
    int incx, incy;

    /* compute the distances in both directions */
    delta_x = x1 - x0;
    delta_y = y1 - y0;

    /* Compute the direction of the increment,
       an increment of 0 means either a horizontal or vertical
       line.
    */
    if (delta_x > 0)
        incx = 1;
    else if (delta_x == 0)
        incx = 0;
    else
        incx = -1;

    if (delta_y > 0)
        incy = 1;
    else if (delta_y == 0)
        incy = 0;
    else
        incy = -1;

    /* determine which distance is greater */
    delta_x = abs(delta_x);
    delta_y = abs(delta_y);
    if (delta_x > delta_y)
        distance = delta_x;
    else
        distance = delta_y;

    /* draw the line */
    for (t = 0; t <= distance + 1; t++)
    {
        setPixel(x0, y0);

        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            x0 += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            y0 += incy;
        }
    }
}

void SH1106::drawRect(int x, int y, int width, int height)
{
    for (int i = x; i < x + width; i++)
    {
        setPixel(i, y);
        setPixel(i, y + height);
    }
    for (int i = y; i < y + height; i++)
    {
        setPixel(x, i);
        setPixel(x + width, i);
    }
}

void SH1106::drawSine(float offset)
{
    float angle = offset;
    for (int x=0; x<128; x++){
        angle += 2*M_PI/128;
        int y = 32 - float(24) * sin(angle + offset);
        setPixel(x, y);
    }
}

void SH1106::drawGrid()
{
    for (int col=0; col <= 130; col += 1) {
            setPixel(col,0);
            setPixel(col,63);
    }
    
    for (int row=0; row <= 64; row += 4){
        setPixel(64,row);
    }
    for (int row=0; row <= 64; row += 1){
        setPixel(0,row);
        setPixel(127,row);
    }

    for (int row=2; row <= 128; row += 4){
        setPixel(row,32);
    }
}

void SH1106::drawLFOGrid(int pos)
{
    for (int col=0; col <= 90; col += 1) {
            setPixel(col,0);
            setPixel(col,63);
    }
    
    // for (int row=0; row <= 64; row += 4){
    //     setPixel(45,row);
    // }
    for (int row=0; row <= 64; row += 1){
        setPixel(0,row);
        setPixel(90,row);
    }

    for (int row=2; row <= 90; row += 4){
        setPixel(row,pos);
    }
}



void SH1106::fillRect(int x, int y, int width, int height)
{
    for (int i = x; i < x + width; i++)
    {
        for (int j = y; j < y + height; j++)
        {
            setPixel(i, j);
        }
    }
}

void SH1106::drawCircle(int x0, int y0, int radius)
{
    int f = 1 - radius;
    int ddF_x = 0;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;

    setPixel(x0, y0 + radius);
    setPixel(x0, y0 - radius);
    setPixel(x0 + radius, y0);
    setPixel(x0 - radius, y0);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x + 1;
        setPixel(x0 + x, y0 + y);
        setPixel(x0 - x, y0 + y);
        setPixel(x0 + x, y0 - y);
        setPixel(x0 - x, y0 - y);
        setPixel(x0 + y, y0 + x);
        setPixel(x0 - y, y0 + x);
        setPixel(x0 + y, y0 - x);
        setPixel(x0 - y, y0 - x);
    }
}

void SH1106::fillCircle(int x0, int y0, int radius)
{
    int f = 1 - radius;
    int ddF_x = 0;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;
    drawLine(x0 - radius, y0, x0 + radius, y0);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x + 1;
        drawLine(x0 - x, y0 + y, x0 + x, y0 + y);
        drawLine(x0 - x, y0 - y, x0 + x, y0 - y);
        drawLine(x0 - y, y0 + x, x0 + y, y0 + x);
        drawLine(x0 - y, y0 - x, x0 + y, y0 - x);
    }
}

void SH1106::drawXbm(int x, int y, int width, int height, const char *xbm)
{
    if (width % 8 != 0)
    {
        width = ((width / 8) + 1) * 8;
    }
    for (int i = 0; i < width * height / 8; i++)
    {
        unsigned char charColumn = pgm_read_byte(xbm + i);
        for (int j = 0; j < 8; j++)
        {
            int targetX = (i * 8 + j) % width + x;
            int targetY = (8 * i / (width)) + y;
            if (bitRead(charColumn, j))
            {
                setPixel(targetX, targetY);
            }
        }
    }
}

void SH1106::sendCommand(unsigned char com)
{
    digitalWrite(myCS, HIGH);
    digitalWrite(myDC, LOW); //command mode
    digitalWrite(myCS, LOW);
    SPI.transfer(com);
    digitalWrite(myCS, HIGH);
}

void SH1106::sendInitCommands(void)
{
    sendCommand(DISPLAYOFF);
    sendCommand(NORMALDISPLAY);
    sendCommand(SETDISPLAYCLOCKDIV);
    sendCommand(0xF0);
    sendCommand(SETMULTIPLEX);
    sendCommand(0x3F);
    sendCommand(SETDISPLAYOFFSET);
    sendCommand(0x00);
    sendCommand(SETSTARTLINE | 0x00);
    sendCommand(CHARGEPUMP);
    sendCommand(0x14);
    sendCommand(MEMORYMODE);
    sendCommand(0x00);
    sendCommand(SEGREMAP);
    sendCommand(COMSCANDEC);
    sendCommand(SETCOMPINS);
    sendCommand(0x12);
    sendCommand(SETCONTRAST);
    sendCommand(0xCF);
    sendCommand(SETPRECHARGE);
    sendCommand(0xF1);
    sendCommand(SETVCOMDETECT);
    sendCommand(0x40);
    sendCommand(DISPLAYALLON_RESUME);
    sendCommand(NORMALDISPLAY);
    sendCommand(0x2e); // stop scroll
    sendCommand(DISPLAYON);
}

void SH1106::drawBigKnob(int x, int y, int rad, float angle)
{
    float xOff = (rad * 0.5) * sin(angle) + 1;
    float yOff = (rad * 0.5) * cos(angle) + 1;

    drawCircle(x, y, rad);
    fillCircle(x, y, rad * 0.5);
    setColor(BLACK);
    drawLine(x, y, (x + xOff), (x + yOff));
    setColor(WHITE);
}

void SH1106::drawWeeKnob(int x, int y, int rad, float angle)
{
    float xOff = (rad * 0.6) * sin(angle);
    float yOff = (rad * 0.6) * cos(angle);

    drawCircle(x, y, rad);
    fillCircle(x, y, rad * 0.6);
    setColor(BLACK);
    drawLine(x, y, (x + xOff), (y + yOff));
    setColor(WHITE);
}

void SH1106::drawTallTrimmer(int x, int y, int rad, float angle)
{
    float xOff = (rad)*sin(angle);
    float yOff = (rad)*cos(angle);
    drawCircle(x, y, rad);
    drawLine(x, y, (x + xOff), (y + yOff));
}

void SH1106::drawSlider(int x, int y, int pos)
{
    drawRect(x, y, 8, 50);
    fillRect(x, (y + pos / 2), 8, 4);
}