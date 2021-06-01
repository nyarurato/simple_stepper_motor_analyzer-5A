

#include "tft_driver.h"

#include "gpio_tables.h"
#include "hardware/gpio.h"
#include "io.h"
#include "pico/stdlib.h"

// // Assuming landscape mode per memory access command 0x36.
#define WIDTH 480
#define HEIGHT 320

#define ILI9488_SLPOUT 0x11

#define ILI9488_DISPON 0x29

#define ILI9488_CASET 0x2A
#define ILI9488_PASET 0x2B
#define ILI9488_RAMWR 0x2C
#define ILI9488_RAMRD 0x2E

namespace tft_driver {

// Send a byte to the TFT. Since the interface to the TFT is 
// 16 bits parallel, it writes a 16 bits value where the upper
// 8 bits  are all zero.
//
// Note that actual pixels are written by separate code in this file
// which sends them as 16 bits color values.
static inline void write_byte(uint8_t c) {
  sio_hw->gpio_set = gpio_tables::gpio_direct_set_table[c];
  // NOTE: The gpio_direct_clr_table is configured to also do the
  // equivalent of TFT_WR_LOW.
  sio_hw->gpio_clr = gpio_tables::gpio_direct_clr_table[c];
  //  __asm volatile ("nop\n");
  //  __asm volatile ("nop\n");
  TFT_WR_HIGH;
}

void write_command_byte(uint8_t c) {
  TFT_DC_LOW;  // Indicates a command
  write_byte(c);
}

void write_data_byte(uint8_t c) {
  TFT_DC_HIGH;  // Indicates data
  write_byte(c);
}

void begin() {
  // NOTE: To avoid flicker, TFT_BL is set to an inactive state earlier,
  // when initializing the pins and is enabled later after rendering the 
  // first screen.

  TFT_WR_HIGH;
  TFT_RST_HIGH;

  sleep_ms(5);
  TFT_RST_LOW;
  sleep_ms(20);
  TFT_RST_HIGH;
  sleep_ms(150);

  write_command_byte(0xE0);
  write_data_byte(0x00);
  write_data_byte(0x03);
  write_data_byte(0x09);
  write_data_byte(0x08);
  write_data_byte(0x16);
  write_data_byte(0x0A);
  write_data_byte(0x3F);
  write_data_byte(0x78);
  write_data_byte(0x4C);
  write_data_byte(0x09);
  write_data_byte(0x0A);
  write_data_byte(0x08);
  write_data_byte(0x16);
  write_data_byte(0x1A);
  write_data_byte(0x0F);

  write_command_byte(0XE1);
  write_data_byte(0x00);
  write_data_byte(0x16);
  write_data_byte(0x19);
  write_data_byte(0x03);
  write_data_byte(0x0F);
  write_data_byte(0x05);
  write_data_byte(0x32);
  write_data_byte(0x45);
  write_data_byte(0x46);
  write_data_byte(0x04);
  write_data_byte(0x0E);
  write_data_byte(0x0D);
  write_data_byte(0x35);
  write_data_byte(0x37);
  write_data_byte(0x0F);

  write_command_byte(0XC0);  // Power Control 1
  write_data_byte(0x17);     // Vreg1out
  write_data_byte(0x15);     // Verg2out

  write_command_byte(0xC1);  // Power Control 2
  write_data_byte(0x41);     // VGH,VGL

  write_command_byte(0xC5);  // Power Control 3
  write_data_byte(0x00);
  write_data_byte(0x12);  // Vcom
  write_data_byte(0x80);

  write_command_byte(0x36);  // Memory Access
  write_data_byte(0xe8);     // landscape mode. Swapping and mirroring x, y.

  // NOTE: For 16bit parallel transfer, IM jumpers need to be set
  // as 010.
  write_command_byte(0x3A);  // Interface Pixel Format
  write_data_byte(0x55);     // 16 bit

  write_command_byte(0XB0);  // Interface Mode Control
  write_data_byte(0x80);     // SDO NOT USE

  write_command_byte(0xB1);  // Frame rate
  write_data_byte(0xA0);     // 60Hz

  write_command_byte(0xB4);  // Display Inversion Control
  write_data_byte(0x02);     // 2-dot

  write_command_byte(0XB6);  // Display Function Control  RGB/MCU Interface Control

  write_data_byte(0x02);  // MCU
  write_data_byte(0x02);  // Source,Gate scan direction

  write_command_byte(0XE9);  // Set Image Function
  write_data_byte(0x00);     // Disable 24 bit data

  write_command_byte(0xF7);  // Adjust Control
  write_data_byte(0xA9);
  write_data_byte(0x51);
  write_data_byte(0x2C);
  write_data_byte(0x82);  // D7 stream, loose

  write_command_byte(ILI9488_SLPOUT);  // Exit Sleep
  sleep_ms(120);
  write_command_byte(ILI9488_DISPON);  // Display on
}

// This is followed by a stream of pixels to render in this
// rectangle.
void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  write_command_byte(ILI9488_CASET);  // Column addr set
  write_data_byte(x0 >> 8);
  write_data_byte(x0 & 0xFF);  // XSTART
  write_data_byte(x1 >> 8);
  write_data_byte(x1 & 0xFF);  // XEND

  write_command_byte(ILI9488_PASET);  // Row addr set
  write_data_byte(y0 >> 8);
  write_data_byte(y0 & 0xff);  // YSTART
  write_data_byte(y1 >> 8);
  write_data_byte(y1 & 0xff);  // YEND

  // Should follow by data bytes.
  write_command_byte(ILI9488_RAMWR);  // write to RAM

  // Next send the pixels data.
  TFT_DC_HIGH;
}

// NOTE: The gpio_color_clr_table is configured to also do the
// equivalent of TFT_WR_LOW. This macro was optimized for pixel 
// transfer speed while still maintaining data setup and hold
// time relative to the positive transition of the WR signal.
#define SEND_PIXEL(i)                                          \
  color = p[i];                                                \
  TFT_WR_HIGH;                                                 \
  sio_hw->gpio_set = gpio_tables::gpio_color_set_table[color]; \
  sio_hw->gpio_clr = gpio_tables::gpio_color_clr_table[color];

void render_buffer(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                   const uint8_t* color8_p) {
  setAddrWindow(x1, y1, x2, y2);

  const int32_t w_pixels = x2 - x1 + 1;
  const int32_t h_pixels = y2 - y1 + 1;
  const uint32_t pixels_count = w_pixels * h_pixels;

  const uint8_t* p = color8_p;
  const uint8_t* const p_limit = p + pixels_count;
  const uint8_t* const p_limit_minus_20 = p_limit - 20;

  // Fast loop with 8 pixels per iteration.
  uint8_t color = 0;
  while (p < p_limit_minus_20) {
    SEND_PIXEL(0);
    SEND_PIXEL(1);
    SEND_PIXEL(2);
    SEND_PIXEL(3);
    SEND_PIXEL(4);
    SEND_PIXEL(5);
    SEND_PIXEL(6);
    SEND_PIXEL(7);
    SEND_PIXEL(8);
    SEND_PIXEL(9);
    SEND_PIXEL(10);
    SEND_PIXEL(11);
    SEND_PIXEL(12);
    SEND_PIXEL(13);
    SEND_PIXEL(14);
    SEND_PIXEL(15);
    SEND_PIXEL(16);
    SEND_PIXEL(17);
    SEND_PIXEL(18);
    SEND_PIXEL(19);
    p += 20;
  }

  // Remaining pixels.
  while (p < p_limit) {
    SEND_PIXEL(0);
    p++;
  }

  // Make sure last pixel has sufficient hold time.
  // __asm volatile("nop\n");
  // __asm volatile("nop\n");
  TFT_WR_HIGH;
}

}  // namespace tft_driver
