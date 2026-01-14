/*
 * thermistor_generator.c
 * High-level API for thermistor simulation
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/shell/shell.h>
#include "thermistor_sim.h"

#define NUM_CHANNELS 41

// Your existing N2HET PWM control structure
// (adapt this to match your actual implementation)
struct pwm_channel {
    const struct device *dev;
    uint32_t channel;
    float current_duty;  // Current duty cycle (0-100%)
    float current_temp;  // Current simulated temperature
};

static struct pwm_channel channels[NUM_CHANNELS];

// Initialize channels (adapt to your existing init code)
void thermistor_sim_init(void) {
    // Assuming you already have your N2HET PWM devices set up
    // Just initialize the temperature tracking
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].current_temp = 25.0f;  // Start at room temp
        channels[i].current_duty = therm_duty_from_temp(25.0f);
        
        // Call existing PWM set function
        // your_n2het_set_duty(i, channels[i].current_duty);
    }
}

// Set a channel to a specific temperature
int thermistor_set_temp(uint8_t channel, float temp_celsius) {
    if (channel >= NUM_CHANNELS) {
        return -EINVAL;
    }
    
    // Calculate duty cycle for this temperature
    float duty = therm_duty_from_temp(temp_celsius);
    
    // Update state
    channels[channel].current_temp = temp_celsius;
    channels[channel].current_duty = duty;
    
    // Call your existing N2HET PWM function
    // return your_n2het_set_duty(channel, duty);
    
    printk("Ch%02d: %.1f°C -> %.1fΩ -> %.3fV (%.1f%% duty)\n",
           channel, 
           temp_celsius,
           therm_resistance_from_temp(temp_celsius),
           therm_voltage_from_temp(temp_celsius),
           duty);
    
    return 0;
}

// Set all channels to same temperature
int thermistor_set_all_temp(float temp_celsius) {
    int ret;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ret = thermistor_set_temp(i, temp_celsius);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

// Set temperature gradient across all channels
int thermistor_set_gradient(float t_min, float t_max) {
    int ret;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        float temp = t_min + (t_max - t_min) * ((float)i / (NUM_CHANNELS - 1));
        ret = thermistor_set_temp(i, temp);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

// Temperature sweep on a channel
void thermistor_sweep_channel(uint8_t channel, float t_start, float t_end, 
                              uint32_t duration_ms, uint32_t steps) {
    uint32_t delay_ms = duration_ms / steps;
    
    for (uint32_t i = 0; i <= steps; i++) {
        float temp = t_start + (t_end - t_start) * ((float)i / steps);
        thermistor_set_temp(channel, temp);
        k_msleep(delay_ms);
    }
}

/*
 * Shell commands for thermistor simulator control
 */

// Command: therm set <channel> <temp>
static int cmd_therm_set(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "Usage: therm set <channel> <temp>");
        return -EINVAL;
    }
    
    int channel = atoi(argv[1]);
    float temp = atof(argv[2]);
    
    if (channel < 0 || channel >= NUM_CHANNELS) {
        shell_error(sh, "Channel must be 0-%d", NUM_CHANNELS - 1);
        return -EINVAL;
    }
    
    int ret = thermistor_set_temp(channel, temp);
    if (ret < 0) {
        shell_error(sh, "Failed to set temperature");
        return ret;
    }
    
    shell_print(sh, "Channel %d set to %.1f°C", channel, temp);
    return 0;
}

// Command: therm all <temp>
static int cmd_therm_all(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: therm all <temp>");
        return -EINVAL;
    }
    
    float temp = atof(argv[1]);
    
    int ret = thermistor_set_all_temp(temp);
    if (ret < 0) {
        shell_error(sh, "Failed to set temperatures");
        return ret;
    }
    
    shell_print(sh, "All channels set to %.1f°C", temp);
    return 0;
}

// Command: therm gradient <t_min> <t_max>
static int cmd_therm_gradient(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "Usage: therm gradient <t_min> <t_max>");
        return -EINVAL;
    }
    
    float t_min = atof(argv[1]);
    float t_max = atof(argv[2]);
    
    int ret = thermistor_set_gradient(t_min, t_max);
    if (ret < 0) {
        shell_error(sh, "Failed to set gradient");
        return ret;
    }
    
    shell_print(sh, "Gradient set: %.1f°C to %.1f°C", t_min, t_max);
    return 0;
}

// Command: therm sweep <channel> <t_start> <t_end> <duration_sec>
static int cmd_therm_sweep(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 5) {
        shell_error(sh, "Usage: therm sweep <ch> <t_start> <t_end> <dur_sec>");
        return -EINVAL;
    }
    
    int channel = atoi(argv[1]);
    float t_start = atof(argv[2]);
    float t_end = atof(argv[3]);
    int duration_sec = atoi(argv[4]);
    
    if (channel < 0 || channel >= NUM_CHANNELS) {
        shell_error(sh, "Channel must be 0-%d", NUM_CHANNELS - 1);
        return -EINVAL;
    }
    
    shell_print(sh, "Sweeping channel %d: %.1f°C to %.1f°C over %ds",
                channel, t_start, t_end, duration_sec);
    
    thermistor_sweep_channel(channel, t_start, t_end, duration_sec * 1000, 100);
    
    shell_print(sh, "Sweep complete");
    return 0;
}

// Command: therm info <channel>
static int cmd_therm_info(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "Usage: therm info <channel>");
        return -EINVAL;
    }
    
    int channel = atoi(argv[1]);
    
    if (channel < 0 || channel >= NUM_CHANNELS) {
        shell_error(sh, "Channel must be 0-%d", NUM_CHANNELS - 1);
        return -EINVAL;
    }
    
    float temp = channels[channel].current_temp;
    float resistance = therm_resistance_from_temp(temp);
    float voltage = therm_voltage_from_temp(temp);
    float duty = channels[channel].current_duty;
    
    shell_print(sh, "Channel %d:", channel);
    shell_print(sh, "  Temperature:  %.2f °C", temp);
    shell_print(sh, "  Resistance:   %.0f Ω", resistance);
    shell_print(sh, "  Voltage:      %.3f V", voltage);
    shell_print(sh, "  Duty Cycle:   %.2f %%", duty);
    
    return 0;
}

// Command: therm table
static int cmd_therm_table(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "\nThermistor Lookup Table:");
    shell_print(sh, "Temp(°C)  Resist(Ω)  Voltage(V)  Duty(%%)");
    shell_print(sh, "--------  ---------  ----------  -------");
    
    for (int temp = -40; temp <= 125; temp += 5) {
        float r = therm_resistance_from_temp(temp);
        float v = therm_voltage_from_temp(temp);
        float d = therm_duty_from_temp(temp);
        shell_print(sh, "%4d      %9.0f  %10.3f  %7.2f", temp, r, v, d);
    }
    
    return 0;
}

// Register shell commands
SHELL_STATIC_SUBCMD_SET_CREATE(therm_cmds,
    SHELL_CMD(set, NULL, "Set channel temp", cmd_therm_set),
    SHELL_CMD(all, NULL, "Set all channels", cmd_therm_all),
    SHELL_CMD(gradient, NULL, "Set temp gradient", cmd_therm_gradient),
    SHELL_CMD(sweep, NULL, "Sweep channel temp", cmd_therm_sweep),
    SHELL_CMD(info, NULL, "Show channel info", cmd_therm_info),
    SHELL_CMD(table, NULL, "Show lookup table", cmd_therm_table),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(therm, &therm_cmds, "Thermistor simulator commands", NULL);
