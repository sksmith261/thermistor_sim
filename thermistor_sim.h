/*
 * thermistor_sim.h
 * Thermistor voltage simulation for N2HET PWM outputs
 */

#ifndef THERMISTOR_SIM_H
#define THERMISTOR_SIM_H

#include <stdint.h>
#include <math.h>


// Thermistor parameters (10K NTC)
#define THERM_VCC           3.3f
#define THERM_R_FIXED       10000.0f
#define THERM_R_NOMINAL     10000.0f
#define THERM_BETA          3950.0f
#define THERM_T_NOMINAL     25.0f

// Temperature -> Resistance (Beta equation)
static inline float therm_resistance_from_temp(float temp_celsius) {
    float t_kelvin = temp_celsius + 273.15f;
    float t_nom_kelvin = THERM_T_NOMINAL + 273.15f;
    float exp_term = expf(THERM_BETA * (1.0f/t_kelvin - 1.0f/t_nom_kelvin));
    return THERM_R_NOMINAL * exp_term;
}

// Resistance -> Voltage (voltage divider)
static inline float therm_voltage_from_resistance(float r_thermistor) {
    return THERM_VCC * (THERM_R_FIXED / (r_thermistor + THERM_R_FIXED));
}

// Temperature -> Voltage (combined)
static inline float therm_voltage_from_temp(float temp_celsius) {
    float r = therm_resistance_from_temp(temp_celsius);
    return therm_voltage_from_resistance(r);
}

// Voltage -> Duty Cycle % (for PWM)
static inline float therm_duty_from_voltage(float voltage) {
    return (voltage / THERM_VCC) * 100.0f;
}

// Temperature -> Duty Cycle % (direct conversion)
static inline float therm_duty_from_temp(float temp_celsius) {
    float voltage = therm_voltage_from_temp(temp_celsius);
    return therm_duty_from_voltage(voltage);
}

// Reverse: Voltage -> Temperature (for verification)
static inline float therm_temp_from_voltage(float voltage) {
    // Solve voltage divider for R_thermistor
    float r_therm = THERM_R_FIXED * (THERM_VCC / voltage - 1.0f);
    
    // Solve Beta equation for temperature
    float t_nom_kelvin = THERM_T_NOMINAL + 273.15f;
    float t_kelvin = 1.0f / ((1.0f / t_nom_kelvin) + 
                             (1.0f / THERM_BETA) * logf(r_therm / THERM_R_NOMINAL));
    
    return t_kelvin - 273.15f;
}

#endif /* THERMISTOR_SIM_H */
