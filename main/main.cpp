#include <iostream>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "pwm_cap.hpp"

QueueHandle_t my_queue{};

enum class Channel : unsigned int {
  Steering = 0,
  Throttle = 1,
};

struct my_pwm_item {
  pwm_cap::pwm_item item;
  Channel ch;
};

void my_user_cb(pwm_cap::pwm_item &item, Channel &ch) {
  my_pwm_item my_item = {
      .item = item,
      .ch = ch,
  };
  xQueueSendFromISR(my_queue, &my_item, NULL);
}

extern "C" void app_main(void) {
  my_queue = xQueueCreate(10, sizeof(my_pwm_item));
  if (!my_queue)
    exit(1);

  pwm_cap::init_for_all();

  pwm_cap::PwmCap steering_ch{GPIO_NUM_14, std::function(my_user_cb),
                              Channel::Steering};
  pwm_cap::PwmCap throttle_ch{GPIO_NUM_13, std::function(my_user_cb),
                              Channel::Throttle};

  steering_ch.start();
  throttle_ch.start();

  while (1) {
    using std::cout;

    my_pwm_item item{};
    if (xQueueReceive(my_queue, &item, portMAX_DELAY)) {
      cout << "t " << static_cast<unsigned int>(item.ch) << ": "
           << item.item.ton << "\n";
    }
  }

  while (1) {
    sleep(1);
  }
}
