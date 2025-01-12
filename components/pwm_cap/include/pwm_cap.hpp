#include "driver/gpio.h"
#include "esp_timer.h"

#include <functional>

namespace pwm_cap {

struct pwm_item {
  int64_t rising;
  uint32_t ton;
  uint32_t toff;
};

esp_err_t init_for_all() { return gpio_install_isr_service(0); }

esp_err_t deinit_for_all() {
  gpio_uninstall_isr_service();
  return ESP_OK;
}

template <typename CTX> class PwmCap {
public:
  typedef std::function<void(pwm_item &, CTX &)> user_cb;

  PwmCap(gpio_num_t gpio, user_cb cb, CTX ctx) : gpio(gpio), cb(cb), ctx(ctx) {
    gpio_reset_pin(this->gpio);
    gpio_config_t conf = {
        .pin_bit_mask = (uint64_t)1 << (uint64_t)this->gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&conf);
  }

  ~PwmCap() {
    if (this->started)
      this->stop();
    gpio_reset_pin(this->gpio);
  }

  PwmCap(const PwmCap &) = delete;
  PwmCap &operator=(const PwmCap &) = delete;

  esp_err_t start() {
    this->started = true;
    return gpio_isr_handler_add(this->gpio, gpio_isr_handler, this);
  }

  esp_err_t stop() {
    this->started = false;
    return gpio_isr_handler_remove(this->gpio);
  }

private:
  static void gpio_isr_handler(void *arg) {
    const int64_t curr_time{esp_timer_get_time()};
    PwmCap *instance{static_cast<PwmCap *>(arg)};
    const int level{gpio_get_level(instance->gpio)};

    if (level) {
      pwm_item item = {
          .rising = instance->last_rising,
          .ton = static_cast<uint32_t>(instance->fall - instance->last_rising),
          .toff = static_cast<uint32_t>(curr_time - instance->fall),
      };

      instance->last_rising = curr_time;

      // call user cb with item as an argument
      instance->cb(item, instance->ctx);
    } else {
      instance->fall = curr_time;
    }
  }

  gpio_num_t gpio{GPIO_NUM_NC};

  bool started{false};

  int64_t fall{};
  int64_t last_rising{};

  user_cb cb{};
  CTX ctx{};
};
} // namespace pwm_cap