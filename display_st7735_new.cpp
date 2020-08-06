#include "display_st7735.h"
#include "miosix.h"
#include <algorithm>
#include <cstdarg>

using namespace std;
using namespace miosix;

#ifdef _BOARD_STM32F4DISCOVERY


//Control interface
typedef Gpio<GPIOA_BASE, 5> scl; //SPI1_SCK (af5)
typedef Gpio<GPIOA_BASE, 7> sda; //SPI1_MOSI (af5)
typedef Gpio<GPIOB_BASE, 6> csx; //free I/O pin
typedef Gpio<GPIOC_BASE, 7> resx; //free I/O pin
typedef Gpio<GPIOA_BASE, 9> dcx; //free I/O pin, used only in 4-line SPI
//rdx not used in serial, only parallel
//te not used in serial, only parallel

/* OLED connection example
typedef Gpio<GPIOB_BASE,3> sck;
typedef Gpio<GPIOB_BASE,4> miso; //Not used
typedef Gpio<GPIOB_BASE,5> mosi;
typedef Gpio<GPIOB_BASE,7> cs;

typedef Gpio<GPIOB_BASE,8> dc;
typedef Gpio<GPIOB_BASE,15> reset;
*/


static void spiInit()
{
    scl::mode(Mode::ALTERNATE);
    scl::alternateFunction(5);
    sda::mode(Mode::ALTERNATE);
    sda::alternateFunction(5);
    csx::mode(Mode::OUTPUT);
    csx::high();
    //dcx::mode(Mode::OUTPUT);
    //resx::mode(Mode::OUTPUT);

    {
        FastInterruptDisableLock dLock;
        RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
        RCC_SYNC();
    }
    //Master mode no hardware CS pin
    //Note: SPI3 is attached on the 42MHz APB2 bus, so the clock is set
    //to APB2/2/2=10.5MHz. This overclocking the SSD1332 by 500KHz
    SPI3->CR1=SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | SPI_CR1_BR_0;
    SPI3->CR2=0;
    SPI3->CR1 |= SPI_CR1_SPE; //Enable the SPI
}

/**
 * Send and receive a byte through SPI1
 * \param data byte to send
 * \return byte received
 */
 static unsigned char spiSendRecv(unsigned char data=0)
 {
     SPI3->DR=data;
     while((SPI3->SR & SPI_SR_RXNE)==0) ;
     return SPI3->DR;
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


namespace mxgui {

/*
* printf debugging function
*/
int __io_putchar(int ch){
     ITM_SendChar(ch);
     return(ch);
}


/**
 * Function to attach the display we are using.
 */
void registerDisplayHook(DisplayManager& dm) {
    dm.registerDisplay(&DisplayST7735::instance());
}

/**
 * Class DisplayImpl
 */
DisplayST7735& DisplayST7735::instance() {
    static DisplayST7735 instance;
    return instance;
}

DisplayST7735::DisplayST7735()  {
    spiInit();
    __io_putchar(22);
    dcx::mode(Mode::OUTPUT);
    resx::mode(Mode::OUTPUT);

    resx::high();
    Thread::sleep(1);
    resx::low();
    delayUs(100);
    resx::high();
    delayUs(100);

    cmd(0x01);                                          // ST7735_SWRESET
    delayMs(150);
    cmd(0x11);                                          // ST7735_SLPOUT
    delayMs(255);
    cmd(0x3a);  data(0x01); data(0x05);                 // ST7735_COLMOD, color mode: 16-bit/pixel
    cmd(0xb1);  data(0x03); data(0x01); data(0x2c);     // ST7735_FRMCTR1, normal mode frame rate
                data(0x2d);
    delayMs(10);
    cmd(0x36);  data(0x01); data(0x08);                 // ST7735_MADCTL, row/col addr, bottom-top refresh
    cmd(0xB6);  data(0x02); data(0x15); data(0x02);     // ST7735_DISSET5, display settings
    cmd(0xB4);  data(0x01); data(0x00);                 // ST7735_INVCTR, line inversion active
    cmd(0xC0);  data(0x02); data(0x02); data(0x70);     // ST7735_PWCTR1, default (4.7V, 1.0 uA)
    delayMs(10);
    cmd(0xC1);  data(0x01); data(0x05);                 // ST7735_PWCTR2, default (VGH=14.7V, VGL=-7.35V)
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
    cmd(0x2A);  data(0x04); data(0x00); data(0x00);     // ST7735_CASET, column address: x_start = 0, x_end = 127
                data(0x00); data(0x7F);
    cmd(0x2B);  data(0x04); data(0x00); data(0x00);     // ST7735_RASET, row address: x_start = 0, x_end = 159
                data(0x00); data(0x9F);
    cmd(0x13);                                          // ST7735_NORON, normal display mode on
    delayMs(10);
    //clear(0);
    //update();
    cmd(0x29);                                          // ST7735_DISPON, display on
}

void DisplayST7735::doTurnOn() {
    cmd(0x29);          //ST7735_DISPOFF TODO: should be followed by SLPIN
    delayMs(150);
}

void DisplayST7735::doTurnOff() {
    cmd(0x28);          //ST7735_DISPOFF TODO: should be followed by SLPIN
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
    csx::high();
    delayUs(1);
}

} //namespace mxgui
#endif
