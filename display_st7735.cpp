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
typedef Gpio<GPIOA, 4> csx; //SPI1_NSS (af5)
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

//TODO: verify if it is correct 
void DisplayImpl::clear(Point p1, Point p2, Color color) {}

void DisplayImpl::beginPixel() {}

void DisplayImpl::setPixel(Point p, Color color)
{
    int offset = p.x() + p.y() * width;
    if(offset < 0 || offset >= numPixels) { return; }
    *(framebuffer1+offset) = color;
}






}