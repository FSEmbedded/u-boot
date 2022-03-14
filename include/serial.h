#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <post.h>

struct serial_device {
	/* enough bytes to match alignment of following func pointer */
	char	name[16];

	int	(*start)(const struct serial_device *sdev);
	int	(*stop)(const struct serial_device *sdev);
	void	(*setbrg)(const struct serial_device *sdev);
	int	(*getc)(const struct serial_device *sdev);
	int	(*tstc)(const struct serial_device *sdev);
	void	(*putc)(const struct serial_device *sdev, const char c);
	void	(*puts)(const struct serial_device *sdev, const char *s);
#if CONFIG_POST & CONFIG_SYS_POST_UART
	void	(*loop)(const struct serial_device *sdev, int);
#endif
	void	*priv;
	struct serial_device *next;
};

void default_serial_puts(const char *s);

extern struct serial_device serial_smc_device;
extern struct serial_device serial_scc_device;
extern struct serial_device *default_serial_console(void);

#if	defined(CONFIG_MPC83xx) || defined(CONFIG_MPC85xx) || \
	defined(CONFIG_MPC86xx) || \
	defined(CONFIG_TEGRA) || defined(CONFIG_SYS_COREBOOT) || \
	defined(CONFIG_MICROBLAZE)
extern struct serial_device serial0_device;
extern struct serial_device serial1_device;
#endif

extern void serial_register(struct serial_device *);
extern void serial_initialize(void);
extern void serial_stdio_init(void);
extern int serial_assign(const char *name);
extern void serial_reinit_all(void);

/* For usbtty */
#ifdef CONFIG_USB_TTY

extern int usbtty_getc(void);
extern void usbtty_putc(const char c);
extern void usbtty_puts(const char *str);
extern int usbtty_tstc(void);

#else

/* stubs */
#define usbtty_getc() 0
#define usbtty_putc(a)
#define usbtty_puts(a)
#define usbtty_tstc() 0

#endif /* CONFIG_USB_TTY */

struct udevice;

enum serial_par {
	SERIAL_PAR_NONE,
	SERIAL_PAR_ODD,
	SERIAL_PAR_EVEN
};

#define SERIAL_PAR_SHIFT	0
#define SERIAL_PAR_MASK		(0x03 << SERIAL_PAR_SHIFT)
#define SERIAL_GET_PARITY(config) \
	((config & SERIAL_PAR_MASK) >> SERIAL_PAR_SHIFT)

enum serial_bits {
	SERIAL_5_BITS,
	SERIAL_6_BITS,
	SERIAL_7_BITS,
	SERIAL_8_BITS
};

#define SERIAL_BITS_SHIFT	2
#define SERIAL_BITS_MASK	(0x3 << SERIAL_BITS_SHIFT)
#define SERIAL_GET_BITS(config) \
	((config & SERIAL_BITS_MASK) >> SERIAL_BITS_SHIFT)

enum serial_stop {
	SERIAL_HALF_STOP,	/* 0.5 stop bit */
	SERIAL_ONE_STOP,	/*   1 stop bit */
	SERIAL_ONE_HALF_STOP,	/* 1.5 stop bit */
	SERIAL_TWO_STOP		/*   2 stop bit */
};

#define SERIAL_STOP_SHIFT	4
#define SERIAL_STOP_MASK	(0x3 << SERIAL_STOP_SHIFT)
#define SERIAL_GET_STOP(config) \
	((config & SERIAL_STOP_MASK) >> SERIAL_STOP_SHIFT)

#define SERIAL_CONFIG(par, bits, stop) \
		     (par << SERIAL_PAR_SHIFT | \
		      bits << SERIAL_BITS_SHIFT | \
		      stop << SERIAL_STOP_SHIFT)

#define SERIAL_DEFAULT_CONFIG	SERIAL_PAR_NONE << SERIAL_PAR_SHIFT | \
				SERIAL_8_BITS << SERIAL_BITS_SHIFT | \
				SERIAL_ONE_STOP << SERIAL_STOP_SHIFT

/**
 * struct struct dm_serial_ops - Driver model serial operations
 *
 * The uclass interface is implemented by all serial devices which use
 * driver model.
 */
struct dm_serial_ops {
	/**
	 * setbrg() - Set up the baud rate generator
	 *
	 * Adjust baud rate divisors to set up a new baud rate for this
	 * device. Not all devices will support all rates. If the rate
	 * cannot be supported, the driver is free to select the nearest
	 * available rate. or return -EINVAL if this is not possible.
	 *
	 * @dev: Device pointer
	 * @baudrate: New baud rate to use
	 * @return 0 if OK, -ve on error
	 */
	int (*setbrg)(struct udevice *dev, int baudrate);
	/**
	 * getc() - Read a character and return it
	 *
	 * If no character is available, this should return -EAGAIN without
	 * waiting.
	 *
	 * @dev: Device pointer
	 * @return character (0..255), -ve on error
	 */
	int (*getc)(struct udevice *dev);
	/**
	 * puts() - puts a string
	 *
	 * @dev: Device pointer
	 * @str: string to write
	 * @return 0 if OK, -ve on error
	 */
	int (*puts)(struct udevice *dev, const char *str);
	/**
	 * putc() - Write a character
	 *
	 * @dev: Device pointer
	 * @ch: character to write
	 * @return 0 if OK, -ve on error
	 */
	int (*putc)(struct udevice *dev, const char ch);
	/**
	 * pending() - Check if input/output characters are waiting
	 *
	 * This can be used to return an indication of the number of waiting
	 * characters if the driver knows this (e.g. by looking at the FIFO
	 * level). It is acceptable to return 1 if an indeterminant number
	 * of characters is waiting.
	 *
	 * This method is optional.
	 *
	 * @dev: Device pointer
	 * @input: true to check input characters, false for output
	 * @return number of waiting characters, 0 for none, -ve on error
	 */
	int (*pending)(struct udevice *dev, bool input);
	/**
	 * clear() - Clear the serial FIFOs/holding registers
	 *
	 * This method is optional.
	 *
	 * This quickly clears any input/output characters from the UART.
	 * If this is not possible, but characters still exist, then it
	 * is acceptable to return -EAGAIN (try again) or -EINVAL (not
	 * supported).
	 *
	 * @dev: Device pointer
	 * @return 0 if OK, -ve on error
	 */
	int (*clear)(struct udevice *dev);
#if CONFIG_POST & CONFIG_SYS_POST_UART
	/**
	 * loop() - Control serial device loopback mode
	 *
	 * @dev: Device pointer
	 * @on: 1 to turn loopback on, 0 to turn if off
	 */
	int (*loop)(struct udevice *dev, int on);
#endif

	/**
	 * setconfig() - Set up the uart configuration
	 * (parity, 5/6/7/8 bits word length, stop bits)
	 *
	 * Set up a new config for this device.
	 *
	 * @dev: Device pointer
	 * @parity: parity to use
	 * @bits: bits number to use
	 * @stop: stop bits number to use
	 * @return 0 if OK, -ve on error
	 */
	int (*setconfig)(struct udevice *dev, uint serial_config);
};

/**
 * struct serial_dev_priv - information about a device used by the uclass
 *
 * @sdev:	stdio device attached to this uart
 *
 * @buf:	Pointer to the RX buffer
 * @rd_ptr:	Read pointer in the RX buffer
 * @wr_ptr:	Write pointer in the RX buffer
 */
struct serial_dev_priv {
	struct stdio_dev *sdev;

	char *buf;
	int rd_ptr;
	int wr_ptr;
};

/* Access the serial operations for a device */
#define serial_get_ops(dev)	((struct dm_serial_ops *)(dev)->driver->ops)

void atmel_serial_initialize(void);
void mcf_serial_initialize(void);
void mpc85xx_serial_initialize(void);
void mpc8xx_serial_initialize(void);
void mxc_serial_initialize(void);
void ns16550_serial_initialize(void);
void pl01x_serial_initialize(void);
void pxa_serial_initialize(void);
void sh_serial_initialize(void);

#endif

#ifdef CONFIG_SERIAL_SOFTWARE_FIFO
void	serial_buffered_init (void);
void	serial_buffered_putc (const struct stdio_dev *pdev, const char);
void	serial_buffered_puts (const struct stdio_dev *pdev, const char *);
int	serial_buffered_getc (const struct stdio_dev *pdev);
int	serial_buffered_tstc (const struct stdio_dev *pdev);
#endif /* CONFIG_SERIAL_SOFTWARE_FIFO */

/* arch/$(ARCH)/cpu/$(CPU)/$(SOC)/serial.c */
int	serial_init   (void);
void	serial_setbrg (void);
void	serial_putc   (const char);
void	serial_puts   (const char *);
int	serial_getc   (void);
int	serial_tstc   (void);
#ifdef CONFIG_DM_SERIAL
int	serial_get_alias_seq(void);
#endif
