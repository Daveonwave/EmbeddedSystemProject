#include "display_st7735.h"
#include "miosix.h"
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F407VG_STM32F4DISCOVERY

namespace mxgui {

//Control interface
typedef Gpio<GPIOA, 5> scl; //SPI1_SCK (af5)
typedef Gpio<GPIOA, 7> sda; //SPI1_MOSI (af5)
typedef Gpio<GPIOA, 4> csx; //free I/O pin
typedef Gpio<GPIOB, 5> resx; //free I/O pin
typedef Gpio<GPIOB, 6> dcx; //free I/O pin, used only in 4-line SPI
//rdx not used in serial, only parallel
//te not used in serial, only parallel

/**
 * Send and receive a byte through SPI1
 * \param c byte to send
 * \return byte received
 */
static unsigned char sp1sendRev(unsigned char c=0) {
    SPI1->DR = c;
    while((SPI1->SR && SPI_SR_RXNE) == 0);
    return SPI1->DR;
}

/**
 * Send a command to the ST7735 display
 * \param cmd command
 * \param len length of the (optional) argument, or 0 for commands without
 * arguments.
 */
static void sendCmd(unsigned char cmd, int len, ...) {
    dcx::low(); //tells there is a command
    csx::low();
    sp1sendRev(cmd);
    csx::high(); //indicate the start of the transmission
    delayUs(1); //TODO: delay
    dcx::high(); //tells there are parameters
    va_list arg;
    va_start(arg, len)
    for(int i = 0; i < len; i++)
    {
        csx::low();
        sp1sendRev(va_arg, int);
        csx::high()
        delayUs(1);
    }
    va_end(arg);
}

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
    sendCmd(0x29, 0);   //ST7735_DISPON
    delayMs(150);       //Should be 120ms, TODO: maybe noy necessary!
}

void DisplayImpl::doTurnOff() {
    sendCmd(0x28, 0);   //ST7735_DISPOFF TODO: should be followed by SLPIN
}

//No function to set brightness
void DisplayImpl::doSetBrightness(int brt) {}

pair<short int, short int> DisplayImpl::doGetSize() const {
    return make_pair(height, weight);
}

void DisplayImpl::write(Point p, const char *text) {
    font.draw(*this, textColor, p, text);
}

void DisplyImpl::clippedWrite(Point p, Point a,  Point b, const char *text) {
    font.clippedDraw(*this, textColor, p, a, b, text);
}

void DisplayImpl::clear(Color color) {
    clear(Point(0,0), Point(width-1,  height-1), color);
}

//TODO: TO IMPLEMENT ---------------------------------------------------------------------
//----------------------------------------------------------------------------------------
void DisplayImpl::clear(Point p1, Point p2, Color color) {}

void DisplayImpl::beginPixel() {}

void DisplayImpl::setPixel(Point p, Color color) {}

void DIsplayImpl::line(Point a, Point b, Color color) {}

void DisplayImpl::scanLine(Point p, const Color *colors, unsigned short length) {}

Color *DisplayImpl::getScanLineBuffer() {}

void DisplayImpl::scanLineBuffer(Point p, unsigned short length) {}

void DisplayImpl::drawImage(Point p, unsigned short length) {}

void DisplayImpl::clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) {}

void DisplayImpl::drawTectangle(Point a, Point b, Color c) {}

Display::pixel_iterator DisplayImpl::begin(Point p1, Point p2, IteratorDirection d) {}

void DisplayImpl::setCursor(Point p) {}

void DisplayImpl::textWindow(Point p1, Point p2) {}

DisplayImpl::imageWindow(Point p1, Point p2) {}

DisplayImpl::~DisplayImpl() {}
//---------------------------------------------------------------------------------------
//TODO: ---------------------------------------------------------------------------------

DisplayImpl::Display(): buffer(0) {
    //TODO: RCC configuration
    {
        FastInterruptDisableLock dLock;

        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        SPI1->CR2=0;
        SPI1->CR1=SPI_CR1_SSM  //Software cs
                | SPI_CR1_SSI  //Hardware cs internally tied high
                | SPI_CR1_MSTR //Master mode
                | SPI_CR1_SPE; //SPI enabled
                | (3<<3)        //Divide input clock by 16: 84/16=5.25MHz
        //RCC something and stuff..
        //Spi1en..
    }

    //TODO: while something (RCC->CR == 0)...

    {
        FastInterruptDisableLock dLock;

        scl::mode(Mode::ALTERNATE);     scl::alternateFunction(5);
        sda::mode(Mode::ALTERNATE);     sda::alternateFunction(5);
        csx::mode(Mode::OUTPUT);
        dcx::mode(Mode::OUTPUT);
        rsx::mode(Mode::OUTPUT);

        //TODO: RCC something...
    }

    //Instructions for initialization...
    sendCmd(0x01, 0);                       // ST7735_SWRESET
    delayMs(150);
    sendCmd(0x11, 0);                       // ST7735_SLPOUT
    delayMs(255);
    sendCmd(0x3A, 1, 0x05);                 // ST7735_COLMOD, color mode: 16-bit/pixel

    sendCmd(0xB1, 3, 0x01, 0x2C, 0x2D);     // ST7735_FRMCTR1, normal mode frame rate
    delayMs(10);
    //sendCmd(0xB2, 3, 0x01, 0x2C, 0x2D);   // ST7735_FRMCTR2, idle mode frame rate
    //sendCmd(0xB3, 6, 0x01, 0x2C, 0x2D,
    //                 0x01, 0x2C, 0x2D);   // ST7735_FRMCTR3, partial mode frame rate

    sendCmd(0x36, 1, 0x08);                 // ST7735_MADCTL, row/col addr, bottom-top refresh
    sendCmd(0xB6, 2, 0x15, 0x02);           // ST7735_DISSET5, display settings
    sendCmd(0xB4, 1, 0x00);                 // ST7735_INVCTR, line inversion active

    sendCmd(0xC0, 2, 0x02, 0x70);           // ST7735_PWCTR1, default (4.7V, 1.0 uA)
    delayMs(10);
    sendCmd(0xC1, 1, 0x05);                 // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
    sendCmd(0xC2, 2, 0x01, 0x02);           // ST7735_PWCTR3, opamp current small, boost frequency
    sendCmd(0xC5, 2, 0x3C, 0x38);           // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
    delayMs(10);
    sendCmd(0xFC, 2, 0x11, 0x15);           // ST7735_PWCTR6, power control (partial mode+idle) TODO: get rid of it

    sendCmd(0xE0, 16,                       // ST7735_GMCTRP1, Gamma adjustments (pos. polarity)
        0x09, 0x16, 0x09, 0x20,             //(Not entirely necessary, but provides
        0x21, 0x1B, 0x13, 0x19,             // accurate colors)
        0x17, 0x15, 0x1E, 0x2B,
        0x04, 0x05, 0x02, 0x0E);
    sendCmd(0xE1, 16,                       // ST7735_GMCTRN1, Gamma adjustments (neg. polarity)
        0x0B, 0x14, 0x08, 0x1E,             //(Not entirely necessary, but provides
        0x22, 0x1D, 0x18, 0x1E,             //accurate colors)
        0x1B, 0x1A, 0x24, 0x2B,
        0x06, 0x06, 0x02, 0x0F);
    delayMs(10);

    sendCmd(0x2A, 4,                        // ST7735_CASET, column address
        0x00, 0x00,                         // x_start = 0
        0x00, 0x7F);                        // x_end = 127
    sendCmd(0x2B, 4,                        // ST7735_RASET, row address
        0x00, 0x00,                         // x_start = 0
        0x00, 0x9F);                        // x_end = 159

    sendCmd(0x13, 0);                       // ST7735_NORON, normal display mode on
    delayMs(10);
    sendCmd(0x29, 0);                       // ST7735_DISPON, display on
    delayMs(150);

}





}
