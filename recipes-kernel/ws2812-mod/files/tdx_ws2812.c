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

/* Timing constants in nanoseconds 
 * INVERTED LOGIC (MOSFET + 10k Pullup)
 * - Added 200ns to HIGH pulse to compensate for slow 10k pull-up rise time
 * - Subtracted 150ns for GPIO raw value overhead 
 */
#define T0H 400
#define T0L 450
#define T1H 750
#define T1L 250

static void ws2812_send_byte(struct gpio_desc *gpio, u8 byte) {
    int i;
    for (i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            // T1: Gửi HIGH ra LED => GPIO phải LOW (MOSFET tắt)
            gpiod_set_raw_value(gpio, 0); 
            ndelay(T1H);
            // Gửi LOW ra LED => GPIO phải HIGH (MOSFET mở)
            gpiod_set_raw_value(gpio, 1);
            ndelay(T1L);
        } else {
            // T0: Gửi HIGH ra LED => GPIO phải LOW
            gpiod_set_raw_value(gpio, 0);
            ndelay(T0H);
            // Gửi LOW ra LED => GPIO phải HIGH
            gpiod_set_raw_value(gpio, 1);
            ndelay(T0L);
        }
    }
}

static void ws2812_update(struct ws2812_data *priv) {
    unsigned long flags;
    int i;

    local_irq_save(flags);
    
    for (i = 0; i < priv->num_leds * 3; i++) {
        ws2812_send_byte(priv->gpio, priv->colors[i]);
    }
    
    // Idle state: Chân LED phải là LOW => GPIO phải HIGH (MOSFET mở ngậm)
    gpiod_set_raw_value(priv->gpio, 1);
    local_irq_restore(flags);
    
    udelay(300); /* Xung Reset lớn hỗ trợ WS2812B mới */
}

static ssize_t colors_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    struct ws2812_data *priv = dev_get_drvdata(dev);
    int r, g, b, i;
    
    if (sscanf(buf, "%d %d %d", &r, &g, &b) != 3)
        return -EINVAL;

    for (i = 0; i < priv->num_leds; i++) {
        priv->colors[i*3] = (u8)g;     
        priv->colors[i*3 + 1] = (u8)r;
        priv->colors[i*3 + 2] = (u8)b;
    }
    
    ws2812_update(priv);
    
    return count;
}

static DEVICE_ATTR_WO(colors);

static int ws2812_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    struct ws2812_data *priv;
    int ret;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;

    // Khoi tao muc HIGH (Mosfet dan -> LED nhan muc 0 mac dinh)
    priv->gpio = devm_gpiod_get(dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->gpio)) {
        dev_err(dev, "Failed to get GPIO\n");
        return PTR_ERR(priv->gpio);
    }

    priv->num_leds = 7; 
    platform_set_drvdata(pdev, priv);

    ret = device_create_file(dev, &dev_attr_colors);
    if (ret) return ret;

    dev_info(dev, "WS2812 Inverted Logic tuned driver initialized\n");
    return 0;
}

static int ws2812_remove(struct platform_device *pdev) {
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
    .probe = ws2812_probe,
    .remove = ws2812_remove,
};

module_platform_driver(ws2812_driver);
MODULE_AUTHOR("Antigravity");
MODULE_LICENSE("GPL");
