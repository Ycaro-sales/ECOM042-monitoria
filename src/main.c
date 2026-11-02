/*******************************************************************
 * @file main.c
 *
 * @brief Main file.
 * @author João Matheus Nascimento Dias (jmnd@ic.ufal.br)
 * @author José Félix de Oliveira Neto (jfon@ic.ufal.br)
 * @version 0.1
 * @date 26/08/2026
 *******************************************************************/

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int get_button_state()
{
	return gpio_pin_get_dt(&button);
}

int get_led_state()
{
	return gpio_emul_output_get_dt(&led0);
}

int set_led_state(bool state)
{
	return gpio_pin_set_dt(&led0, state > 0);
}

int handle_button()
{
	int button_state = get_button_state();

	if (button_state < 0) {
		return button_state;
	}

	int ret = set_led_state(button_state);

	if (ret < 0) {
		return ret;
	}

	int led_state = get_led_state();
	if (led_state < 0) {
		return led_state;
	}

	printk("Button: %d -> LED: %d\n", button_state, led_state);

	return 0;
}

int init_board()
{
	int ret;
	if (!gpio_is_ready_dt(&button) || !gpio_is_ready_dt(&led0)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int main()
{
	int sequence[] = {0, 1, 0, 1};
	int ret = 0;
	ret = init_board();
	if (ret < 0) {
		goto end;
	}
	for (int i = 0; i < ARRAY_SIZE(sequence); i++) {
		ret = gpio_emul_input_set_dt(&button, sequence[i]);
		if (ret < 0) {
			goto end;
		}

		ret = handle_button();

		if (ret < 0) {
			goto end;
		}

		k_msleep(100);
	}
end:

	return ret;
}
