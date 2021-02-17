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
 * Class DisplayImpl
 */
DisplayImpl& DisplayImpl::instance() {
    static DisplayImpl instance;
    return instance;
}

void DisplayImpl::doTurnOn() {
    {
        SPITransaction t;
        writeCmd(0x29);
    }
}

void DisplayImpl::doTurnOff() {
    {
        SPITransaction t;
        writeCmd(0x28);     //ST7735_DISPOFF TODO: should be followed by SLPIN
        delayMs(150);
    }
}

//No function to set brightness
void DisplayImpl::doSetBrightness(int brt) {}

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
    clear(Point(0,0), Point(width-1,  height-1), color);
}

void DisplayImpl::clear(Point p1, Point p2, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    imageWindow(p1, p2);
    int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);

    {
        SPITransaction t;
        writeCmd(0x2C);     //RAMWR, to write the GRAM
        //Send data to write on GRAM
        for(int i=0; i < numPixels; i++) {
            writeData(msb);
            writeData(lsb);
        }
    }
}

void DisplayImpl::beginPixel() {
    imageWindow(Point(0,0), Point(width-1, height-1));
}

void DisplayImpl::setPixel(Point p, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    setCursor(p);
    {
        SPITransaction t;
        writeCmd(0x2C);         //RAMWR, to write the GRAM
        writeData(msb);
        writeData(lsb);
    }
}

void DisplayImpl::line(Point a, Point b, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;

    //Horizontal line speed optimization
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()), Point(max(a.x(), b.x()), a.y()));
        int numPixels = abs(a.x() - b.x());

        {
            SPITransaction t;
            writeCmd(0x2C);         //RAMWR, to write the GRAM
            //Send data to write on GRAM
            for(int i=0; i <= numPixels; i++) {
                writeData(msb);
                writeData(lsb);
            }
        }
        return;
    }
    //Vertical line speed optimization
    if(a.x() == b.x())
    {
        textWindow(Point(a.x(), min(a.y(), b.y())), Point(a.x(), max(a.y(), b.y())));
        int numPixels = abs(a.y() - b.y());

        {
            SPITransaction t;
            writeCmd(0x2C);         //RAMWR, to write the GRAM
            //Send data to write on GRAM
            for(int i=0; i <= numPixels; i++) {
                writeData(msb);
                writeData(lsb);
            }
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

    imageWindow(p, Point(width - 1, p.y()));
    if(p.x() + length > width) { return; }

    {
        SPITransaction t;
        writeCmd(0x2C);         //RAMWR, to write the GRAM
        //Send data to write on GRAM
        for(int i=0; i < length; i++) {
            lsb = colors[i] & 0xFF;
            msb = (colors[i] >> 8) & 0xFF;

            writeData(msb);
            writeData(lsb);
        }
    }
}

Color *DisplayImpl::getScanLineBuffer() {
    return buffers[which];
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

        {
            SPITransaction t;
            writeCmd(0x2C);         //RAMWR, to write the GRAM
            for(int i=0; i <= numPixels; i++)
            {
                lsb = imgData[i] & 0xFF;
                msb = (imgData[i] >> 8) & 0xFF;
                writeData(msb);
                writeData(lsb);
            }
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


void DisplayImpl::window(Point p1, Point p2) {
    // TODO useless
}

void DisplayImpl::update() {}

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

        if(d == DR) {   // Down - Right
            textWindow(p1, p2);
        }
        else {
            imageWindow(p1, p2);
        }

        {
            SPITransaction t;
            writeCmd(0x2C);         //RAMWR, to write the GRAM
        }


        unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
        return pixel_iterator(numPixels);
}

//Destructor
DisplayImpl::~DisplayImpl() {}

//Constructor
DisplayImpl::DisplayImpl(): which(0) {
    {
        FastInterruptDisableLock dLock;

        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; //Enabling the SPI2 on APB1 bus
        SPI2->CR1 = 0;                      //Control register 1
        SPI2->CR1 = SPI_CR1_SSM             //(Software slave management) Software cs
                  | SPI_CR1_SSI             //(Internal slave select) Hardware cs internally tied high
                  | SPI_CR1_BR_0
                  | SPI_CR1_BR_1            // clock divider: 16  ->  2,625 MHz -> 381 ns
                  | SPI_CR1_MSTR            //(Master Selection) Master mode
                  | SPI_CR1_SPE;            //SPI enabled

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
    // HW RESET
    resx::high();
    delayMs(150);
    resx::low();
    delayMs(150);
    resx::high();
    delayMs(150);

    sendInitSeq();

    setFont(miscFixed);
    setTextColor(make_pair(white, blue));
}


void DisplayImpl::sendInitSeq() {

    {
        SPITransaction t;

        //SW RESET
        writeCmd(0x01);
        // must wait 120ms after SW reset cmd
        delayMs(150);

        //SLPOUT
        writeCmd(0x11);
        // must wait 120ms after SLEEP OUT cmd
        delayMs(150);

        //COLMOD, color mode: 16-bit/pixel
        writeCmd(0x3A);     writeData(0x05);
        /*
        //MADCTL, top-bottom, left-right refresh, RGB color mode
        writeCmd(0x36);     writeData(0xC0);

        //CASET, column address
        writeCmd(0x2A);     writeData(0x00); writeData(0x02);   //x_start = 2
                            writeData(0x00); writeData(0x81);   //x_end = 129
        delayMs(150);
        //RASET, row address
        writeCmd(0x2B);     writeData(0x00); writeData(0x01);   //y_start = 1
                            writeData(0x00); writeData(0xA0);   //y_end = 160
        delayMs(150);
        */

        //FRMCTR1, normal mode frame rate
        writeCmd(0xB1);     writeData(0x00); writeData(0x06); writeData(0x03);
        //DISSET5, display settings
        writeCmd(0xB6);     writeData(0x15); writeData(0x02);
        //INVCTR, line inversion active
        writeCmd(0xB4);     writeData(0x00);
        //PWCTR1, default (4.7V, 1.0 uA)
        writeCmd(0xC0);     writeData(0x02); writeData(0x70);
        //PWCTR2, default (VGH=14.7V, VGL=-7.35V)
        writeCmd(0xC1);     writeData(0x05);
        /* TODO not working
        //PWCTR3, opamp current small, boost frequency  (NOT WORKING)
        writeCmd(0xC2);     writeData(0x01); writeData(0x02);
        */
        //PWCTR4, default
        writeCmd(0xC3);     writeData(0x02); writeData(0x07);
        //VMCTR1, VCOMH=4V VCOML=-1.1
        writeCmd(0xC5);     writeData(0x3C); writeData(0x38);
        //PWCTR6, default
        writeCmd(0xFC);     writeData(0x11); writeData(0x15);
        //GMCTRP1, Gamma adjustments (pos. polarity)
        writeCmd(0xE0);     writeData(0x09); writeData(0x16); writeData(0x09); writeData(0x20);
                            writeData(0x21); writeData(0x1B); writeData(0x13); writeData(0x19);
                            writeData(0x17); writeData(0x15); writeData(0x1E); writeData(0x2B);
                            writeData(0x04); writeData(0x05); writeData(0x02); writeData(0x0E);
        //GMCTRN1, Gamma adjustments (neg. polarity)
        writeCmd(0xE1);     writeData(0x0B); writeData(0x14); writeData(0x08); writeData(0x1E);
                            writeData(0x22); writeData(0x1D); writeData(0x18); writeData(0x1E);
                            writeData(0x1B); writeData(0x1A); writeData(0x24); writeData(0x2B);
                            writeData(0x06); writeData(0x06); writeData(0x02); writeData(0x0F);
        //NORON, normal display mode on
        writeCmd(0x13);
        //DISPON
        writeCmd(0x29);
        delayMs(150);
        writeCmd(0x00);
    }

    
}

} //mxgui

#else
#warning "This SPI driver has only been tested on an STM32F4DISCOVERY"
#endif //_BOARD_STM32F4DISCOVERY
