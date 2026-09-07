#ifndef INTERNALTEMPERATURE_H
#define INTERNALTEMPERATURE_H
    #include "adc.h"

    /**
     * @brief The RP2040/RP2350's built-in temperature sensor, as an Adc
     * extension: it is read through the ADC exactly like a GPIO input
     * (see Adc's @brief in adc.h), just on a fixed channel
     * (ADC_TEMPERATURE_CHANNEL_NUM) that has no physical pin behind it
     * - see ADC_GPIO_NONE. Adds one thing beyond Adc: calibration_offset,
     * added to every reading (see GetInternalTemperatureCelsius()) - the
     * sensor's absolute accuracy without it is poor (the datasheet's own
     * formula assumes an exact 3.3V reference and a fixed 0.706V/27degC
     * reference point neither of which is guaranteed per board), so a
     * one-point calibration against a real thermometer is the practical
     * way to get a trustworthy absolute value - see
     * CalibrateInternalTemperature().
     */
    typedef struct {
        Adc base;
        float calibration_offset;
    } InternalTemperature;

    /**
     * @brief Allocates a new InternalTemperature and enables the chip's
     * temperature sensor input on the ADC. Registers via Agent_Init()
     * with timeout == 0: like Adc, there is no cache to refresh, so no
     * FreeRTOS task is needed for it. Starts uncalibrated
     * (calibration_offset == 0) - see CalibrateInternalTemperature().
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @return InternalTemperature* Newly heap-allocated, already-enabled sensor.
     */
    InternalTemperature *NewInternalTemperature(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority);

    /**
     * @brief Reads a fresh raw sample (see GetAdcValue()), converts it
     * to degrees Celsius using the formula from the RP2040/RP2350
     * datasheet, and applies temp's calibration_offset - never cached,
     * exact at the moment of the call.
     *
     * @param temp InternalTemperature to read.
     * @return float Calibrated temperature in degrees Celsius (equal to
     * the raw formula's result if CalibrateInternalTemperature() was
     * never called).
     */
    float GetInternalTemperatureCelsius(InternalTemperature *temp);

    /**
     * @brief One-point calibration: reads the sensor right now and sets
     * temp's calibration_offset to whatever constant makes that reading
     * equal actualCelsius - every GetInternalTemperatureCelsius() call
     * afterwards applies this same offset. Call it once, right after
     * construction, with a real temperature measured independently
     * (e.g. with a thermometer) at that exact moment; calling it again
     * later replaces the offset with a fresh one.
     *
     * @param temp InternalTemperature to calibrate.
     * @param actualCelsius The real, externally measured temperature right now.
     */
    void CalibrateInternalTemperature(InternalTemperature *temp, float actualCelsius);

#endif
