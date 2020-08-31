#include "display_st7735.h"
#include "miosix.h"
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

namespace mxgui {

//Control interface
typedef Gpio<GPIOA_BASE, 5> scl; //SPI1_SCK (af5)
typedef Gpio<GPIOA_BASE, 7> sda; //SPI1_MOSI (af5)
typedef Gpio<GPIOB_BASE, 6> csx; //free I/O pin
typedef Gpio<GPIOC_BASE, 7> resx; //free I/O pin
typedef Gpio<GPIOA_BASE, 9> dcx; //free I/O pin, used only in 4-line SPI
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
static void sendCmd(unsigned char cmd, int len=0, ...) {
    dcx::low(); //tells there is a command
    csx::low(); //enables SPI transmission
    sp1sendRev(cmd);
    csx::high(); //stops the transmission
    delayUs(1); //TODO: delay
    dcx::high(); //tells there are parameters
    if(len != 0){
        va_list arg;
        va_start(arg, len);
        for(int i = 0; i < len; i++)
        {
            csx::low();
            sp1sendRev(va_arg(arg, int));
            csx::high();
            delayUs(1);
        }
        va_end(arg);
    }
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
    //TODO: RCC configuration
    {
        FastInterruptDisableLock dLock;

        //Enable all gpios
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                      | RCC_AHB1ENR_GPIOBEN
                      | RCC_AHB1ENR_GPIOCEN;


        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        SPI1->CR1 = 0;
        SPI1->CR1 = SPI_CR1_SSM  //Software cs
                  | SPI_CR1_SSI  //Hardware cs internally tied high
                  | SPI_CR1_MSTR //Master mode
                  | SPI_CR1_SPE  //SPI enabled
                  | (3<<3);        //Divide input clock by 16: 84/16=5.25MHz
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
        resx::mode(Mode::OUTPUT);

        //TODO: RCC something...
    }

    const unsigned char initCmds[]={
        0x01, 0x00,                         // ST7735_SWRESET
        0x11, 0x00,                         // ST7735_SLPOUT
        0x3A, 0X01, 0x05,                   // ST7735_COLMOD, color mode: 16-bit/pixel
        0xB1, 0x03, 0x01, 0x2C, 0x2D,       // ST7735_FRMCTR1, normal mode frame rate
        0x36, 0x01, 0x08,                   // ST7735_MADCTL, row/col addr, bottom-top refresh
        0xB6, 0x02, 0x15, 0x02,             // ST7735_DISSET5, display settings
        0xB4, 0x01, 0x00,                   // ST7735_INVCTR, line inversion active
        0xC0, 0x02, 0x02, 0x70,             // ST7735_PWCTR1, default (4.7V, 1.0 uA)
        0xC1, 0x01, 0x05,                   // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
        0xC2, 0x02, 0x01, 0x02,             // ST7735_PWCTR3, opamp current small, boost frequency
        0xC5, 0x02, 0x3C, 0x38,             // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
        0xFC, 0x02, 0x11, 0x15,             // ST7735_PWCTR6, power control (partial mode+idle) TODO: get rid of it
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
        0x2A, 0x04,                         // ST7735_CASET, column address
            0x00, 0x00,                     // x_start = 0
            0x00, 0x7F,                     // x_end = 127
        0x2B, 0x04,                         // ST7735_RASET, row address
            0x00, 0x00,                     // x_start = 0
            0x00, 0x9F,                     // x_end = 159
        0x13, 0x00,                         // ST7735_NORON, normal display mode on
        0x29, 0x00,                         // ST7735_DISPON, display on
        0x2C, 0x00,                         // ST7735_RAMWR, write on GRAM
        0x00                                //END while
    };

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

    sendCmd(0x2C, 0);                       // ST7735_RAMWR, write in GRAM

    /*
    delayMs(4000);
    Thread::sleep(4000);
    sendCmd(0x28, 0);                       // ST7735_DISPOFF, display off
    delayMs(4000);
    Thread::sleep(4000);
    */

    clear(black);
}

void DisplayImpl::doTurnOff() {
    sendCmd(0x28, 0);   //ST7735_DISPOFF TODO: should be followed by SLPIN
    delayMs(150);
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
    imageWindow(p1, p2);
    sendCmd(0x2C, 0);       //ST7735_RAMWR, to enable write on GRAM
    int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);

    unsigned char msb = 0x00;
    unsigned char lsb = 0x00;

    //Send data to write on GRAM
    for(int i=0; i < numPixels; i++) {
        dcx::high();
        csx::low();
        delayUs(1);
        msb = color >> 8;
        lsb = color & 0xFF;
        sp1sendRev(msb);
        sp1sendRev(lsb);
        csx::high();
        delayUs(1);
    }
}

void DisplayImpl::beginPixel() {
    imageWindow(Point(0,0), Point(width-1, height-1));
}

void DisplayImpl::setPixel(Point p, Color color) {
    setCursor(p);
    sendCmd(0x2C);

    unsigned char msb = 0x00;
    unsigned char lsb = 0x00;

    //Send data to write on GRAM
    dcx::high();
    csx::low();
    delayUs(1);
    msb = color >> 8;
    lsb = color & 0xFF;
    sp1sendRev(msb);
    sp1sendRev(lsb);
    csx::high();
    delayUs(1);
}

void DisplayImpl::line(Point a, Point b, Color color) {
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()),
                    Point(max(a.x(), b.x()), a.y()));
        sendCmd(0x2C);      //ST7735_RAMWR, to write in GRAM
        int numPixels = abs(a.x() - b.x());

        unsigned char msb = 0x00;
        unsigned char lsb = 0x00;

        //Send data to write on GRAM
        for(int i=0; i < numPixels; i++) {
            dcx::high();
            csx::low();
            delayUs(1);
            msb = color >> 8;
            lsb = color & 0xFF;
            sp1sendRev(msb);
            sp1sendRev(lsb);
            csx::high();
            delayUs(1);
        }
        return;
    }
    //Vertical line speed optimization
    if(a.x() == b.x())
    {
        textWindow(Point(a.x(), min(a.y(), b.y())),
                    Point(a.x(), max(a.y(), b.y())));
        sendCmd(0x2C);      //ST7735_RAMWR, to write in GRAM
        int numPixels = abs(a.y() - b.y());

        unsigned char msb = 0x00;
        unsigned char lsb = 0x00;

        //Send data to write on GRAM
        for(int i=0; i < numPixels; i++) {
            dcx::high();
            csx::low();
            delayUs(1);
            msb = color >> 8;
            lsb = color & 0xFF;
            sp1sendRev(msb);
            sp1sendRev(lsb);
            csx::high();
            delayUs(1);
        }
        return;
    }
    //General case, always works but it is much slower due to the display
    //not having fast random access to pixels
    Line::draw(*this, a, b, color);
}

void DisplayImpl::scanLine(Point p, const Color *colors, unsigned short length) {
    imageWindow(p, Point(width - 1, p.y()));
    sendCmd(0x2C);      //ST7735_RAMWR, to write in GRAM

    if(p.x() + length > width) { return; }

    unsigned char msb = 0x00;
    unsigned char lsb = 0x00;

    //Send data to write on GRAM
    for(int i=0; i < length; i++) {
        dcx::high();
        csx::low();
        delayUs(1);
        msb = colors[i] >> 8;
        lsb = colors[i] & 0xFF;
        sp1sendRev(msb);
        sp1sendRev(lsb);
        csx::high();
        delayUs(1);
    }
}

//TODO: vedere se funziona questa implementazione
Color *DisplayImpl::getScanLineBuffer() {
    if(buffer == 0) { buffer = new Color[getWidth()]; }
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
        //Optimized version for memory-loaded images
        imageWindow(p, Point(xEnd, yEnd));
        sendCmd(0x2C);
        int numPixels = img.getHeight() * img.getWidth();

        unsigned char msb = 0x00;
        unsigned char lsb = 0x00;

        for(int i=0;i<=numPixels;i++)
        {
            dcx::high();
            csx::low();
            delayUs(1);
            msb = *imgData >> 8;
            lsb = *imgData & 0xFF;
            sp1sendRev(msb);
            sp1sendRev(lsb);
            csx::high();
            delayUs(1);
            imgData++;
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

void DisplayImpl::update() {}

DisplayImpl::pixel_iterator DisplayImpl::begin(Point p1,
    Point p2, IteratorDirection d) {
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

        sendCmd(0x2C);      //ST7735_RAMWR, write to GRAM

        unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
        return pixel_iterator(numPixels);
    }

DisplayImpl::~DisplayImpl() {
    if(buffer) delete[] buffer;
}

DisplayImpl::DisplayImpl(): which(0) {
    doTurnOn();
    setFont(droid11);
    setTextColor(make_pair(white, black));
}

/**
 * Methods for window size handling.
 */
void DisplayImpl::setCursor(Point p) { window(p, p); }

void DisplayImpl::textWindow(Point p1, Point p2) {
    sendCmd(0x36, 1, 0x20);
    window(p1, p2);
}

void DisplayImpl::imageWindow(Point p1, Point p2) {
    sendCmd(0x36, 1, 0x00);
    window(p1, p2);
}

void DisplayImpl::window(Point p1, Point p2) {
    //Setting column bounds, ST7735_CASET
    unsigned char buff_caset[4];
    buff_caset[0] = p1.x()>>8;      buff_caset[1] = p1.x() & 255;
    buff_caset[2] = p2.x()>>8;      buff_caset[3] = p2.x() & 255;
    sendCmd(0x2A, sizeof(buff_caset),
        buff_caset[0], buff_caset[1],
        buff_caset[2], buff_caset[3]);

    //Setting row bounds, ST7735_RASET
    unsigned char buff_raset[4];
    buff_raset[0] = p1.y()>>8;      buff_raset[1] = p1.y() & 255;
    buff_raset[2] = p2.y()>>8;      buff_raset[3] = p2.y() & 255;
    sendCmd(0x2B, sizeof(buff_raset),
        buff_raset[0], buff_raset[1],
        buff_raset[2], buff_raset[3]);
}

} //mxgui

#endif //_BOARD_STM32F4DISCOVERY
