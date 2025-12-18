/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "zap-vga16.h"
#define PSF_HEADER_SIZE 4
#define PSF_GLYPH_SIZE 16

/*
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 in LSB format otherwise 0x01
#define SHARPMEM_BIT_VCOM (0x02)     // Sent now using SPI_DEVICE_TXBIT_LSBFIRST
#define SHARPMEM_BIT_CLEAR (0x04)

#define ESP_HOST    SPI2_HOST // SPI2
#define PIN_NUM_MOSI 35
#define PIN_NUM_CLK  36
#define PIN_NUM_CS   38
#define PIN_NUM_VCOM   33
#define PIN_BLUE_LED   13

#define PXWIDTH 320
#define PXHEIGHT 240

void displayInit(void);
void display_write_data(uint8_t addr, uint8_t data);
#define SPI_TAG "spi_protocol"

typedef enum {
    INSERT_MODE,    
    REPLACE_MODE,
    NORMAL_MODE
} cursor_mode;

spi_device_handle_t spi;
uint8_t *sharpmem_buffer = NULL;

void vcom_toggle_task(void *pvParameters) 
{
    gpio_set_direction(PIN_NUM_VCOM, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BLUE_LED, GPIO_MODE_OUTPUT);
    while (1) {
        // Toggle the GPIO state
        gpio_set_level((gpio_num_t)PIN_NUM_VCOM, 0);
        gpio_set_level((gpio_num_t)PIN_BLUE_LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS); // Delay for 1 second
        gpio_set_level((gpio_num_t)PIN_NUM_VCOM, 1);
        gpio_set_level((gpio_num_t)PIN_BLUE_LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS); // Delay for 1 second
    }
}


void displayInit(void)
{
    esp_err_t ret;
    /* Allocate pixel buffer for SHARP DISP*/
    sharpmem_buffer = (uint8_t *)malloc((PXWIDTH * PXHEIGHT) / 8);
    if (!sharpmem_buffer) {
      printf("Error: sharpmem_buffer was NOT allocated\n\n");
      return;
    }

    gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);                   // Setting the CS' pin to work in OUTPUT mode

    spi_bus_config_t buscfg = {                                         // Provide details to the SPI_bus_sturcture of pins and maximum data size
        .miso_io_num = -1,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 512 * 8                                       // 4095 bytes is the max size of data that can be sent because of hardware limitations
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,                             // Clock out at 12 MHz
        .mode = 0,                                                      // SPI mode 0: CPOL:-0 and CPHA:-0
        .spics_io_num = -1,                                     // Control the CS ourselves
        .flags = (SPI_DEVICE_TXBIT_LSBFIRST | SPI_DEVICE_3WIRE),
        .queue_size = 7,                                                // We want to be able to queue 7 transactions at a time
    };

    ret = spi_bus_initialize(ESP_HOST, &buscfg, SPI_DMA_CH_AUTO);       // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(ESP_HOST, &devcfg, &spi);                  // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);
    printf("SPI initialized. MOSI:%d CLK:%d CS:%d\n", PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    gpio_set_level((gpio_num_t)PIN_NUM_CS, 0);
}


// 1<<n is a costly operation on AVR -- table usu. smaller & faster
static const uint8_t  set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                      clr[] = {(uint8_t)~1,  (uint8_t)~2,  (uint8_t)~4,
                              (uint8_t)~8,  (uint8_t)~16, (uint8_t)~32,
                              (uint8_t)~64, (uint8_t)~128};


void setPixel(int16_t x, int16_t y, uint16_t color) {
  if (color) {
    sharpmem_buffer[(y * PXWIDTH + x) / 8] |= set[x & 7]; // set[x & 7]
  } else {
    sharpmem_buffer[(y * PXWIDTH + x) / 8] &= clr[x & 7]; // clr[x & 7]
  }
}

uint8_t getPixel(uint16_t x, uint16_t y) {
  if ((x >= PXWIDTH) || (y >= PXHEIGHT))
    return 0; // <0 test not needed, unsigned
  return sharpmem_buffer[(y * PXWIDTH + x) / 8] & set[x & 7] ? 1 : 0;
}

void setChar(uint8_t index, uint8_t row, uint8_t col) {
 for (int m =0; m < PSF_GLYPH_SIZE; m++) {
    sharpmem_buffer[((row * PSF_GLYPH_SIZE +m)*PXWIDTH + 8 * col) / 8] = zap_vga16_psf[ PSF_HEADER_SIZE + index * PSF_GLYPH_SIZE +m];
 }
}

void setCursor(cursor_mode mode, uint8_t row, uint8_t col) {
 for (int m =0; m < PSF_GLYPH_SIZE; m++) {
    uint8_t cursor = sharpmem_buffer[((row * PSF_GLYPH_SIZE +m)*PXWIDTH + 8 * col) / 8]; 
    if (mode == INSERT_MODE) cursor |= ~zap_vga16_psf[ PSF_HEADER_SIZE + 0xCF * PSF_GLYPH_SIZE +m];
    else cursor = ~cursor; 
    sharpmem_buffer[((row * PSF_GLYPH_SIZE +m)*PXWIDTH + 8 * col) / 8] = cursor;
 }
}

void clearDisplay() {
  memset(sharpmem_buffer, 0xff, (PXWIDTH * PXHEIGHT) / 8);
  gpio_set_level((gpio_num_t)PIN_NUM_CS, 1);
  esp_rom_delay_us(6);
  uint8_t clear_data[2] = {(uint8_t)(SHARPMEM_BIT_CLEAR), 0x00};
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));        //Zero out the transaction
  t.length = sizeof(clear_data)*8; //Each data byte is 8 bits Einstein
  t.tx_buffer = clear_data;
  ret = spi_device_polling_transmit(spi, &t); // spi_device_polling_transmit
  gpio_set_level((gpio_num_t)PIN_NUM_CS, 0);
  esp_rom_delay_us(2);
  //printf("clearDisplay b1:%02x 2:%02x lenght:%d\n\n", clear_data[0], clear_data[1], t.length);
  assert(ret==ESP_OK);
}

void refreshDisplay(void) {
  uint16_t i, currentline;

  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       //Zero out the transaction

  gpio_set_level((gpio_num_t)PIN_NUM_CS, 1);
  esp_rom_delay_us(6);
  uint8_t write_data[1] = {(uint8_t)SHARPMEM_BIT_WRITECMD};
  t.length = sizeof(write_data)*8;                  //Each data byte is 8 bits
  t.tx_buffer = write_data;
  ret = spi_device_transmit(spi, &t);

  uint8_t bytes_per_line = PXWIDTH / 8;
  uint16_t totalbytes = (PXWIDTH * PXHEIGHT) / 8;

  for (i = 0; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address byte
    currentline = ((i + 1) / (PXWIDTH / 8)) + 1;
    line[0] = currentline;
    // copy over this line
    memcpy(line + 1, sharpmem_buffer + i, bytes_per_line);
    // Send end of line
    line[bytes_per_line + 1] = 0x00;

    t.length = (bytes_per_line+2) *8; // bytes_per_line+2
    t.tx_buffer = line;
    ret = spi_device_transmit(spi, &t);
    assert(ret==ESP_OK);
  }
  // Send another trailing 8 bits for the last line
  int last_line[1] = {0x00};
  t.length = 8;

  t.tx_buffer = last_line;
  ret = spi_device_transmit(spi, &t); // spi_device_polling_transmit
  gpio_set_level((gpio_num_t)PIN_NUM_CS, 0);
  esp_rom_delay_us(2);

  assert(ret==ESP_OK);
  }

void updateLines(uint8_t from, uint8_t to) {
  uint16_t i, currentline;

  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       //Zero out the transaction

  gpio_set_level((gpio_num_t)PIN_NUM_CS, 1);
  esp_rom_delay_us(6);
  uint8_t write_data[1] = {(uint8_t)SHARPMEM_BIT_WRITECMD};
  t.length = sizeof(write_data)*8;                  //Each data byte is 8 bits
  t.tx_buffer = write_data;
  ret = spi_device_transmit(spi, &t);

  uint8_t bytes_per_line = PXWIDTH / 8;
  uint16_t totalbytes = (PXWIDTH * (to - from +1)) / 8;

  for (i = from; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address byte
    currentline = ((i + 1) / (PXWIDTH / 8)) + 1;
    line[0] = currentline;
    // copy over this line
    memcpy(line + 1, sharpmem_buffer + i, bytes_per_line);
    // Send end of line
    line[bytes_per_line + 1] = 0x00;

    t.length = (bytes_per_line+2) *8; // bytes_per_line+2
    t.tx_buffer = line;
    ret = spi_device_transmit(spi, &t);
    assert(ret==ESP_OK);
  }
  // Send another trailing 8 bits for the last line
  int last_line[1] = {0x00};
  t.length = 8;

  t.tx_buffer = last_line;
  ret = spi_device_transmit(spi, &t); // spi_device_polling_transmit
  gpio_set_level((gpio_num_t)PIN_NUM_CS, 0);
  esp_rom_delay_us(2);

  assert(ret==ESP_OK);
  }


void clearDisplayBuffer() {
  memset(sharpmem_buffer, 0xFF, (PXWIDTH * PXHEIGHT) / 8);
}

void app_main(void)
{
    // When writing characters from a font, dont need to invert the bits every time. The font can be directly encoded that way.
    for (int m=0; m < (256 * PSF_GLYPH_SIZE) ; m++) {
        uint8_t c = 0;
        for (uint8_t x=0; x < 8; ++x) {
            c = (c << 1) | (zap_vga16_psf[ PSF_HEADER_SIZE + m] & 1);
            zap_vga16_psf[ PSF_HEADER_SIZE + m] >>= 1;
        }
        zap_vga16_psf[ PSF_HEADER_SIZE + m] = ~c;
    }

    xTaskCreate(&vcom_toggle_task, "vcom", 2048, NULL, 5, NULL);
    displayInit();
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    clearDisplay();
    for (int m=0; m<80; m++) setPixel(m, 10, 1);
    for (int m=20; m<100; m++) setPixel(m, 20, 0);
    for (int m=20; m<100; m++) setPixel(100, m, 0);

    char toprint[12] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21};
    char strlength = sizeof(toprint);
    char curx = 0;
    char cury = 0;
    for (int m=0; m < strlength ; m++){
        setChar(toprint[m], cury, curx);
	updateLines(PSF_GLYPH_SIZE*cury, PSF_GLYPH_SIZE*(cury+1)-1);
	curx++;
    }
    
    curx = 39 - strlength +1;
    cury = 14;
    for (int m=0; m < strlength ; m++){
        setChar(toprint[m], cury, curx);
	updateLines(PSF_GLYPH_SIZE*cury, PSF_GLYPH_SIZE*(cury+1)-1);
	curx++;
    }
    setCursor(1, 0,2);
    setCursor(0, 0,11);

    refreshDisplay();
}

