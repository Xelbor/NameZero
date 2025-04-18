#include <Arduino.h>

// Статусы таймера
enum TimerStatus {
  TIMER_IDLE,
  TIMER_RUNNING,
  TIMER_FINISHED
};

// Структура таймера
struct Timer {
  TimerStatus status;
  uint32_t duration_ms;
  TaskHandle_t handle;
};

// Универсальная функция запуска таймера
Timer* startTimer(uint32_t duration_ms, BaseType_t coreID = 1) {
  Timer* timer = new Timer{TIMER_RUNNING, duration_ms, NULL};

  auto timerTask = [](void* param) {
    Timer* t = static_cast<Timer*>(param);
    vTaskDelay(pdMS_TO_TICKS(t->duration_ms));
    t->status = TIMER_FINISHED;
    vTaskDelete(NULL);
  };

  xTaskCreatePinnedToCore(
    timerTask,
    "TimerTask",
    2048,
    timer,
    1,
    &timer->handle,
    coreID
  );

  return timer; // Возвращаем указатель на объект таймера
}
