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
 * BẢN FIX CỰC ĐOAN (STRETCHED INVERTED LOGIC)
 * Bù thêm hẳn 500ns cho thời gian Rise Time siêu chậm của trở 10K.
 */
#define T0H 850  // Chờ LED nhận đủ 350ns
#define T0L 250  
#define T1H 1250 // Chờ LED nhận đủ 750ns
#define T1L 250

static void ws2812_send_byte(struct gpio_desc *gpio, u8 byte) {
    int i;
    for (i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            gpiod_set_raw_value(gpio, 0); 
            ndelay(T1H);
            gpiod_set_raw_value(gpio, 1);
            ndelay(T1L);
        } else {
            gpiod_set_raw_value(gpio, 0);
            ndelay(T0H);
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
    gpiod_set_raw_value(priv->gpio, 1);
    local_irq_restore(flags);
    udelay(300);
}

static ssize_t colors_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct ws2812_data *priv = dev_get_drvdata(dev);
    int r, g, b, i;
    if (sscanf(buf, "%d %d %d", &r, &g, &b) != 3) return -EINVAL;
    for (i = 0; i < priv->num_leds; i++) {
        priv->colors[i*3] = (u8)g; priv->colors[i*3 + 1] = (u8)r; priv->colors[i*3 + 2] = (u8)b;
    }
    ws2812_update(priv);
    return count;
}
static DEVICE_ATTR_WO(colors);

static int ws2812_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    struct ws2812_data *priv;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;
    priv->gpio = devm_gpiod_get(dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->gpio)) return PTR_ERR(priv->gpio);

    priv->num_leds = 7; 
    platform_set_drvdata(pdev, priv);
    if (device_create_file(dev, &dev_attr_colors)) return -EINVAL;
    dev_info(dev, "WS2812 v3 EXTREME STRETCH driver initialized\n");
    return 0;
}

static int ws2812_remove(struct platform_device *pdev) {
    device_remove_file(&pdev->dev, &dev_attr_colors);
    return 0;
}

static const struct of_device_id ws2812_of_match[] = { { .compatible = "custom,ws2812-bitbang", }, { }, };
MODULE_DEVICE_TABLE(of, ws2812_of_match);

static struct platform_driver ws2812_driver = {
    .driver = { .name = DRIVER_NAME, .of_match_table = ws2812_of_match, },
    .probe = ws2812_probe, .remove = ws2812_remove,
};

module_platform_driver(ws2812_driver);
MODULE_AUTHOR("Antigravity");
MODULE_LICENSE("GPL");
