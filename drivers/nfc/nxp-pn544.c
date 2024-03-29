/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/nfc/nxp-pn544.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#define MAX_BUFFER_SIZE	512

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	unsigned int		clk_req_gpio;
	struct clk		*ref_clk;
	struct regulator 	*pvdd_reg;
//	struct regulator	*pmuvcc_reg;
	bool				irq_enabled;
	spinlock_t			irq_enabled_lock;
};

static irqreturn_t pn544_dev_clk_req_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;

	disable_irq_nosync(irq);

	if(gpio_get_value(pn544_dev->clk_req_gpio)) {
		clk_enable(pn544_dev->ref_clk);
	} else {
		clk_disable(pn544_dev->ref_clk);
	}

	enable_irq(irq);
	return IRQ_HANDLED;
}

static void pn544_disable_irq(struct pn544_dev *pn544_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn544_dev->irq_enabled_lock, flags);
	if (pn544_dev->irq_enabled) {
		disable_irq_nosync(pn544_dev->client->irq);
		pn544_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn544_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;

	pn544_disable_irq(pn544_dev);

	/* Wake up waiting readers */
	wake_up(&pn544_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&pn544_dev->read_mutex);

	if (!gpio_get_value(pn544_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		while (1) {
			pn544_dev->irq_enabled = true;
			enable_irq(pn544_dev->client->irq);
			ret = wait_event_interruptible(
					pn544_dev->read_wq,
					!pn544_dev->irq_enabled);

			pn544_disable_irq(pn544_dev);

			if (ret)
				goto fail;

			if (gpio_get_value(pn544_dev->irq_gpio))
				break;

			pr_warning("%s: spurious interrupt detected\n", __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn544_dev->client, tmp, count);

	mutex_unlock(&pn544_dev->read_mutex);

	/* pn544 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn544_dev->read_mutex);
	return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev  *pn544_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	pn544_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);
	/* Write data */
	ret = i2c_master_send(pn544_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* pn544 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = container_of(filp->private_data,
						struct pn544_dev,
						pn544_device);

	filp->private_data = pn544_dev;

	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static long  pn544_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;

	switch (cmd) {
	case PN544_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_info("%s power on with firmware\n", __func__);
			regulator_enable(pn544_dev->pvdd_reg);
			gpio_set_value(pn544_dev->ven_gpio, 1);
			msleep(20);
			if (pn544_dev->firm_gpio)
				gpio_set_value(pn544_dev->firm_gpio, 1);
			msleep(20);
			gpio_set_value(pn544_dev->ven_gpio, 0);
			msleep(100);
			gpio_set_value(pn544_dev->ven_gpio, 1);
			msleep(20);
		} else if (arg == 1) {
			/* power on */
			pr_info("%s power on\n", __func__);
			regulator_enable(pn544_dev->pvdd_reg);
			if (pn544_dev->firm_gpio)
				gpio_set_value(pn544_dev->firm_gpio, 0);
			gpio_set_value(pn544_dev->ven_gpio, 1);
			msleep(100);
		} else  if (arg == 0) {
			/* power off */
			pr_info("%s power off\n", __func__);
			regulator_disable(pn544_dev->pvdd_reg);
			if (pn544_dev->firm_gpio)
				gpio_set_value(pn544_dev->firm_gpio, 0);
			gpio_set_value(pn544_dev->ven_gpio, 0);
			msleep(100);
		} else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn544_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn544_dev_read,
	.write	= pn544_dev_write,
	.open	= pn544_dev_open,
	.unlocked_ioctl  = pn544_dev_ioctl,
};

static int pn544_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct pn544_i2c_platform_data *platform_data = NULL;
	struct pn544_dev *pn544_dev = NULL;

	pr_err("*** %s : Enter\n", __func__);
    if (client->dev.of_node) {

		platform_data = devm_kzalloc(&client->dev,
			sizeof(struct pn544_i2c_platform_data), GFP_KERNEL);

		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		platform_data->irq_gpio = of_get_named_gpio_flags(client->dev.of_node, "nxp,irq-gpio", 0, &platform_data->irq_gpio_flags);
		pr_err("*** %s : platform_data->irq_gpio = %d\n", __func__, platform_data->irq_gpio);
		platform_data->ven_gpio = of_get_named_gpio_flags(client->dev.of_node, "nxp,enable-gpio", 0, &platform_data->ven_gpio_flags);
		pr_err("*** %s : platform_data->ven_gpio = %d\n", __func__, platform_data->ven_gpio);
		platform_data->firm_gpio = of_get_named_gpio_flags(client->dev.of_node, "nxp,dload-gpio", 0, &platform_data->firm_gpio_flags);
		pr_err("*** %s : platform_data->firm_gpio = %d\n", __func__, platform_data->firm_gpio);
		platform_data->clk_req_gpio = of_get_named_gpio_flags(client->dev.of_node, "nxp,clk-req-gpio", 0, &platform_data->clk_req_gpio_flags);
		pr_err("*** %s : platform_data->clk_req_gpio = %d\n", __func__, platform_data->clk_req_gpio);

	    platform_data->pvdd_reg = regulator_get(&client->dev, "nxp,pvdd");
	    if(IS_ERR(platform_data->pvdd_reg)) {
	        pr_err("*** %s: could not get nxp,pvdd-supply, rc=%ld\n", __func__, PTR_ERR(platform_data->pvdd_reg));
	        platform_data->pvdd_reg = NULL;
	    }
	    else {
		    pr_err("*** %s: platform_data->pvdd_reg obtained\n", __func__);
	    }
	}
	else {
		platform_data = client->dev.platform_data;
	}

	if (platform_data == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	ret = gpio_request(platform_data->irq_gpio, "nfc_int");
	if (ret) {
		pr_err("%s : Failed to request nfc_int\n", __func__);
		return  -ENODEV;
	}
	ret = gpio_request(platform_data->ven_gpio, "nfc_ven");
	if (ret)
		goto err_ven;

	if ((int)platform_data->firm_gpio > 0) {
		ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
		if (ret)
			goto err_firm;
	}

	if((int)platform_data->clk_req_gpio > 0) {
		ret = gpio_request(platform_data->clk_req_gpio, "nfc_clk_req");
		if (ret)
			goto err_clk_req;
	}


	pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
	if (pn544_dev == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn544_dev->ref_clk = clk_get(&client->dev, "ref_clk");
	if(IS_ERR(pn544_dev->ref_clk)) {
		pr_err("failed to get ref clk for %s. err msg:%ld\n", dev_name(&client->dev), PTR_ERR(pn544_dev->ref_clk));
		ret = PTR_ERR(pn544_dev->ref_clk);
		goto err_exit;
	}
	if((int)platform_data->clk_req_gpio > 0) {
		clk_prepare(pn544_dev->ref_clk);
	} else {
		/* if the clock request pin is not present, we have to permanently enable the ref clock. */
		clk_prepare_enable(pn544_dev->ref_clk);
	}

	pn544_dev->irq_gpio = platform_data->irq_gpio;
	pn544_dev->ven_gpio  = platform_data->ven_gpio;
	pn544_dev->firm_gpio  = platform_data->firm_gpio;
	pn544_dev->clk_req_gpio = platform_data->clk_req_gpio;
	pn544_dev->client   = client;
	pn544_dev->pvdd_reg = platform_data->pvdd_reg;

	ret = gpio_direction_input(pn544_dev->irq_gpio);
	if (ret < 0) {
		pr_err("%s :not able to set irq_gpio as input\n", __func__);
		goto err_ven;
	}
	ret = gpio_direction_output(pn544_dev->ven_gpio, 0);
	if (ret < 0) {
		pr_err("%s : not able to set ven_gpio as output\n", __func__);
		goto err_firm;
	}
	gpio_set_value(pn544_dev->ven_gpio, 1);
	if ((int)platform_data->firm_gpio > 0) {
		ret = gpio_direction_output(pn544_dev->firm_gpio, 0);
		if (ret < 0) {
			pr_err("%s : not able to set firm_gpio as output\n",
				 __func__);
			goto err_exit;
		}
	}
	if ((int)platform_data->clk_req_gpio > 0) {
		ret = gpio_direction_input(pn544_dev->clk_req_gpio);
		if(ret < 0) {
			pr_err("%s : not able to set clk_req_gpio as input\n", __func__);
			goto err_exit;
		}
	}

	/* init mutex and queues */
	init_waitqueue_head(&pn544_dev->read_wq);
	mutex_init(&pn544_dev->read_mutex);
	spin_lock_init(&pn544_dev->irq_enabled_lock);

	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	pn544_dev->pn544_device.name = "pn544";
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn544_dev->irq_enabled = true;
	ret = request_irq(client->irq, pn544_dev_irq_handler,
			  IRQF_TRIGGER_HIGH, client->name, pn544_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn544_disable_irq(pn544_dev);

	if((int)platform_data->clk_req_gpio > 0) {
		pr_info("%s : requesting clock request IRQ %d\n", __func__, gpio_to_irq(pn544_dev->clk_req_gpio));
		ret = request_irq(gpio_to_irq(pn544_dev->clk_req_gpio), pn544_dev_clk_req_irq_handler,
				IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "pn547_clk_req", pn544_dev);
	}
	else {
		pr_info("%s : clock request pin not found\n", __func__);
	}

	i2c_set_clientdata(client, pn544_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	mutex_destroy(&pn544_dev->read_mutex);
	kfree(pn544_dev);
err_exit:
	if (pn544_dev->clk_req_gpio)
		gpio_free(platform_data->clk_req_gpio);
err_clk_req:
	if (pn544_dev->firm_gpio)
		gpio_free(platform_data->firm_gpio);
err_firm:
	gpio_free(platform_data->ven_gpio);
err_ven:
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	clk_disable_unprepare(pn544_dev->ref_clk);
	free_irq(client->irq, pn544_dev);
	misc_deregister(&pn544_dev->pn544_device);
	mutex_destroy(&pn544_dev->read_mutex);
	gpio_free(pn544_dev->irq_gpio);
	gpio_free(pn544_dev->ven_gpio);
	if (pn544_dev->firm_gpio)
		gpio_free(pn544_dev->firm_gpio);
	if (pn544_dev->clk_req_gpio)
		gpio_free(pn544_dev->clk_req_gpio);
	regulator_disable(pn544_dev->pvdd_reg);
	regulator_put(pn544_dev->pvdd_reg);
	kfree(pn544_dev);

	return 0;
}

static int pn544_suspend(struct device *dev)
{
	struct pn544_dev *pn544_dev;
	pn544_dev = dev_get_drvdata(dev);

/* Add desired suspend activities here. */
	return 0;
}

static int pn544_resume(struct device *dev)
{
	struct pn544_dev *pn544_dev;
	pn544_dev = dev_get_drvdata(dev);

/* Add desired resume activities here. */
	return 0;
}

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};


#ifdef CONFIG_OF
static struct of_device_id nfc_match_table[] = {
	{ .compatible = "nxp,pn547",},
	{ },
};
#else
#define nfc_match_table NULL
#endif

static const struct dev_pm_ops pn544_pm_ops = {
	.suspend	= pn544_suspend,
	.resume		= pn544_resume,
};

static struct i2c_driver pn544_driver = {
	.id_table	= pn544_id,
	.probe		= pn544_probe,
	.remove		= pn544_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
		.of_match_table = nfc_match_table,
		.pm		= &pn544_pm_ops,
	},
};

/*
 * module load/unload record keeping
 */

static int __init pn544_dev_init(void)
{
	pr_info("Loading pn544 driver\n");
	return i2c_add_driver(&pn544_driver);
}
module_init(pn544_dev_init);

static void __exit pn544_dev_exit(void)
{
	pr_info("Unloading pn544 driver\n");
	i2c_del_driver(&pn544_driver);
}
module_exit(pn544_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");

