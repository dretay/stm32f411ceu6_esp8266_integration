#include "stm32_hal_mock.h"
#include <string.h>

// Mock RTC handle
RTC_HandleTypeDef hrtc;

// Mock implementations for HAL functions
HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format) {
    // Return a mock time for testing
    sTime->Hours = 10;
    sTime->Minutes = 30;
    sTime->Seconds = 45;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format) {
    // Return a mock date for testing
    sDate->Year = 24;  // 2024
    sDate->Month = RTC_MONTH_JANUARY;
    sDate->Date = 8;
    sDate->WeekDay = RTC_WEEKDAY_MONDAY;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format) {
    // Mock: do nothing
    return HAL_OK;
}

HAL_StatusTypeDef HAL_RTC_SetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format) {
    // Mock: do nothing
    return HAL_OK;
}
