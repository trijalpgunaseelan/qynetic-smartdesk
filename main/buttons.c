#include "navigation.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
static sd_navigation navigation;
void sd_buttons_init(void) {
    sd_nav_init(&navigation,0);
#if CONFIG_SMARTDESK_BUILTIN_BOOT
    gpio_config_t config={.pin_bit_mask=1ULL<<9,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_LOGI("buttons","Built-in BOOT GPIO9: tap next, hold previous; RESET restarts");
#else
    gpio_config_t config={.pin_bit_mask=(1ULL<<CONFIG_SMARTDESK_BUTTON_PREVIOUS)|(1ULL<<CONFIG_SMARTDESK_BUTTON_NEXT),.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_LOGI("buttons","Previous GPIO%d, Next GPIO%d; active low",CONFIG_SMARTDESK_BUTTON_PREVIOUS,CONFIG_SMARTDESK_BUTTON_NEXT);
#endif
}
void sd_buttons_poll(int64_t now) {
#if CONFIG_SMARTDESK_BUILTIN_BOOT
    sd_nav_poll_boot(&navigation,now,!gpio_get_level(GPIO_NUM_9));
#else
    sd_nav_poll(&navigation,now,!gpio_get_level(CONFIG_SMARTDESK_BUTTON_PREVIOUS),!gpio_get_level(CONFIG_SMARTDESK_BUTTON_NEXT));
#endif
}
const sd_navigation *sd_navigation_state(void) {return &navigation;}
