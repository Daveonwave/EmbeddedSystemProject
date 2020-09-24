#ifndef MXGUI_LIBRARY
#error "This is header is private, it can be used only within mxgui."
#error "If your code depends on a private header, it IS broken."
#endif //MXGUI_LIBRARY

#ifndef DISPLAY_ST7735H
#define DISPLAY_ST7735H

#ifdef _BOARD_STM32F4DISCOVERY

#include <config/mxgui_settings.h>
#include "display.h"
#include "point.h"
#include "color.h"
#include "font.h"
#include "image.h"
#include "iterator_direction.h"
#include "misc_inst.h"
#include "line.h"
#include "miosix.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdarg>

using namespace std;
using namespace miosix;

namespace mxgui {

//This display can be 12, 16 or 18 bits per pixel, check that the color depth is properly
//configured
//TODO: add MXGUI_COLOR_DEPTH_12_BIT and MXGUI_COLOR_DEPTH_18_BIT which are not present
//in mxgui_settings.h
#ifndef MXGUI_COLOR_DEPTH_16_BIT
#error The ST7735 driver requires a color depth of 12 or 16 or 18 bits per pixel
#endif

#ifndef MXGUI_ORIENTATION_VERTICAL
#error Unsupported orientation
#endif

//Control interface
typedef Gpio<GPIOB_BASE, 3> scl; //SPI1_SCK (af5)
typedef Gpio<GPIOB_BASE, 5> sda; //SPI1_MOSI (af5)
typedef Gpio<GPIOB_BASE, 4> csx; //free I/O pin
typedef Gpio<GPIOC_BASE, 6> resx; //free I/O pin
typedef Gpio<GPIOA_BASE, 8> dcx; //free I/O pin, used only in 4-line SPI
//rdx not used in serial, only parallel
//te not used in serial, only parallel

//A falling edge of CSX enables the SPI1 transaction1
class SPITransaction
{
public:
    SPITransaction()  { csx::low();  }
    ~SPITransaction() { csx::high(); }
};

//A falling edge on DCX means that the transmitted byte is a command
class CommandTransaction
{
public:
    CommandTransaction()  { dcx::low();  }
    ~CommandTransaction() { dcx::high(); }
};

class DisplayImpl : public Display
{
public:
    /**
     * \return an instance to this class(singleton)
     */
    static DisplayImpl& instance();

    /**
     * Turn the display On after it has been turned Off.
     * Display initial state On.
     */
    void doTurnOn() override;

    /**
     * Turn the display Off. It can be later turned back On.
     */
    void doTurnOff() override;

    /**
     * Set display brightness. Depending on the underlying driver,
     * may do nothing.
     * \param brt from 0 to 100
     */
    void doSetBrightness(int brt) override;

    /**
     * \return a pair with the display height and width
     */
    std::pair<short int, short int> doGetSize() const override;

    /**
     * Write text to the display. If text is too long it will be truncated
     * \param p point where the upper left corner of the text will be printed
     * \param text, text to print.
     */
    void write(Point p, const char *text) override;

    /**
     * Write part of text to the display
     * \param p point of the upper left corner where the text will be drawn.
     * Negative coordinates are allowed, as long as the clipped view has
     * positive or zero coordinates
     * \param a Upper left corner of clipping rectangle
     * \param b Lower right corner of clipping rectangle
     * \param text text to write
     */
    void clippedWrite(Point p, Point a,  Point b, const char *text) override;

    /**
     * Clear the Display. The screen will be filled with the desired color
     * \param color fill color
     */
    void clear(Color color) override;

    /**
     * Clear an area of the screen
     * \param p1 upper left corner of area to clear
     * \param p2 lower right corner of area to clear
     * \param color fill color
     */
    void clear(Point p1, Point p2, Color color) override;

    /**
     * This member function is used on some target displays to reset the
     * drawing window to its default value. You have to call beginPixel() once
     * before calling setPixel(). You can then make any number of calls to
     * setPixel() without calling beginPixel() again, as long as you don't
     * call any other member function in this class. If you call another
     * member function, for example line(), you have to call beginPixel() again
     * before calling setPixel().
     *
     * MAYBE USELESS IN ST7735
     * This backend does not require it, so it is a blank.
     */
    void beginPixel() override;

    /**
     * Draw a pixel with desired color. You have to call beginPixel() once
     * before calling setPixel()
     * \param p point where to draw pixel
     * \param color pixel color
     */
    void setPixel(Point p, Color color) override;

    /**
     * Draw a line between point a and point b, with color c
     * \param a first point
     * \param b second point
     * \param c line color
     */
    void line(Point a, Point b, Color color) override;

    /**
     * Draw an horizontal line on screen.
     * Instead of line(), this member function takes an array of colors to be
     * able to individually set pixel colors of a line.
     * \param p starting point of the line
     * \param colors an array of pixel colors whoase size must be b.x()-a.x()+1
     * \param length length of colors array.
     * p.x()+length must be <= display.width()
     */
    void scanLine(Point p, const Color *colors, unsigned short length) override;

    /**
     * \return a buffer of length equal to this->getWidth() that can be used to
     * render a scanline.
     */
    Color *getScanLineBuffer() override;

    /**
     * Draw the content of the last getScanLineBuffer() on an horizontal line
     * on the screen.
     * \param p starting point of the line
     * \param length length of colors array.
     * p.x()+length must be <= display.width()
     */
    void scanLineBuffer(Point p, unsigned short length) override;

    /**
     * Draw an image on the screen
     * \param p point of the upper left corner where the image will be drawn
     * \param i image to draw
     */
    void drawImage(Point p, const ImageBase& img) override;

    /**
     * Draw part of an image on the screen
     * \param p point of the upper left corner where the image will be drawn.
     * Negative coordinates are allowed, as long as the clipped view has
     * positive or zero coordinates
     * \param a Upper left corner of clipping rectangle
     * \param b Lower right corner of clipping rectangle
     * \param img Image to draw
     */
    void clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) override;

     /**
     * Draw a rectangle (not filled) with the desired color
     * \param a upper left corner of the rectangle
     * \param b lower right corner of the rectangle
     * \param c color of the line
     */
    void drawRectangle(Point a, Point b, Color c) override;

     /**
     * Make all changes done to the display since the last call to update()
     * visible.
     * TODO: understand if it can be useful or not.
     */
    void update() override;

    /**
     * Pixel iterator. A pixel iterator is an output iterator that allows to
     * define a window on the display and write to its pixels.
     */
    class pixel_iterator
    {
    public:
        /**
         * Default constructor, results in an invalid iterator.
         */
        pixel_iterator(): pixelLeft(0) {}

        /**
         * Set a pixel and move the pointer to the next one
         * \param color color to set the current pixel
         * \return a reference to this
         */
        pixel_iterator& operator= (Color color)
        {
            writeRam(color);
            if(--pixelLeft == 0) invalidate();
            return *this;
        }

        /**
         * Compare two pixel_iterators for equality.
         * They are equal if they point to the same location.
         */
        bool operator== (const pixel_iterator& itr)
        {
            return this->pixelLeft == itr.pixelLeft;
        }

        /**
         * Compare two pixel_iterators for inequality.
         * They different if they point to different locations.
         */
        bool operator!= (const pixel_iterator& itr)
        {
            return this->pixelLeft != itr.pixelLeft;
        }

        /**
         * \return a reference to this.
         */
        pixel_iterator& operator* () { return *this; }

        /**
         * \return a reference to this. Does not increment pixel pointer.
         */
        pixel_iterator& operator++ () { return *this; }

        /**
         * \return a reference to this. Does not increment pixel pointer.
         */
        pixel_iterator& operator++ (int) { return *this; }

        /**
         * Must be called if not all pixels of the required window are going
         * to be written.
         */
        void invalidate() {
            csx::high();
            writeRamEnd();
        }

    private:
        /**
         * Constructor
         * \param pixelLeft number of remaining pixels
         */
        pixel_iterator(unsigned int pixelLeft): pixelLeft(pixelLeft) {}

        unsigned int pixelLeft; ///< How many pixels are left to draw

        friend class DisplayImpl; //Needs access to ctor
    };

    /**
     * Specify a window on screen and return an object that allows to write
     * its pixels.
     * Note: a call to begin() will invalidate any previous iterator.
     * \param p1 upper left corner of window
     * \param p2 lower right corner (included)
     * \param d increment direction
     * \return a pixel iterator
     */
    pixel_iterator begin(Point p1, Point p2, IteratorDirection d);

    /**
     * \return an iterator which is one past the last pixel in the pixel
     * specified by begin. Behaviour is undefined if called before calling
     * begin()
     */
    pixel_iterator end() const
    {
        // Default ctor: pixelLeft is zero
        return pixel_iterator();
    }

    /**
     * Destructor
     */
    ~DisplayImpl() override;

private:
    static const short int width = 128;
    static const short int height = 160;
    /**
     * Constructor.
     * Do not instantiate objects of this type directly from application code.
     */
    DisplayImpl();

    /**
     * Set cursor to desired location
     * \param point where to set cursor (0<=x<132, 0<=y<162)
     */
    static inline void setCursor(Point p)
    {
        /*
         * Maybe there is a more efficient way
         */
        window(p,p);
    }

    /**
    *   register 0x36: bit 7------0
    *       |||||+--  MH horizontal referesh (0 L-to-R, 1 R-to-L)
    *       ||||+---  RGB BRG order (0 for RGB)
    *       |||+----  ML vertical refesh (0 T-to-D, 1 D-to-T)
    *       ||+-----  MV row column exchange
    *       |+------  MX column address order
    *       +-------  MY row address order
    */
    /**
     * Set a hardware window on the screen, optimized for writing text.
     * The GRAM increment will be set to up-to-down first, then left-to-right
     * which is the correct increment to draw fonts
     * \param p1 upper left corner of the window
     * \param p2 lower right corner of the window
     */
    static inline void textWindow(Point p1, Point p2) {
        // reg 0x36 is MADCTL (memory data access control)
        // value 0x20 is YXV=001 ML=0 RGB=0 MH=0  --> 00100000
        writeReg (0x36, 0x20);
        window(p1, p2);
    }

    /**
     * Set a hardware window on the screen, optimized for drawing images.
     * The GRAM increment will be set to left-to-right first, then up-to-down
     * which is the correct increment to draw images
     * \param p1 upper left corner of the window
     * \param p2 lower right corner of the window
     */
    static inline void imageWindow(Point p1, Point p2){
        // reg 0x36 is MADCTL (memory data access control)
        // value 0x0 is YXV=000 ML=0 RGB=0 MH=0  --> 00000000
        writeReg (0x36, 0x00, 0x01);
        window(p1, p2);
    }

    /**
     * Common part of all window commands
     */
    static void window(Point p1, Point p2);

    /**
     * Sends command 0xc which seems to be the one to start sending pixels.
     * Also, change SPI interface to 16 bit mode
     */
    static void writeRamBegin()
    {
        CommandTransaction c;
        writeRam(0x2C);     //ST7735_RAMWR
        //Change SPI interface to 16 bit mode, for faster pixel transfer
        SPI1->CR1 = 0;
        SPI1->CR1 = SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_DFF //TODO: this could be useless
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;
    }

    /**
     * Used to send pixel data to the display's RAM, and also to send commands.
     * The SPI chip select must be low before calling this member function
     * \param data data to write
     */
    static unsigned short writeRam(unsigned short data)
    {
        SPI1->DR = data;
        while((SPI1->SR & SPI_SR_RXNE)==0) ;
        return SPI1->DR; //Note: reading back SPI1->DR is necessary.
    }

    /**
     * Ends a pixel transfer to the display
     */
    static void writeRamEnd()
    {
        //Put SPI back into 8 bit mode
        SPI1->CR1 = 0;
        SPI1->CR1 = SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;
    }

    /**
     * Write data to a display register
     * \param reg which register?
     * \param data data to write
     */
    static void writeReg(unsigned char reg, unsigned char data);

    /**
     * Write data to a display register
     * \param reg which register?
     * \param data data to write, if null only reg will be written (zero arg cmd)
     * \param len length of data, number of argument bytes
     */
    static void writeReg(unsigned char reg, const unsigned char *data=0, int len=1);

    /**
     * Start a DMA transfer to the display
     * \param data pointer to the data to be written
     * \param length number of 16bit transfers to make to the display
     * \param increm if true, the DMA will increment the pointer between
     * successive transfers, otherwise the first 16bits pointed to will be
     * repeatedly read length times
     */
    void startDmaTransfer(const unsigned short *data, int length, bool increm);

    /**
     * Check if a pending DMA transfer is in progress, and wait for
     * its completion
     */
    void waitDmaCompletion();

    Color pixel; ///< Buffer of one pixel, for overlapped I/O
    Color buffers[2][128]; ///< Line buffers for scanline overlapped I/O
    int which; ///< Currently empty buffer
};

} //namespace mxgui

#endif //_BOARD_STM32F407VG_STM32F4DISCOVERY

#endif //DISPLAY_ST7735H
