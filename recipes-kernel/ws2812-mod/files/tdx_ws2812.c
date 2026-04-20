#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>

#define DRIVER_NAME "ws2812_mod"
#define MAX_LEDS 64

struct ws2812_data {
    struct gpio_desc *gpio;
    u8 colors[MAX_LEDS * 3];
    int num_leds;
};

/*
 * Inverted logic: MOSFET AOD2408 open-drain with 10K pull-up.
 * GPIO=0 -> MOSFET OFF -> pull-up -> DIN HIGH
 * GPIO=1 -> MOSFET ON  -> drain LOW -> DIN LOW
 *
 * ndelay() on non-RT Linux kernel cannot guarantee sub-microsecond precision.
 * Minimum granularity is typically ~1µs. We use udelay() with integer
 * microseconds and accept that bit-rate will be slower (~200kHz instead of
 * 800kHz). WS2812B tolerates longer LOW periods; total bit time ≤ ~9µs is
 * within the spec's undefined range but works in practice when timing ratio
 * is correct. If LEDs still don't respond, use the SPI method instead.
 *
 * Timing (µs) for inverted MOSFET:
 *   Bit-1: GPIO=0 (HIGH) for T1H µs, then GPIO=1 (LOW) for T1L µs
 *   Bit-0: GPIO=0 (HIGH) for T0H µs, then GPIO=1 (LOW) for T0L µs
 *
 * Ratio T1H:T0H ≈ 2:1 is the key requirement for WS2812.
 */
#define T0H_US  1   /* HIGH pulse for bit-0: ~1µs  */
#define T0L_US  3   /* LOW  pulse for bit-0: ~3µs  */
#define T1H_US  2   /* HIGH pulse for bit-1: ~2µs  */
#define T1L_US  1   /* LOW  pulse for bit-1: ~1µs  */

static inline void ws2812_send_byte(struct gpio_desc *gpio, u8 byte)
{
    int i;
    for (i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            /* Bit 1: long HIGH, short LOW */
            gpiod_set_raw_value(gpio, 0); /* → DIN HIGH */
            udelay(T1H_US);
            gpiod_set_raw_value(gpio, 1); /* → DIN LOW  */
            udelay(T1L_US);
        } else {
            /* Bit 0: short HIGH, long LOW */
            gpiod_set_raw_value(gpio, 0); /* → DIN HIGH */
            udelay(T0H_US);
            gpiod_set_raw_value(gpio, 1); /* → DIN LOW  */
            udelay(T0L_US);
        }
    }
}

static void ws2812_update(struct ws2812_data *priv)
{
    unsigned long flags;
    int i;

    local_irq_save(flags);
    for (i = 0; i < priv->num_leds * 3; i++)
        ws2812_send_byte(priv->gpio, priv->colors[i]);

    /* Drive DIN LOW (GPIO=1 → MOSFET ON → line LOW) for reset >280µs */
    gpiod_set_raw_value(priv->gpio, 1);
    local_irq_restore(flags);

    udelay(300); /* reset pulse: ≥280µs */
}

static ssize_t colors_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct ws2812_data *priv = dev_get_drvdata(dev);
    int r, g, b, i;

    if (sscanf(buf, "%d %d %d", &r, &g, &b) != 3) {
        dev_err(dev, "Invalid format. Use: echo 'R G B' > colors\n");
        return -EINVAL;
    }

    /* WS2812 order is GRB */
    for (i = 0; i < priv->num_leds; i++) {
        priv->colors[i * 3]     = (u8)g;
        priv->colors[i * 3 + 1] = (u8)r;
        priv->colors[i * 3 + 2] = (u8)b;
    }

    dev_info(dev, "Setting %d LEDs: R=%d G=%d B=%d\n", priv->num_leds, r, g, b);
    ws2812_update(priv);
    return count;
}
static DEVICE_ATTR_WO(colors);

static int ws2812_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct ws2812_data *priv;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->gpio = devm_gpiod_get(dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->gpio)) {
        dev_err(dev, "Failed to get led-gpios: %ld\n", PTR_ERR(priv->gpio));
        return PTR_ERR(priv->gpio);
    }

    priv->num_leds = 7;
    platform_set_drvdata(pdev, priv);

    if (device_create_file(dev, &dev_attr_colors))
        return -EINVAL;

    /* Ensure idle state: DIN LOW (GPIO=1 → MOSFET ON → line LOW) */
    gpiod_set_raw_value(priv->gpio, 1);

    dev_info(dev, "WS2812 driver initialized: %d LEDs on GPIO, idle=LOW\n",
             priv->num_leds);
    return 0;
}

static int ws2812_remove(struct platform_device *pdev)
{
    device_remove_file(&pdev->dev, &dev_attr_colors);
    return 0;
}

static const struct of_device_id ws2812_of_match[] = {
    { .compatible = "custom,ws2812-bitbang", },
    { },
};
MODULE_DEVICE_TABLE(of, ws2812_of_match);

static struct platform_driver ws2812_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = ws2812_of_match,
    },
    .probe  = ws2812_probe,
    .remove = ws2812_remove,
};

module_platform_driver(ws2812_driver);
MODULE_AUTHOR("Antigravity");
MODULE_LICENSE("GPL");
