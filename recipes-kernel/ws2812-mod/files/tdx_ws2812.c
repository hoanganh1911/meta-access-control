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
 * Timing targets (WS2812B spec, accounting for ~150ns gpiod_set_raw_value overhead):
 *   Bit-0: T0H_actual = T0H_ns + overhead ≈ 400ns, T0L_actual ≈ 900ns
 *   Bit-1: T1H_actual = T1H_ns + overhead ≈ 800ns, T1L_actual ≈ 600ns
 *
 * KEY BUG FIXED: original T0H == T0L = 600ns → 50% duty, WS2812 cannot
 * distinguish bit-0 from bit-1. Now T0H << T0L and T1H > T0H.
 *
 * local_irq_save() in ws2812_update ensures ndelay accuracy.
 */
#define T0H_NS  250  /* ndelay for bit-0 HIGH: 250ns + ~150ns overhead = ~400ns */
#define T0L_NS  750  /* ndelay for bit-0 LOW:  750ns + ~150ns overhead = ~900ns */
#define T1H_NS  650  /* ndelay for bit-1 HIGH: 650ns + ~150ns overhead = ~800ns */
#define T1L_NS  450  /* ndelay for bit-1 LOW:  450ns + ~150ns overhead = ~600ns */

static inline void ws2812_send_byte(struct gpio_desc *gpio, u8 byte)
{
    int i;
    for (i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            /* Bit 1: T1H HIGH (~800ns), T1L LOW (~600ns) */
            gpiod_set_raw_value(gpio, 0); /* → DIN HIGH */
            ndelay(T1H_NS);
            gpiod_set_raw_value(gpio, 1); /* → DIN LOW  */
            ndelay(T1L_NS);
        } else {
            /* Bit 0: T0H HIGH (~400ns), T0L LOW (~900ns) */
            gpiod_set_raw_value(gpio, 0); /* → DIN HIGH */
            ndelay(T0H_NS);
            gpiod_set_raw_value(gpio, 1); /* → DIN LOW  */
            ndelay(T0L_NS);
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
