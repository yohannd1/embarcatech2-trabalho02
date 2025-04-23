#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "ssd1306.h"

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define ADC_INPUT_PIN 28
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

const float r_conhecido = 10000.0f; // resistência conhecida (ohm)
const float ADC_VREF = 3.31f; // tensão de referência do ADC
const uint32_t ADC_RESOLUTION = 4095;

static void on_button_press(uint gpio, uint32_t events);

int main(void) {
	stdio_init_all();

	gpio_init(BUTTON_A_PIN);
	gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_A_PIN);

	gpio_init(BUTTON_B_PIN);
	gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_B_PIN);
	gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &on_button_press);

	i2c_init(DISPLAY_I2C_PORT, 400000); // 400KHz

	gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(DISPLAY_SDA_PIN);
	gpio_pull_up(DISPLAY_SCL_PIN);

	ssd1306_t display;
	ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, DISPLAY_I2C_PORT);

	// Limpa o display. O display inicia com todos os pixels apagados.
	ssd1306_fill(&display, false);
	ssd1306_send_data(&display);

	adc_init();
	adc_gpio_init(ADC_INPUT_PIN); // GPIO 28 como entrada analógica

	float tensao;
	char str_x[5]; // Buffer para armazenar a string
	char str_y[5]; // Buffer para armazenar a string

	const int PASS_COUNT = 500;

	bool cor = true;
	while (true) {
		adc_select_input(2); // Seleciona o ADC para eixo X. O pino 28 como entrada analógica

		float soma = 0.0f;
		for (int i = 0; i < PASS_COUNT; i++) {
			/* soma += adc_read(); */
			soma += 800;
			sleep_ms(1);
		}
		float media = soma / PASS_COUNT;

		// Fórmula simplificada: r_x = r_conhecido * ADC_encontrado /(ADC_RESOLUTION - adc_encontrado)
		float r_x = (r_conhecido * media) / (ADC_RESOLUTION - media);

		snprintf(str_x, 5, "%.0f", media);
		snprintf(str_y, 5, "%.0f", r_x);

		// cor = !cor;
		//  Atualiza o conteúdo do display com animações

		ssd1306_fill(&display, !cor);
		ssd1306_rect(&display, 3, 3, 122, 60, cor, !cor);
		ssd1306_line(&display, 3, 25, 123, 25, cor);
		ssd1306_line(&display, 3, 37, 123, 37, cor);

		uint8_t x, y;

		x = 8, y = 6;
		ssd1306_draw_string(&display, "CEPEDI   TIC37", &x, &y);

		x = 20, y = 16;
		ssd1306_draw_string(&display, "EMBARCATECH", &x, &y);

		x = 10, y = 28;
		ssd1306_draw_string(&display, "  Ohmimetro", &x, &y);

		x = 13, y = 41;
		ssd1306_draw_string(&display, "ADC", &x, &y);

		x = 50, y = 41;
		ssd1306_draw_string(&display, "Resisten.", &x, &y);

		ssd1306_line(&display, 44, 37, 44, 60, cor);

		x = 8, y = 52;
		ssd1306_draw_string(&display, str_x, &x, &y);

		x = 59, y = 52;
		ssd1306_draw_string(&display, str_y, &x, &y);

		ssd1306_send_data(&display);

		sleep_ms(700);
	}
}

static void on_button_press(uint gpio, uint32_t events) {
	if (gpio == BUTTON_B_PIN) {
		reset_usb_boot(0, 0);
	}
}
