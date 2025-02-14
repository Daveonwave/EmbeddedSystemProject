/***************************************************************************
 *   Copyright (C) 2013 by Salaorni Davide, Velati Matteo                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "display_st7735.h"
#include "miosix.h"
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

namespace mxgui {
    
/**
 * Function to attach the display we are using.
 */
void registerDisplayHook(DisplayManager& dm) {
    dm.registerDisplay(&DisplayImpl::instance());
}

/**
 * Init sequence for the correct functioning of the ST7735 display
 */
const unsigned char initST7735b[] = {
    0x3A, 0X01, 0x05,                   // ST7735_COLMOD, color mode: 16-bit/pixel
    0xB1, 0x03, 0x00, 0x06, 0x03,       // ST7735_FRMCTR1, normal mode frame rate
    0xB6, 0x02, 0x15, 0x02,             // ST7735_DISSET5, display settings
    0xB4, 0x01, 0x00,                   // ST7735_INVCTR, line inversion active
    0xC0, 0x02, 0x02, 0x70,             // ST7735_PWCTR1, default (4.7V, 1.0 uA)
    0xC1, 0x01, 0x05,                   // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
    0xC3, 0x02, 0x02, 0x07,             // ST7735_PWCTR4, bclk/2, opamp current small and medium low
    0xC5, 0x02, 0x3C, 0x38,             // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
    0xFC, 0x02, 0x11, 0x15,             // ST7735_PWCTR6, power control (partial mode+idle)
    0xE0, 0x10,                         // ST7735_GMCTRP1, Gamma adjustments (pos. polarity)
        0x09, 0x16, 0x09, 0x20,         // (Not entirely necessary, but provides
        0x21, 0x1B, 0x13, 0x19,         // accurate colors)
        0x17, 0x15, 0x1E, 0x2B,
        0x04, 0x05, 0x02, 0x0E,
    0xE1, 0x10,                         // ST7735_GMCTRN1, Gamma adjustments (neg. polarity)
        0x0B, 0x14, 0x08, 0x1E,         // (Not entirely necessary, but provides
        0x22, 0x1D, 0x18, 0x1E,         // accurate colors)
        0x1B, 0x1A, 0x24, 0x2B,
        0x06, 0x06, 0x02, 0x0F,
    0x13,                               // ST7735_NORON, normal display mode on
    0x00
};

/**
 * Class DisplayImpl
 */
DisplayImpl& DisplayImpl::instance() {
    static DisplayImpl instance;
    return instance;
}

void DisplayImpl::doTurnOn() {
    writeReg(0x29);     //ST7735_DISPON 
    delayMs(150);
}

void DisplayImpl::doTurnOff() {
    writeReg(0x28);     //ST7735_DISPOFF 
    delayMs(150);
}

void DisplayImpl::doSetBrightness(int brt) {
    //No function to set brightness for this display
}

pair<short int, short int> DisplayImpl::doGetSize() const {
    return make_pair(height, width);
}

void DisplayImpl::write(Point p, const char *text) {
    font.draw(*this, textColor, p, text);
}

void DisplayImpl::clippedWrite(Point p, Point a,  Point b, const char *text) {
    font.clippedDraw(*this, textColor, p, a, b, text);
}

void DisplayImpl::clear(Color color) {
    clear(Point(0,0), Point(width-1, height-1), color);
}

void DisplayImpl::clear(Point p1, Point p2, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    imageWindow(p1, p2);
    int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);

    SPITransaction t;
    writeRamBegin();

    //Send data to write on GRAM
    for(int i=0; i < numPixels; i++) {
        writeRam(msb);
        writeRam(lsb);
    }
}

void DisplayImpl::beginPixel() {
    imageWindow(Point(0,0), Point(width-1, height-1));
}

void DisplayImpl::setPixel(Point p, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    setCursor(p);
    SPITransaction t;
    writeRamBegin();
    writeRam(msb);
    writeRam(lsb);
}

void DisplayImpl::line(Point a, Point b, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    //Horizontal line speed optimization
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()), Point(max(a.x(), b.x()), a.y()));
        int numPixels = abs(a.x() - b.x());

        SPITransaction t;
        writeRamBegin();

        //Send data to write on GRAM
        for(int i=0; i <= numPixels; i++) {
            writeRam(msb);
            writeRam(lsb);
        }
        return;
    }
    //Vertical line speed optimization
    if(a.x() == b.x())
    {
        textWindow(Point(a.x(), min(a.y(), b.y())), Point(a.x(), max(a.y(), b.y())));
        int numPixels = abs(a.y() - b.y());

        SPITransaction t;
        writeRamBegin();

        //Send data to write on GRAM
        for(int i=0; i <= numPixels; i++) {
            writeRam(msb);
            writeRam(lsb);
        }
        return;
    }
    //General case, always works but it is much slower due to the display
    //not having fast random access to pixels
    Line::draw(*this, a, b, color);
}

void DisplayImpl::scanLine(Point p, const Color *colors, unsigned short length) {
    unsigned char lsb = 0x00;
    unsigned char msb = 0x00;

    if(p.x() + length > width) { return; }
    imageWindow(p, Point(width - 1, p.y()));

    SPITransaction t;
    writeRamBegin();

    //Send data to write on GRAM
    for(int i=0; i < length; i++) {
        lsb = colors[i] & 0xFF;
        msb = (colors[i] >> 8) & 0xFF;

        writeRam(msb);
        writeRam(lsb);
    }
}

Color *DisplayImpl::getScanLineBuffer() {
    if(buffer == 0) buffer = new Color[getWidth()];
    return buffer;
}

void DisplayImpl::scanLineBuffer(Point p, unsigned short length) {
    scanLine(p, buffer, length);
}

void DisplayImpl::drawImage(Point p, const ImageBase& img) {
    short int xEnd = p.x() + img.getWidth() - 1;
    short int yEnd = p.y() + img.getHeight() - 1;
    if(xEnd >= width || yEnd >= height) { return; }

    const unsigned short *imgData = img.getData();
    if(imgData != 0)
    {
        unsigned char lsb = 0x00;
        unsigned char msb = 0x00;

        //Optimized version for memory-loaded images
        imageWindow(p, Point(xEnd, yEnd));
        int numPixels = img.getHeight() * img.getWidth();

        SPITransaction t;
        writeRamBegin();

        for(int i=0; i <= numPixels; i++)
        {
            lsb = imgData[i] & 0xFF;
            msb = (imgData[i] >> 8) & 0xFF;
            writeRam(msb);
            writeRam(lsb);
        }
    }
    else { img.draw(*this,p); }
}

void DisplayImpl::clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) {
    img.clippedDraw(*this,p,a,b);
}

void DisplayImpl::drawRectangle(Point a, Point b, Color c) {
    line(a,Point(b.x(), a.y()), c);
    line(Point(b.x(), a.y()), b, c);
    line(b,Point(a.x(), b.y()), c);
    line(Point(a.x(), b.y()), a, c);
}

void DisplayImpl::window(Point p1, Point p2, bool swap) {
    #ifdef MXGUI_ORIENTATION_VERTICAL
        char caset_offset = 2;
        char raset_offset = 1;
    #else //MXGUI_ORIENTATION_HORIZONTAL
        char caset_offset = 1;
        char raset_offset = 2;
    #endif

    //Setting column bounds, ST7735_CASET (adding offset)
    unsigned char buff_caset[4];
    buff_caset[0] = (p1.x()+caset_offset)>>8 & 255;
    buff_caset[1] = (p1.x()+caset_offset) & 255;
    buff_caset[2] = (p2.x()+caset_offset)>>8 & 255;
    buff_caset[3] = (p2.x()+caset_offset) & 255;

    //Setting row bounds, ST7735_RASET (adding offset)
    unsigned char buff_raset[4];
    buff_raset[0] = (p1.y()+raset_offset)>>8 & 255;
    buff_raset[1] = (p1.y()+raset_offset) & 255;
    buff_raset[2] = (p2.y()+raset_offset)>>8 & 255;
    buff_raset[3] = (p2.y()+raset_offset) & 255;

    // For drawing texts, swap the caset and raset buffers
    if (swap){
        writeReg(0x2A, buff_raset, sizeof(buff_raset));
        writeReg(0x2B, buff_caset, sizeof(buff_caset));
    }
    else {
        writeReg(0x2A, buff_caset, sizeof(buff_caset));
        writeReg(0x2B, buff_raset, sizeof(buff_raset));
    }
}

void DisplayImpl::update() {
    // Useless for ST7735 display
}

DisplayImpl::pixel_iterator
DisplayImpl::begin(Point p1, Point p2, IteratorDirection d) {
        if(p1.x()<0 || p1.y()<0 || p2.x()<0 || p2.y()<0) {
            return pixel_iterator();
        }
        if(p1.x() >= width || p1.y() >= height || p2.x() >= width || p2.y() >= height) {
            return pixel_iterator();
        }
        if(p2.x() < p1.x() || p2.y() < p1.y()) {
            return pixel_iterator();
        }

        if(d == DR) { textWindow(p1, p2); }
        else { imageWindow(p1, p2); }

        SPITransaction t;
        writeRamBegin();

        unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
        return pixel_iterator(numPixels);
}

//Destructor
DisplayImpl::~DisplayImpl() {
    if(buffer) delete[] buffer;
}

//Constructor
DisplayImpl::DisplayImpl(): buffer(0) {
    {
        FastInterruptDisableLock dLock;

        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
        SPI2->CR1 = 0;
        SPI2->CR1 = SPI_CR1_SSM     //Software cs
                  | SPI_CR1_SSI     //Hardware cs internally tied high
                  | SPI_CR1_BR_0    //clock divider: 4  ->  10,5 MHz -> 95 ns
                  | SPI_CR1_MSTR    //Master mode
                  | SPI_CR1_SPE;    //SPI enabled

        scl::mode(Mode::ALTERNATE);     scl::alternateFunction(5);
        sda::mode(Mode::ALTERNATE);     sda::alternateFunction(5);
        // GPIO software controlled
        csx::mode(Mode::OUTPUT);
        dcx::mode(Mode::OUTPUT);
        resx::mode(Mode::OUTPUT);
    }
    
    csx::high();
    dcx::high();

    // POWER ON SEQUENCE: HW RESET -> SW RESET -> SLPOUT
    resx::high();
    delayMs(150);
    resx::low();
    delayMs(150);
    resx::high();
    delayMs(150);

    writeReg(0x01);    // ST7735_SWRESET
    delayMs(150);
    writeReg(0x11);    // ST7735_SLPOUT
    delayMs(150);

    sendCmds(initST7735b);

    doTurnOn();
    setFont(droid11);
    setTextColor(make_pair(white, black));
}

/**
 * Write only commands with one parameter.
 */
void DisplayImpl::writeReg(unsigned char reg, unsigned char data)
{
    SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    writeRam(data);
}

/**
 * Write commands with more parameters.
 */
void DisplayImpl::writeReg(unsigned char reg, const unsigned char *data, int len)
{
     SPITransaction t;
    {
        CommandTransaction c;
        writeRam(reg);
    }
    if(data)
    {
        for(int i = 0; i < len; i++) {
            writeRam(*data++);
        }
    }
}

/**
 * Send commands of 8 bits to the MCU of the display.
 */
void DisplayImpl::sendCmds(const unsigned char *cmds) {
    while(*cmds)
    {
        unsigned char cmd = *cmds++;
        unsigned char numArgs = *cmds++;
        writeReg(cmd, cmds, numArgs);
        cmds += numArgs;
        delayUs(1);
    }
}

} //mxgui

#else
#warning "This SPI driver has only been tested on an STM32F4DISCOVERY"
#endif //_BOARD_STM32F4DISCOVERY
