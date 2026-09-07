/**
 * @file internaltemperature.c
 * @brief Implementation of InternalTemperature: a thin Adc extension
 * that fixes the channel to the chip's built-in temperature sensor,
 * converts the raw sample to degrees Celsius, and applies a one-point
 * calibration offset (see CalibrateInternalTemperature() in
 * internaltemperature.h) since the raw formula's absolute accuracy is
 * poor without one.
 */

#include "internaltemperature.h"
#include <stdlib.h>
#include "hardware/adc.h"

/**
 * @brief Disables the temperature sensor input before the
 * InternalTemperature is freed. Chains to Adc_Delete() for the GPIO
 * side (a no-op here, since this Adc was configured with
 * ADC_GPIO_NONE), then undoes adc_set_temp_sensor_enabled(true) from
 * NewInternalTemperature().
 *
 * @param self The InternalTemperature being deleted, as its base Agent.
 */
static void InternalTemperature_Delete(Agent *self) {
    Adc_Delete(self);
    adc_set_temp_sensor_enabled(false);
}

/**
 * @brief Reads a fresh sample and converts it with the RP2040/RP2350
 * datasheet formula, before any calibration_offset is applied. Shared
 * by GetInternalTemperatureCelsius() and CalibrateInternalTemperature()
 * so both always agree on what "the raw reading" means.
 */
static float ReadRawCelsius(InternalTemperature *temp) {
    uint16_t raw = GetAdcValue(&temp->base);
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = (float)raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

/** @copydoc NewInternalTemperature */
InternalTemperature *NewInternalTemperature(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority) {
    InternalTemperature *temp = malloc(sizeof(InternalTemperature));
    temp->calibration_offset = 0.0f;
    Adc_Init(&temp->base, ADC_GPIO_NONE, ADC_TEMPERATURE_CHANNEL_NUM, Adc_Behave, InternalTemperature_Delete, name, timeout, uxStackDepth, uxPriority);
    adc_set_temp_sensor_enabled(true);
    return temp;
}

/** @copydoc GetInternalTemperatureCelsius */
float GetInternalTemperatureCelsius(InternalTemperature *temp) {
    return ReadRawCelsius(temp) + temp->calibration_offset;
}

/** @copydoc CalibrateInternalTemperature */
void CalibrateInternalTemperature(InternalTemperature *temp, float actualCelsius) {
    temp->calibration_offset = actualCelsius - ReadRawCelsius(temp);
}
