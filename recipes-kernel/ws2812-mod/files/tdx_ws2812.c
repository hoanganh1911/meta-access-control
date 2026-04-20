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
 * ROOT CAUSE ANALYSIS: With 10KΩ pull-up and ~50pF trace capacitance,
 * τ = R×C = 10000 × 50e-12 = 500ns. The DIN signal crosses WS2812 Vih
 * threshold (~3.5V) only after ~600ns (= -τ × ln(1 - 3.5/5)).
 *
 * Sub-microsecond HIGH pulses (ndelay approach) are INVISIBLE to the LED:
 *   T0H=250ns → DIN reaches only 1.97V (below 3.5V Vih) → WS2812 sees nothing!
 *
 * FIX: Use slow udelay timing so RC circuit has time to charge fully:
 *   T0H=2µs  → DIN reaches 4.91V (well above 3.5V) ✓
 *   T1H=6µs  → clearly distinguishable from T0H (3:1 ratio) ✓
 *   T0L=10µs, T1L=2µs → both well under 50µs WS2812 reset threshold ✓
 *
 * NOTE: If this works, permanent fix is replacing 10KΩ pull-up with 1KΩ
 * to allow normal 800kHz WS2812 timing (τ = 50ns, Vih at ~60ns).
 */
#define T0H_US  2   /* 2µs HIGH for bit-0 → RC charges to ~4.9V */
#define T0L_US  10  /* 10µs LOW for bit-0  → clearly LOW, < 50µs reset */
#define T1H_US  6   /* 6µs HIGH for bit-1  → 3× T0H, unmistakable */
#define T1L_US  2   /* 2µs LOW for bit-1 */

static inline void ws2812_send_byte(struct gpio_desc *gpio, u8 byte)
{
    int i;
    for (i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            /* Bit 1: long HIGH (6µs), short LOW (2µs) */
            gpiod_set_raw_value(gpio, 0); /* → DIN HIGH */
            udelay(T1H_US);
            gpiod_set_raw_value(gpio, 1); /* → DIN LOW  */
            udelay(T1L_US);
        } else {
            /* Bit 0: short HIGH (2µs), long LOW (10µs) */
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
