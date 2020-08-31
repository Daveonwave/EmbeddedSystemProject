
/*


    WRONG version
    i'm actually sending the number of the parameters to the MCU
    resulting in a black screen at some point






*/



#include "display_st7735.h"
#include "miosix.h"
#include <algorithm>
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

namespace mxgui{

//Control interface
typedef Gpio<GPIOB_BASE, 3> scl; //SPI1_SCK (af5)
typedef Gpio<GPIOB_BASE, 5> sda; //SPI1_MOSI (af5)
typedef Gpio<GPIOB_BASE, 4> csx; //free I/O pin
typedef Gpio<GPIOC_BASE, 6> resx; //free I/O pin
typedef Gpio<GPIOA_BASE, 8> dcx; //free I/O pin, used only in 4-line SPI
//rdx not used in serial, only parallel
//te not used in serial, only parallel

/*
* printf debugging function
*/
int __io_putchar(int ch){
     ITM_SendChar(ch);
     return(ch);
}

/**
 * Class DisplayImpl
 */
DisplayST7735& DisplayST7735::instance() {
    static DisplayST7735 instance;
    return instance;
}

/**
 * Function to attach the display we are using.
 */
void registerDisplayHook(DisplayManager& dm) {
    dm.registerDisplay(&DisplayST7735::instance());
}

/*
    inizialization of the SPI and internal registers
    display clock speed = 1/60ns = 16,6 MHz
    SPI1 connected to APB2 bus: max frequency 84 MHz
    Dividing max frequency by (2<<2 = 8) we obtain 10,5 MHz
*/
static void spiInit()
{
    scl::mode(Mode::ALTERNATE);
    scl::alternateFunction(5);
    sda::mode(Mode::ALTERNATE);
    sda::alternateFunction(5);
    csx::mode(Mode::OUTPUT);
    csx::high();
    dcx::mode(Mode::OUTPUT);
    resx::mode(Mode::OUTPUT);
    __io_putchar('I');
    __io_putchar('\n');

    {
        FastInterruptDisableLock dLock;
        //RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        RCC_SYNC();

        SPI1->CR1 = 0;
        SPI1->CR2 = 0;
        //Master mode, no hardware CS pin
        SPI1->CR1 = SPI_CR1_SSM
                  | SPI_CR1_SSI
                  | SPI_CR1_MSTR
                  | (3<<3);
        //Enable the SPI
        SPI1->CR1 |= SPI_CR1_SPE;
    }
}
/*
per risolvere l'overloading, il compilatore esegue name mangling, ovvero "traduce" le funzioni C/C++ in una stringa così formata:
_Z : per funzione c++
20 : numero di caratteri (puntatore) dopo i quali troverò la funzione
nome funzione
parametri (V sta per void)
*/

/**
 * Send and receive a byte through SPI1
 * \param data byte to send
 * \return byte received  -  Note: reading back SPI1->DR is necessary
 */
 static unsigned char spiSendRecv(unsigned char data=0)
 {
     SPI1->DR=data;
     while((SPI1->SR & SPI_SR_RXNE)==0) ;
     return SPI1->DR;
 }

 /**
  * Send a command to the ST7735 display
  * \param c command to be sent
  */
 static void cmd(unsigned char c)
 {
     dcx::low();

     csx::low();
     delayUs(1);
     spiSendRecv(c);
     csx::high();
     delayUs(1);
 }

 /**
  * Send data to the ST7735 display
  * \param d data to be sent
  */
 static void data(unsigned char d)
 {
     dcx::high();

     csx::low();
     delayUs(1);
     spiSendRecv(d);
     csx::high();
     delayUs(1);
 }

void DisplayST7735::doTurnOn() {
    cmd(0x29);          //ST7735_DISPON
    cmd(0x11);          //ST7735_SLEEPOUT
    __io_putchar('N');
    __io_putchar('\n');
    delayMs(150);
}

void DisplayST7735::doTurnOff() {
    cmd(0x28);          //ST7735_DISPOFF
    cmd(0x10);          //ST7735_SLEEPIN
    __io_putchar('F');
    __io_putchar('\n');
    delayMs(150);
}

void DisplayST7735::update()
{
    static const unsigned char xStart=28;
    static const unsigned char xEnd=91;
    static const unsigned char yStart=0;
    static const unsigned char yEnd=63;

    //Setting column bounds, ST7735_CASET
    cmd(0x2A);  data(0x00); data(xStart);
                data(0x00); data(xEnd);
    //Setting row bounds, ST7735_RASET
    cmd(0x2B);  data(0x00); data(yStart);
                data(0x00); data(yEnd);

    dcx::high();
    csx::low();
    delayUs(1);
    //for(int i=0;i<fbSize;i++) spiSendRecv(backbuffer[i]);
    doTurnOn();
    csx::high();
    delayUs(1);
}

DisplayST7735::DisplayST7735()  {
    //initialize SPI and internal registers
    spiInit();

    //global reset
    resx::high();
    Thread::sleep(1);
    resx::low();
    delayUs(100);
    resx::high();
    delayUs(100);

    //initialization sequence
    /*


        TOTALLY WRONG, I'M SENDING NUMBER OF PARAMETERS TO MCU BEFORE ACTUALLY SENDING THEM


    */
    cmd(0x01);                                          // ST7735_SWRESET
    delayMs(150);
    cmd(0x11);                                          // ST7735_SLPOUT
    delayMs(255);

    cmd(0x3A);  data(0x01); data(0x05);                 // ST7735_COLMOD, color mode: 16-bit/pixel
    delayMs(10);

    cmd(0xB1);  data(0x03); data(0x00); data(0x06);     // ST7735_FRMCTR1, normal mode frame rate
                data(0x03);                             //maybe try with 0x01, 0x2C, 0x2D
    delayMs(10);
    cmd(0x36);  data(0x01); data(0x08);                 // ST7735_MADCTL, row/col addr, bottom-top refresh

    //after the below line the screen becomes black
    cmd(0xB6);  data(0x02); data(0x15); data(0x00);     // ST7735_DISSET5, display settings (2nd param 0x00 or 0x02?)
    cmd(0xB4);  data(0x01); data(0x00);                 // ST7735_INVCTR, line inversion active //TODO maybe 0x07 (no inverison)
    cmd(0xC0);  data(0x02); data(0x02); data(0x70);     // ST7735_PWCTR1, default (4.7V, 1.0 uA)
    delayMs(10);
    cmd(0xC1);  data(0x01); data(0x05);                 // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
    //after the below line the screen returns white
    delayMs(1000);
    cmd(0xC2);  data(0x02); data(0x01); data(0x02);     // ST7735_PWCTR3, opamp current small, boost frequency
    cmd(0xC5);  data(0x02); data(0x3C); data(0x38);     // ST7735_VMCTR1, VCOMH=4V VCOML=-1.1
    delayMs(10);
    cmd(0xFC);  data(0x02); data(0x11); data(0x15);     // ST7735_PWCTR6, power control (partial mode+idle) TODO: get rid of it
    cmd(0xE0);  data(0x10); data(0x09); data(0x16);     // ST7735_GMCTRP1, Gamma adjustments (pos. polarity)
                data(0x09); data(0x20); data(0x21);
                data(0x1B); data(0x13); data(0x19);
                data(0x17); data(0x15); data(0x1E);
                data(0x2B); data(0x04); data(0x05);
                data(0x02); data(0x0E);
    cmd(0xE1);  data(0x10); data(0x0B); data(0x14);     // ST7735_GMCTRN1, Gamma adjustments (neg. polarity)
                data(0x08); data(0x1E); data(0x22);
                data(0x1D); data(0x18); data(0x1E);
                data(0x1B); data(0x1A); data(0x24);
                data(0x2B); data(0x06); data(0x06);
                data(0x02); data(0x0F);
    delayMs(10);
    cmd(0x2A);  data(0x04); data(0x00); data(0x01);     // ST7735_CASET, column address: x_start = 1, x_end = 127
                data(0x00); data(0x7F);
    cmd(0x2B);  data(0x04); data(0x00); data(0x01);     // ST7735_RASET, row address: x_start = 1, x_end = 159
                data(0x00); data(0x9F);
    cmd(0x13);                                          // ST7735_NORON, normal display mode on
    delayMs(10);
    //clear(0);
    //update();
    cmd(0x29);                                          // ST7735_DISPON, display on
    delayMs(255);
    clear(RED);

}

DisplayST7735::~DisplayST7735()  {}

//override functions
void DisplayST7735::drawRectangle(Point a, Point b, Color c) {}
void DisplayST7735::doSetBrightness(int brt) {}
pair<short int, short int> DisplayST7735::doGetSize() const{
    return make_pair(height, width);
}

void DisplayST7735::write(Point p, const char *text) {
    font.draw(*this, textColor, p, text);
}

void DisplayST7735::clippedWrite(Point p, Point a,  Point b, const char *text) {
    font.clippedDraw(*this, textColor, p, a, b, text);
}

void DisplayST7735::clear(Color color) {
    clear(Point(0,0), Point(width-1,  height-1), color);
}

void DisplayST7735::clear(Point p1, Point p2, Color color) {
    imageWindow(p1, p2);
    cmd(0x2C);       //ST7735_RAMWR, to enable write on GRAM
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
        spiSendRecv(msb);
        spiSendRecv(lsb);
        csx::high();
        delayUs(1);
    }
}

void DisplayST7735::textWindow(Point p1, Point p2) {
    cmd(0x36);
    data(0x20);
    window(p1, p2);
}

void DisplayST7735::imageWindow(Point p1, Point p2) {
    cmd(0x36);
    data(0x00);
    window(p1, p2);
}

void DisplayST7735::window(Point p1, Point p2) {
    //Setting column bounds, ST7735_CASET
    unsigned char buff_caset[4];
    buff_caset[0] = p1.x()>>8;      buff_caset[1] = p1.x() & 255;
    buff_caset[2] = p2.x()>>8;      buff_caset[3] = p2.x() & 255;
    cmd(0x2A);
    data(buff_caset[0]);
    data(buff_caset[1]);
    data(buff_caset[2]);
    data(buff_caset[3]);

    //Setting row bounds, ST7735_RASET
    unsigned char buff_raset[4];
    buff_raset[0] = p1.y()>>8;      buff_raset[1] = p1.y() & 255;
    buff_raset[2] = p2.y()>>8;      buff_raset[3] = p2.y() & 255;
    cmd(0x2B);
    data(buff_raset[0]);
    data(buff_raset[1]);
    data(buff_raset[2]);
    data(buff_raset[3]);
}

void DisplayST7735::beginPixel() {
    imageWindow(Point(0,0), Point(width-1, height-1));
}

void DisplayST7735::setPixel(Point p, Color color) {
    setCursor(p);
    cmd(0x2C);

    unsigned char msb = 0x00;
    unsigned char lsb = 0x00;

    //Send data to write on GRAM
    dcx::high();
    csx::low();
    delayUs(1);
    msb = color >> 8;
    lsb = color & 0xFF;
    spiSendRecv(msb);
    spiSendRecv(lsb);
    csx::high();
    delayUs(1);
}

DisplayST7735::pixel_iterator DisplayST7735::begin(Point p1, Point p2, IteratorDirection d) {
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

    cmd(0x2C);      //ST7735_RAMWR, write to GRAM

    unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
    return pixel_iterator(numPixels);
}

void DisplayST7735::line(Point a, Point b, Color color) {
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()),
                    Point(max(a.x(), b.x()), a.y()));
        cmd(0x2C);      //ST7735_RAMWR, to write in GRAM
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
            spiSendRecv(msb);
            spiSendRecv(lsb);
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
        cmd(0x2C);      //ST7735_RAMWR, to write in GRAM
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
            spiSendRecv(msb);
            spiSendRecv(lsb);
            csx::high();
            delayUs(1);
        }
        return;
    }
    //General case, always works but it is much slower due to the display
    //not having fast random access to pixels
    Line::draw(*this, a, b, color);

}

void DisplayST7735::scanLine(Point p, const Color *colors, unsigned short length) {

    imageWindow(p, Point(width - 1, p.y()));
    cmd(0x2C);      //ST7735_RAMWR, to write in GRAM

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
        spiSendRecv(msb);
        spiSendRecv(lsb);
        csx::high();
        delayUs(1);
    }
}

Color *DisplayST7735::getScanLineBuffer() {
    if(buffer == 0) { buffer = new Color[getWidth()]; }
    return buffer;
}

void DisplayST7735::scanLineBuffer(Point p, unsigned short length) {
    scanLine(p, buffer, length);
}
void DisplayST7735::drawImage(Point p, const ImageBase& img) {

    short int xEnd = p.x() + img.getWidth() - 1;
    short int yEnd = p.y() + img.getHeight() - 1;
    if(xEnd >= width || yEnd >= height) { return; }

    const unsigned short *imgData = img.getData();
    if(imgData != 0)
    {
        //Optimized version for memory-loaded images
        imageWindow(p, Point(xEnd, yEnd));
        cmd(0x2C);
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
            spiSendRecv(msb);
            spiSendRecv(lsb);
            csx::high();
            delayUs(1);
            imgData++;
        }
    }
    else { img.draw(*this,p); }

}
void DisplayST7735::clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) {
    img.clippedDraw(*this,p,a,b);
}


} //namespace mxgui
#endif
