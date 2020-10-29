#include "display_st7735.h"
#include "miosix.h"
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY

namespace mxgui {

/*
* printf debugging function
*/
int __io_putchar(int ch){
     ITM_SendChar(ch);
     return(ch);
}

int  _print(int file, const char *ptr, int len) {  
    int DataIdx;   
    for (DataIdx = 0; DataIdx < len; DataIdx++){   
        __io_putchar(*ptr++); 
    }   
    return len; }

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
    writeCmd(0x29);
}

void DisplayImpl::doTurnOff() {
    writeCmd(0x28);     //ST7735_DISPOFF TODO: should be followed by SLPIN
    delayMs(150);
}

//No function to set brightness
//TODO: maybe the backlight??
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

    writeCmd(0x2C);     //RAMWR, to write the GRAM   
    //Send data to write on GRAM
    for(int i=0; i < numPixels; i++) {       
        writeData(msb);
        writeData(lsb);        
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
    //writeRamBegin();
    writeRam(msb);
    writeRam(lsb);
}

void DisplayImpl::line(Point a, Point b, Color color) {
    unsigned char lsb = color & 0xFF;
    unsigned char msb = (color >> 8) & 0xFF;    
    
    if(a.y() == b.y())
    {
        imageWindow(Point(min(a.x(), b.x()), a.y()),
                    Point(max(a.x(), b.x()), a.y()));
        int numPixels = abs(a.x() - b.x());

        SPITransaction t;
        //writeRamBegin(); 
        //Send data to write on GRAM
        for(int i=0; i < numPixels; i++) { 
            writeRam(msb);
            delayUs(1);
            writeRam(lsb);
            delayUs(1);
        }
        return;
    }
    //Vertical line speed optimization
    if(a.x() == b.x())
    {
        textWindow(Point(a.x(), min(a.y(), b.y())),
                    Point(a.x(), max(a.y(), b.y())));
        int numPixels = abs(a.y() - b.y());
        
        SPITransaction t;
        //writeRamBegin();
        //Send data to write on GRAM
        for(int i=0; i < numPixels; i++) { 
            writeRam(msb);
            delayUs(1);
            writeRam(lsb);
            delayUs(1);
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

    SPITransaction t;
    //writeRamBegin();
    //Send data to write on GRAM
    for(int i=0; i < length; i++) { 
        lsb = colors[i] & 0xFF;
        msb = (colors[i] >> 8) & 0xFF;

        writeRam(msb);
        delayUs(1);
        writeRam(lsb);
        delayUs(1);
    }
}

//TODO: vedere se funziona questa implementazione
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

        SPITransaction t;
        //writeRamBegin();
        for(int i=0; i <= numPixels; i++)
        {
            lsb = imgData[i] & 0xFF;
            msb = (imgData[i] >> 8) & 0xFF;
            writeRam(msb);
            delayUs(1);
            writeRam(lsb);
            delayUs(1);
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
        
        SPITransaction t;
        //writeRamBegin();

        unsigned int numPixels = (p2.x() - p1.x() + 1) * (p2.y() - p1.y() + 1);
        return pixel_iterator(numPixels);
}

//Destructor
DisplayImpl::~DisplayImpl() {}

//Constructor
DisplayImpl::DisplayImpl(): which(0) { 
    //TODO: RCC configuration
    {
        FastInterruptDisableLock dLock;

        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        SPI1->CR1 = 0;
        SPI1->CR1 = SPI_CR1_SSM  //Software cs
                  | SPI_CR1_SSI  //Hardware cs internally tied high
                  | (3<<3)        //Divide input clock by 16: 84/16=5.25MHz
                  | SPI_CR1_MSTR //Master mode
                  | SPI_CR1_SPE;  //SPI enabled
        
        scl::mode(Mode::ALTERNATE);     scl::alternateFunction(5);
        sda::mode(Mode::ALTERNATE);     sda::alternateFunction(5);
        csx::mode(Mode::OUTPUT);
        dcx::mode(Mode::OUTPUT);
        resx::mode(Mode::OUTPUT);
    }

    resx::high();
    Thread::sleep(250);
    resx::low();
    Thread::sleep(250);
    resx::high();

    csx::high();
    dcx::high();

    sendInitSeq();
    
    doTurnOn();
    setFont(droid11);
    setTextColor(make_pair(white, black));
}

void DisplayImpl::window(Point p1, Point p2) {
    //Setting column bounds, ST7735_CASET
    unsigned char buff_caset[4];
    buff_caset[0] = p1.x()>>8 & 255;      buff_caset[1] = p1.x() & 255;
    buff_caset[2] = p2.x()>>8 & 255;      buff_caset[3] = p2.x() & 255;
    //writeReg(0x2A, buff_caset, sizeof(buff_caset));
    
    //Setting row bounds, ST7735_RASET
    unsigned char buff_raset[4];
    buff_raset[0] = p1.y()>>8 & 255;      buff_raset[1] = p1.y() & 255;
    buff_raset[2] = p2.y()>>8 & 255;      buff_raset[3] = p2.y() & 255;
    //writeReg(0x2B, buff_raset, sizeof(buff_raset));
}

void DisplayImpl::sendInitSeq() {
    
    //SWRESET
    writeCmd(0x01);
    delayMs(150);
    //SLPOUT
    writeCmd(0x11);
    delayMs(255);

    //COLMOD, color mode: 16-bit/pixel
    writeCmd(0x3A);     writeData(0x05);
    //FRMCTR1, normal mode frame rate
    writeCmd(0xB1);     writeData(0x00); writeData(0x06); writeData(0x03);
    //MADCTL, row/col addr, bottom-top refresh
    writeCmd(0x36);     writeData(0x08);
    //DISSET5, display settings
    writeCmd(0xB6);     writeData(0x15); writeData(0x02);
    //INVCTR, line inversion active
    writeCmd(0xB4);     writeData(0x00);
    //PWCTR1, default (4.7V, 1.0 uA)
    writeCmd(0xC0);     writeData(0x02); writeData(0x70);
    //PWCTR2, default (VGH=14.7V, VGL=-7.35V)
    writeCmd(0xC1);     writeData(0x05);
    //PWCTR3, opamp current small, boost frequency
    writeCmd(0xC2);     writeData(0x01); writeData(0x02);
    //PWCTR4, bclk/2, opamp current small and medium low
    writeCmd(0xC3);     writeData(0x8A); writeData(0x2A);
    //VMCTR1, VCOMH=4V VCOML=-1.1
    writeCmd(0xC5);     writeData(0x3C); writeData(0x38);
    //PWCTR6, power control (partial mode+idle)
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
    
    //CASET, column address
    writeCmd(0x2A);     writeData(0x00); writeData(0x00);   //x_start = 0
                        writeData(0x00); writeData(0x7F);   //x_end = 127
    //RASET, row address
    writeCmd(0x2B);     writeData(0x00); writeData(0x00);   //x_start = 0
                        writeData(0x00); writeData(0x9F);   //x_end = 159
    
    //NORON, normal display mode on
    writeCmd(0x13);
    //NOP
    writeCmd(0x00);
}

} //mxgui

#endif //_BOARD_STM32F4DISCOVERY
