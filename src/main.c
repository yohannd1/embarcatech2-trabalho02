#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "ssd1306.h"
#include "ws2812b_matrix.h"

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define LED_ON_INTENSITY 0.02f
#define LED_STRIP_PIN 7
#define ADC_INPUT_PIN 28
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

const float R_CONHECIDO = 10000.0f; // resistência conhecida (ohm)
const float ADC_VREF = 3.3f; // tensão de referência do ADC
const uint32_t ADC_RESOLUTION = 4095;

static ws2812b_matrix_t matrix;
static ws2812b_buffer_t buffer;

/**
 * Calcula a resistência para resistores de 4 bandas, com tolerância máxima de
 * 5% para o intervalo especificado.
 *
 * @param resist o valor da resistência (510.0f <= resistance <= 100000.0f)
 * @param digit1 (vai receber) a cor do primeiro dígito (0 <= *digit1 <= 9)
 * @param digit2 (vai receber) a cor do segundo dígito (0 <= *digit2 <= 9)
 * @param mult (vai receber) a cor do multiplicador (0 <= *mult <= 9)
 *
 * Retorna um valor indicando se a função rodou com sucesso.
 */
static bool calc_res_colors_4band(float resist, uint8_t *digit1, uint8_t *digit2, uint8_t *mult);

static void draw_resistor_repr(uint8_t digit1, uint8_t digit2, uint8_t mult);
static ws2812b_color_t get_color_for(uint8_t idx);
static void die(const char *msg);
static void on_button_press(uint gpio, uint32_t events);

int main(void) {
	stdio_init_all();

	if (!ws2812b_matrix_init(&matrix, pio0, LED_STRIP_PIN))
		die("falha ao inicializar a matriz de LEDs");

	for (int i = 0; i < 25; i++)
		buffer[i] = (ws2812b_color_t) { 0.0f, 0.0f, 0.0f };
	ws2812b_matrix_draw(&matrix, &buffer);

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

	ssd1306_fill(&display, false);
	ssd1306_send_data(&display);

	adc_init();
	adc_gpio_init(ADC_INPUT_PIN);

	char str_x[5], str_y[5];

	const size_t PASS_COUNT = 500;

	while (true) {
		adc_select_input(2); // referente ao GPIO 28

		printf("Vamos calcular...\n");

		float soma = 0.0f;
		for (int i = 0; i < PASS_COUNT; i++) {
			soma += adc_read();
			sleep_ms(1);
		}
		float adc_encontrado = soma / PASS_COUNT;

		printf("Encontrado adc = %.f\n", adc_encontrado);

		// Fórmula simplificada: r_x = R_CONHECIDO * ADC_encontrado / (ADC_RESOLUTION - adc_encontrado)
		float r_x = (R_CONHECIDO * adc_encontrado) / (ADC_RESOLUTION - adc_encontrado);

		uint8_t d1, d2, m;
		if (calc_res_colors_4band(r_x, &d1, &d2, &m)) {
			printf("Código de 4 bandas calculado (para %.f ohms): { %u %u %u } (tol. 5%)\n", r_x, d1, d2, m);
			draw_resistor_repr(d1, d2, m);
		} else {
			printf("Falha ao calcular o valor da resistência\n");
		}

		snprintf(str_x, sizeof(str_x) / sizeof(str_x[0]), "%.0f", adc_encontrado);
		snprintf(str_y, sizeof(str_y) / sizeof(str_y[0]), "%.0f", r_x);

		ssd1306_fill(&display, 1);
		ssd1306_rect(&display, 3, 3, 122, 60, 1, 0);
		ssd1306_line(&display, 3, 25, 123, 25, 1);
		ssd1306_line(&display, 3, 37, 123, 37, 1);

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

		ssd1306_line(&display, 44, 37, 44, 60, 1);

		x = 8, y = 52;
		ssd1306_draw_string(&display, str_x, &x, &y);

		x = 59, y = 52;
		ssd1306_draw_string(&display, str_y, &x, &y);

		ssd1306_send_data(&display);

		sleep_ms(700);
	}

	return 0;
}

static void on_button_press(uint gpio, uint32_t events) {
	if (gpio == BUTTON_B_PIN) {
		printf("Entrando no modo bootsel...\n");
		reset_usb_boot(0, 0);
	}
}

static bool calc_res_colors_4band(float resist, uint8_t *digit1, uint8_t *digit2, uint8_t *mult) {
	// retornar falso se o valor estiver fora do intervalo especificado
	if (resist < 510.0f || resist > 100000.0f)
		return false;

	float val = resist;
	uint8_t m = 0;

	while (val >= 100.0f) {
		// dividir por 10 e aumentar o multiplicador
		val /= 10.0f;
		m++;
	}

	*digit1 = (int)val / 10;
	*digit2 = (int)val % 10;
	*mult = m;

	return true;
}

static void draw_resistor_repr(uint8_t digit1, uint8_t digit2, uint8_t mult) {
	buffer[5*2 + 1] = get_color_for(digit1);
	buffer[5*2 + 2] = get_color_for(digit2);
	buffer[5*2 + 3] = get_color_for(mult);
	ws2812b_matrix_draw(&matrix, &buffer);
}

static ws2812b_color_t get_color_for(uint8_t idx) {
	if (idx > 9) idx = 9;

	const ws2812b_color_t colors[10] = {
		{ 0.0f, 0.0f,  0.0f }, // preto
		{ 0.3f, 0.2f,  0.0f }, // marrom (não muito bom)
		{ 1.0f, 0.0f,  0.0f }, // vermelho
		{ 1.0f, 0.5f,  0.0f }, // laranja
		{ 1.0f, 1.0f,  0.0f }, // amarelo
		{ 0.0f, 1.0f,  0.0f }, // verde
		{ 0.0f, 0.0f,  1.0f }, // azul
		{ 0.5f, 0.15f, 1.0f }, // violeta
		{ 0.5f, 0.5f,  0.5f }, // cinza
		{ 1.0f, 1.0f,  1.0f }, // branco
	};

	ws2812b_color_t ret = colors[idx];
	ret.r *= LED_ON_INTENSITY;
	ret.g *= LED_ON_INTENSITY;
	ret.b *= LED_ON_INTENSITY;
	return ret;
}

static void die(const char *msg) {
	while (true) {
		printf("ERRO FATAL: %s\n", msg);
		sleep_ms(2000);
	}
}
