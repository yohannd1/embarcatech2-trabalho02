#include "ssd1306.h"
#include "ssd1306_font.h"

static void ssd1306_config(ssd1306_t *disp);

bool ssd1306_init(ssd1306_t *disp, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
	disp->width = width;
	disp->height = height;
	disp->pages = height / 8U;
	disp->address = address;
	disp->i2c_port = i2c;
	disp->bufsize = disp->pages * disp->width + 1;

	disp->ram_buffer = calloc(disp->bufsize, sizeof(uint8_t));
	if (disp->ram_buffer == NULL)
		return false;

	disp->ram_buffer[0] = 0x40;
	disp->port_buffer[0] = 0x80;

	ssd1306_config(disp);

	return true;
}

static void ssd1306_config(ssd1306_t *disp) {
	ssd1306_command(disp, SET_DISP | 0x00);
	ssd1306_command(disp, SET_MEM_ADDR);
	ssd1306_command(disp, 0x01);
	ssd1306_command(disp, SET_DISP_START_LINE | 0x00);
	ssd1306_command(disp, SET_SEG_REMAP | 0x01);
	ssd1306_command(disp, SET_MUX_RATIO);
	ssd1306_command(disp, disp->height - 1);
	ssd1306_command(disp, SET_COM_OUT_DIR | 0x08);
	ssd1306_command(disp, SET_DISP_OFFSET);
	ssd1306_command(disp, 0x00);
	ssd1306_command(disp, SET_COM_PIN_CFG);
	ssd1306_command(disp, 0x12);
	ssd1306_command(disp, SET_DISP_CLK_DIV);
	ssd1306_command(disp, 0x80);
	ssd1306_command(disp, SET_PRECHARGE);
	ssd1306_command(disp, 0xF1);
	ssd1306_command(disp, SET_VCOM_DESEL);
	ssd1306_command(disp, 0x30);
	ssd1306_command(disp, SET_CONTRAST);
	ssd1306_command(disp, 0xFF);
	ssd1306_command(disp, SET_ENTIRE_ON);
	ssd1306_command(disp, SET_NORM_INV);
	ssd1306_command(disp, SET_CHARGE_PUMP);
	ssd1306_command(disp, 0x14);
	ssd1306_command(disp, SET_DISP | 0x01);
}

void ssd1306_command(ssd1306_t *disp, uint8_t command) {
	disp->port_buffer[1] = command;
	i2c_write_blocking(disp->i2c_port, disp->address, disp->port_buffer, 2, false);
}

void ssd1306_send_data(ssd1306_t *disp) {
	ssd1306_command(disp, SET_COL_ADDR);
	ssd1306_command(disp, 0);
	ssd1306_command(disp, disp->width - 1);
	ssd1306_command(disp, SET_PAGE_ADDR);
	ssd1306_command(disp, 0);
	ssd1306_command(disp, disp->pages - 1);
	i2c_write_blocking(disp->i2c_port, disp->address, disp->ram_buffer, disp->bufsize, false);
}

void ssd1306_pixel(ssd1306_t *disp, uint8_t x, uint8_t y, bool value) {
	uint16_t index = (y >> 3) + (x << 3) + 1;
	uint8_t pixel = (y & 0b111);
	if (value)
		disp->ram_buffer[index] |= (1 << pixel);
	else
		disp->ram_buffer[index] &= ~(1 << pixel);
}

void ssd1306_fill(ssd1306_t *disp, bool value) {
	for (uint8_t y = 0; y < disp->height; y++) {
		for (uint8_t x = 0; x < disp->width; x++) {
			ssd1306_pixel(disp, x, y, value);
		}
	}
}

void ssd1306_rect(ssd1306_t *disp, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill) {
	for (uint8_t x = left; x < left + width; x++) {
		ssd1306_pixel(disp, x, top, value);
		ssd1306_pixel(disp, x, top + height - 1, value);
	}
	for (uint8_t y = top; y < top + height; y++) {
		ssd1306_pixel(disp, left, y, value);
		ssd1306_pixel(disp, left + width - 1, y, value);
	}

	if (fill) {
		for (uint8_t x = left + 1; x < left + width - 1; x++) {
			for (uint8_t y = top + 1; y < top + height - 1; y++) {
				ssd1306_pixel(disp, x, y, value);
			}
		}
	}
}

void ssd1306_line(ssd1306_t *disp, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value) {
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	while (true) {
		ssd1306_pixel(disp, x0, y0, value); // desenha o pixel atual

		if (x0 == x1 && y0 == y1) break; // termina quando alcança o ponto final

		int e2 = err * 2;

		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}

		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void ssd1306_hline(ssd1306_t *disp, uint8_t x0, uint8_t x1, uint8_t y, bool value) {
	for (uint8_t x = x0; x <= x1; x++)
		ssd1306_pixel(disp, x, y, value);
}

void ssd1306_vline(ssd1306_t *disp, uint8_t x, uint8_t y0, uint8_t y1, bool value) {
	for (uint8_t y = y0; y <= y1; y++)
		ssd1306_pixel(disp, x, y, value);
}

uint8_t ssd1306_draw_char(ssd1306_t *disp, char c, uint8_t x, uint8_t y) {
	uint16_t index = 0;
	if (c == ' ') return 3;
	else if (c == '.') index = 63 * 8;
	else if (c == '-') index = 64 * 8;
	else if (c == '#') index = 65 * 8;
	else if (c >= 'A' && c <= 'Z') index = (c - 'A' + 11) * 8;
	else if (c >= 'a' && c <= 'z') index = (c - 'a' + 37) * 8;
	else if (c >= '0' && c <= '9') index = (c - '0' + 1) * 8;

	uint8_t width = 0;
	for (uint8_t i = 0; i < 8; i++) {
		uint8_t column = font[index + i];
		if (column > 0) width = i;

		for (uint8_t j = 0; j < 8; j++)
			ssd1306_pixel(disp, x + i, y + j, column & (1 << j));
	}

	return width;
}

void ssd1306_draw_string(ssd1306_t *disp, const char *str, uint8_t x, uint8_t y) {
	ssd1306_draw_string_mobile(disp, str, &x, &y);
}

void ssd1306_draw_string_mobile(ssd1306_t *disp, const char *str, uint8_t *x_, uint8_t *y_) {
	uint8_t x = *x_;
	uint8_t y = *y_;

	for (int i = 0; str[i] != '\0' && y + 8 < disp->height; i++) {
		uint8_t width = ssd1306_draw_char(disp, str[i], x, y);

		x += width + 2; // put += 8 instead of this to get fixed width

		if (x + 8 >= disp->width) {
			x = 0;
			y += 8;
		}
	}

	*x_ = x;
	*y_ = y;
}
