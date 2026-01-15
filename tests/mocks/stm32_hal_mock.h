#ifndef STM32_HAL_MOCK_H
#define STM32_HAL_MOCK_H

#include <stdint.h>

// Mock STM32 HAL types and definitions
typedef enum {
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef struct {
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
} RTC_TimeTypeDef;

typedef struct {
    uint8_t WeekDay;
    uint8_t Month;
    uint8_t Date;
    uint8_t Year;
} RTC_DateTypeDef;

typedef struct {
    void *Instance;
} RTC_HandleTypeDef;

// RTC Format
#define RTC_FORMAT_BIN 0x00U
#define RTC_FORMAT_BCD 0x01U

// RTC Months
#define RTC_MONTH_JANUARY   ((uint8_t)0x01)
#define RTC_MONTH_FEBRUARY  ((uint8_t)0x02)
#define RTC_MONTH_MARCH     ((uint8_t)0x03)
#define RTC_MONTH_APRIL     ((uint8_t)0x04)
#define RTC_MONTH_MAY       ((uint8_t)0x05)
#define RTC_MONTH_JUNE      ((uint8_t)0x06)
#define RTC_MONTH_JULY      ((uint8_t)0x07)
#define RTC_MONTH_AUGUST    ((uint8_t)0x08)
#define RTC_MONTH_SEPTEMBER ((uint8_t)0x09)
#define RTC_MONTH_OCTOBER   ((uint8_t)0x10)
#define RTC_MONTH_NOVEMBER  ((uint8_t)0x11)
#define RTC_MONTH_DECEMBER  ((uint8_t)0x12)

// RTC WeekDay
#define RTC_WEEKDAY_MONDAY    ((uint8_t)0x01)
#define RTC_WEEKDAY_TUESDAY   ((uint8_t)0x02)
#define RTC_WEEKDAY_WEDNESDAY ((uint8_t)0x03)
#define RTC_WEEKDAY_THURSDAY  ((uint8_t)0x04)
#define RTC_WEEKDAY_FRIDAY    ((uint8_t)0x05)
#define RTC_WEEKDAY_SATURDAY  ((uint8_t)0x06)
#define RTC_WEEKDAY_SUNDAY    ((uint8_t)0x07)

// External mock RTC handle
extern RTC_HandleTypeDef hrtc;

// Mock HAL functions
HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format);
HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format);
HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format);
HAL_StatusTypeDef HAL_RTC_SetDate(RTC_HandleTypeDef *hrtc, RTC_DateTypeDef *sDate, uint32_t Format);

#endif // STM32_HAL_MOCK_H
