#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRIVER_NAME "sr602-mod"

struct sr602_data {
    struct gpio_desc *gpiod;
    int irq;
};

static irqreturn_t sr602_irq_handler(int irq, void *dev_id)
{
    struct sr602_data *data = dev_id;
    int val = gpiod_get_value(data->gpiod);

    if (val)
        pr_info(DRIVER_NAME ": Motion detected! (GPIO high)\n");
    else
        pr_info(DRIVER_NAME ": Motion ended. (GPIO low)\n");

    return IRQ_HANDLED;
}

static int sr602_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct sr602_data *data;
    int ret;

    pr_info(DRIVER_NAME ": Probing device...\n");

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    /* Get the GPIO descriptor
     * Requires "gpios" or "<name>-gpios" property in DT node.
     * We'll try to find a generic gpio first if simple binding is used,
     * or rely on the DT overlay structure you have.
     * Based on your device tree:
     * motion {
     *     label = "Motion Detector SR602";
     *     gpios = <&gpio1 11 GPIO_ACTIVE_LOW>;
     * };
     */
    
    // In standard gpio-keys setup you'd just let gpio-keys handle it,
    // but if we want a custom module we should get it
    data->gpiod = devm_gpiod_get(dev, NULL, GPIOD_IN);
    if (IS_ERR(data->gpiod)) {
        pr_err(DRIVER_NAME ": Failed to get GPIO\n");
        return PTR_ERR(data->gpiod);
    }

    /* Convert GPIO to IRQ */
    data->irq = gpiod_to_irq(data->gpiod);
    if (data->irq < 0) {
        pr_err(DRIVER_NAME ": Failed to get IRQ\n");
        return data->irq;
    }

    ret = devm_request_irq(dev, data->irq, sr602_irq_handler,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_SHARED,
                           DRIVER_NAME, data);
    if (ret) {
        pr_err(DRIVER_NAME ": Failed to request IRQ (%d)\n", ret);
        return ret;
    }

    platform_set_drvdata(pdev, data);
    pr_info(DRIVER_NAME ": Probed successfully (IRQ=%d)\n", data->irq);

    return 0;
}

static int sr602_remove(struct platform_device *pdev)
{
    pr_info(DRIVER_NAME ": Device removed\n");
    return 0;
}

static const struct of_device_id sr602_of_match[] = {
    { .compatible = "sr602-mod", },
    { }
};
MODULE_DEVICE_TABLE(of, sr602_of_match);

static struct platform_driver sr602_driver = {
    .probe = sr602_probe,
    .remove = sr602_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = sr602_of_match,
    },
};

module_platform_driver(sr602_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hogan");
MODULE_DESCRIPTION("SR602 PIR Sensor Test Driver");
