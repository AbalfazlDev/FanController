/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include "Framebuffer.h"
#include <stdlib.h>   // ???? itoa, ltoa
#include <string.h>
#include <stdio.h>

// ???? 5x7 ???? ????? 0-9 ? ???? A-Z (?? ???? ????? ???? ??? ????? ????)
// ???? ???? ????????? ???? ?? ????? ????
static const uint8_t font5x7[][5] PROGMEM = {
	{0x3E,0x51,0x49,0x45,0x3E}, // 0
	{0x00,0x42,0x7F,0x40,0x00}, // 1
	{0x42,0x61,0x51,0x49,0x46}, // 2
	{0x21,0x41,0x45,0x4B,0x31}, // 3
	{0x18,0x14,0x12,0x7F,0x10}, // 4
	{0x27,0x45,0x45,0x45,0x39}, // 5
	{0x3C,0x4A,0x49,0x49,0x30}, // 6
	{0x01,0x71,0x09,0x05,0x03}, // 7
	{0x36,0x49,0x49,0x49,0x36}, // 8
	{0x06,0x49,0x49,0x29,0x1E}, // 9
	{0x7C,0x12,0x11,0x12,0x7C}, // A (10)
	{0x7F,0x49,0x49,0x49,0x36}, // B (11)
	{0x3E,0x41,0x41,0x41,0x22}, // C (12)
	{0x7F,0x41,0x41,0x41,0x3E}, // D (13)
	{0x7F,0x49,0x49,0x49,0x41}, // E (14)
	{0x7F,0x09,0x09,0x09,0x01}, // F (15)
	{0x3E,0x41,0x49,0x49,0x7A}, // G (16)
	{0x7F,0x08,0x08,0x08,0x7F}, // H (17)
	{0x00,0x41,0x7F,0x41,0x00}, // I (18)
	{0x20,0x40,0x41,0x3F,0x01}, // J (19)
	{0x7F,0x08,0x14,0x22,0x41}, // K (20)
	{0x7F,0x40,0x40,0x40,0x40}, // L (21)
	{0x7F,0x02,0x0C,0x02,0x7F}, // M (22)
	{0x7F,0x04,0x08,0x10,0x7F}, // N (23)
	{0x3E,0x41,0x41,0x41,0x3E}, // O (24)
	{0x7F,0x09,0x09,0x09,0x06}, // P (25)
	{0x3E,0x41,0x51,0x21,0x5E}, // Q (26)
	{0x7F,0x09,0x19,0x29,0x46}, // R (27)
	{0x26,0x49,0x49,0x49,0x32}, // S (28)
	{0x01,0x01,0x7F,0x01,0x01}, // T (29)
	{0x3F,0x40,0x40,0x40,0x3F}, // U (30)
	{0x1F,0x20,0x40,0x20,0x1F}, // V (31)
	{0x3F,0x40,0x38,0x40,0x3F}, // W (32)
	{0x63,0x14,0x08,0x14,0x63}, // X (33)
	{0x07,0x08,0x70,0x08,0x07}, // Y (34)
	{0x61,0x51,0x49,0x45,0x43}  // Z (35)
};


Framebuffer::Framebuffer() {
	this->clear();
	this->cursorX = 0;
	this->cursorY = 0;
}

#ifndef SIMULATOR
void Framebuffer::drawBitmap(const uint8_t *progmem_bitmap, uint8_t height, uint8_t width, uint8_t pos_x, uint8_t pos_y) {
    uint8_t current_byte;
    uint8_t byte_width = (width + 7)/8;

    for (uint8_t current_y = 0; current_y < height; current_y++) {
        for (uint8_t current_x = 0; current_x < width; current_x++) {
            current_byte = pgm_read_byte(progmem_bitmap + current_y*byte_width + current_x/8);
            if (current_byte & (128 >> (current_x&7))) {
                this->drawPixel(current_x+pos_x,current_y+pos_y,1);
            } else {
                this->drawPixel(current_x+pos_x,current_y+pos_y,0);
            }
        }
    }
}

void Framebuffer::drawBuffer(const uint8_t *progmem_buffer) {
    uint8_t current_byte;

    for (uint8_t y_pos = 0; y_pos < 64; y_pos++) {
        for (uint8_t x_pos = 0; x_pos < 128; x_pos++) {
            current_byte = pgm_read_byte(progmem_buffer + y_pos*16 + x_pos/8);
            if (current_byte & (128 >> (x_pos&7))) {
                this->drawPixel(x_pos,y_pos,1);
            } else {
                this->drawPixel(x_pos,y_pos,0);
            }
        }
    }
}
#endif

void Framebuffer::drawPixel(uint8_t pos_x, uint8_t pos_y, uint8_t pixel_status) {
    if (pos_x >= SSD1306_WIDTH || pos_y >= SSD1306_HEIGHT) {
        return;
    }

    if (pixel_status) {
        this->buffer[pos_x+(pos_y/8)*SSD1306_WIDTH] |= (1 << (pos_y&7));
    } else {
        this->buffer[pos_x+(pos_y/8)*SSD1306_WIDTH] &= ~(1 << (pos_y&7));
    }
}

void Framebuffer::drawPixel(uint8_t pos_x, uint8_t pos_y) {
    if (pos_x >= SSD1306_WIDTH || pos_y >= SSD1306_HEIGHT) {
        return;
    }

    this->buffer[pos_x+(pos_y/8)*SSD1306_WIDTH] |= (1 << (pos_y&7));
}

void Framebuffer::drawVLine(uint8_t x, uint8_t y, uint8_t length) {
    for (uint8_t i = 0; i < length; ++i) {
        this->drawPixel(x,i+y);
    }
}

void Framebuffer::drawHLine(uint8_t x, uint8_t y, uint8_t length) {
    for (uint8_t i = 0; i < length; ++i) {
        this->drawPixel(i+x,y);
    }
}

void Framebuffer::drawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    uint8_t length = x2 - x1 + 1;
    uint8_t height = y2 - y1;

    this->drawHLine(x1,y1,length);
    this->drawHLine(x1,y2,length);
    this->drawVLine(x1,y1,height);
    this->drawVLine(x2,y1,height);
}

void Framebuffer::drawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t fill) {
    if (!fill) {
        this->drawRectangle(x1,y1,x2,y2);
    } else {
        uint8_t length = x2 - x1 + 1;
        uint8_t height = y2 - y1;

        for (int x = 0; x < length; ++x) {
            for (int y = 0; y <= height; ++y) {
                this->drawPixel(x1+x,y+y1);
            }
        }
    }
}

void Framebuffer::clear() {
    for (uint16_t buffer_location = 0; buffer_location < SSD1306_BUFFERSIZE; buffer_location++) {
        this->buffer[buffer_location] = 0x00;
    }
}

void Framebuffer::invert(uint8_t status) {
    this->oled.invert(status);
}

void Framebuffer::show() {
    this->oled.sendFramebuffer(this->buffer);
}

// ----------------------------------------------
// ??? ?? ??????? (??? ????? ? ???? ???? ???????)
// ----------------------------------------------
void Framebuffer::drawChar(uint8_t x, uint8_t y, char ch, uint8_t color) {
	uint8_t index;
	if (ch >= '0' && ch <= '9') {
		index = ch - '0';
		} else if (ch >= 'A' && ch <= 'Z') {
		index = 10 + (ch - 'A');
		} else if (ch >= 'a' && ch <= 'z') {
		index = 10 + (ch - 'a');  // ???? ???? ?? ????? ???? ?? ??? ????????
		} else {
		return; // ??????? ???????? ???????
	}
	
	for (uint8_t i = 0; i < 5; i++) {
		uint8_t line = pgm_read_byte(&font5x7[index][i]);
		for (uint8_t j = 0; j < 7; j++) {
			if (line & (1 << (6 - j))) {
				this->drawPixel(x + i, y + j, color);
			}
		}
	}
}

// ----------------------------------------------
// ??? ?? ????
// ----------------------------------------------
void Framebuffer::drawString(uint8_t x, uint8_t y, const char* str, uint8_t color) {
	uint8_t cx = x;
	while (*str) {
		drawChar(cx, y, *str, color);
		cx += 6; // ??? ??????? 5 ????? + 1 ?????
		str++;
	}
}

// ----------------------------------------------
// ??? ??? ???? (???? ?? ????) ?? ?????? ???? ????????
// ----------------------------------------------
void Framebuffer::drawNumber(int16_t num, uint8_t color) {
	char buffer[7];
	itoa(num, buffer, 10);
	drawString(this->cursorX, this->cursorY, buffer, color);
	// ??????????? ???????? (???????)
	this->cursorX += strlen(buffer) * 6;
}

// ----------------------------------------------
// ??? ??? ???? ?? ?????? ????
// ----------------------------------------------
void Framebuffer::drawNumber(int16_t num, uint8_t x, uint8_t y, uint8_t color) {
	char buffer[7];
	itoa(num, buffer, 10);
	drawString(x, y, buffer, color);
}

// ----------------------------------------------
// ??? ??? ???? ????? (?????? ?? 32767)
// ----------------------------------------------
void Framebuffer::drawUnsignedNumber(uint32_t num, uint8_t x, uint8_t y, uint8_t color) {
	char buffer[11];
	ultoa(num, buffer, 10);
	drawString(x, y, buffer, color);
}

// ----------------------------------------------
// ??? ??? ??????
// ----------------------------------------------
void Framebuffer::drawFloat(float num, uint8_t decimals, uint8_t x, uint8_t y, uint8_t color) {
	char buffer[20];
	// ????? float ?? ???? ?? ????? ????? ????
	int32_t intPart = (int32_t)num;
	uint32_t fracPart = (uint32_t)((num - intPart) * (decimals == 1 ? 10 : (decimals == 2 ? 100 : 1000)));
	if (fracPart < 0) fracPart = -fracPart;
	sprintf(buffer, "%ld.%0*u", intPart, decimals, fracPart);
	drawString(x, y, buffer, color);
}

// ----------------------------------------------
// ?????? ???????? (???????)
// ----------------------------------------------
void Framebuffer::setCursor(uint8_t x, uint8_t y) {
	this->cursorX = x;
	this->cursorY = y;
}

uint8_t Framebuffer::getCursorX() { return this->cursorX; }
uint8_t Framebuffer::getCursorY() { return this->cursorY; }