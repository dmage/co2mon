#include <stdio.h>
#include <libusb.h>

void swap_char(char* a, char* b)
{
    char tmp = *a;
    *a = *b;
    *b = tmp;
}

void decode_buf(char result[8], unsigned char buf[8], unsigned char magic_table[8])
{
    const unsigned char magic_word[8] = "Htemp99e";

    int i;
    unsigned char tmp;

    swap_char(&buf[0], &buf[2]);
    swap_char(&buf[1], &buf[4]);
    swap_char(&buf[3], &buf[7]);
    swap_char(&buf[5], &buf[6]);

    for (i = 0; i < 8; ++i)
    {
        buf[i] ^= magic_table[i];
    }

    tmp = (buf[7] << 5);
    result[7] = (buf[6] << 5) | (buf[7] >> 3);
    result[6] = (buf[5] << 5) | (buf[6] >> 3);
    result[5] = (buf[4] << 5) | (buf[5] >> 3);
    result[4] = (buf[3] << 5) | (buf[4] >> 3);
    result[3] = (buf[2] << 5) | (buf[3] >> 3);
    result[2] = (buf[1] << 5) | (buf[2] >> 3);
    result[1] = (buf[0] << 5) | (buf[1] >> 3);
    result[0] = tmp | (buf[0] >> 3);

    for (i = 0; i < 8; ++i)
    {
        result[i] -= (magic_word[i] << 4) | (magic_word[i] >> 4);
    }
}

void print_device_info(libusb_device* dev, struct libusb_device_descriptor* desc)
{
	int i = 0;
    int r;
	uint8_t path[8];

	printf("%04x:%04x (bus %d, device %d)",
        desc->idVendor, desc->idProduct,
        libusb_get_bus_number(dev), libusb_get_device_address(dev));

    r = libusb_get_port_numbers(dev, path, sizeof(path));
    if (r > 0) {
        printf(" path: %d", path[0]);
        for (i = 1; i < r; i++)
            printf(".%d", path[i]);
	}
    printf("\n");
}

int process_dev(libusb_device* dev, struct libusb_device_descriptor* desc)
{
    print_device_info(dev, desc);

    struct libusb_device_handle* handle;
    int r = libusb_open(dev, &handle);
    if (r != 0)
    {
        fprintf(stderr, "libusb_open error %d\n", r);
        return r;
    }

#ifdef __linux__
    libusb_detach_kernel_driver(handle, 0);
#endif

    r = libusb_claim_interface(handle, 0);
    if (r != 0)
    {
        fprintf(stderr, "libusb_claim_interface error %d\n", r);
        goto cleanup;
    }

    unsigned char magic_table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    r = libusb_control_transfer(handle,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        LIBUSB_REQUEST_SET_CONFIGURATION,
        0x0300, 0,
        magic_table, sizeof(magic_table),
        2000 /* milliseconds */);
    if (r < 0 || r != sizeof(magic_table))
    {
        fprintf(stderr, "libusb_control_transfer(out, magic_table) error %d\n", r);
        goto cleanup;
    }

    int actual_length;
    unsigned char data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    r = libusb_interrupt_transfer(handle,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
        data, sizeof(data), &actual_length,
        2000 /* milliseconds */);
    if (r != 0)
    {
        fprintf(stderr, "libusb_interrupt_transfer(in, data) error %d\n", r);
        goto cleanup;
    }
    if (actual_length != sizeof(data))
    {
        fprintf(stderr, "libusb_interrupt_transfer(in, data) trasferred %d bytes, expected %lu bytes\n", actual_length, (unsigned long)sizeof(data));
        goto cleanup;
    }

    int i;

#ifdef DEBUG
    printf("{");
    for (i = 0; i < actual_length; ++i)
    {
        if (i > 0)
            printf(",");
        printf(" 0x%02hhx", data[i]);
    }
    printf("}\n");
#endif

    unsigned char result[8];
    decode_buf(result, data, magic_table);
#ifdef DEBUG
    printf("{");
    for (i = 0; i < actual_length; ++i)
    {
        if (i > 0)
            printf(",");
        printf(" 0x%02hhx", result[i]);
    }
    printf("}\n");
#endif

    if (result[4] != 0x0d)
    {
        fprintf(stderr, "unexpected data from device (data[4] = %02hhx, await 0x0d)\n", result[4]);
    }

    unsigned char r0 = result[0];
    unsigned char r1 = result[1];
    unsigned char r2 = result[2];
    unsigned char r3 = result[3];
    unsigned char checksum = r0 + r1 + r2;
    if (checksum != r3)
    {
        fprintf(stderr, "checksum error (%02hhx, await %02hhx)\n", checksum, r3);
        goto cleanup;
    }

    uint16_t w = (result[1] << 8) + result[2];
    switch (result[0]) {
    case 0x42:
        printf("  [0x%02hhx] Temperature: %.4f\n", r0, (double)w * 0.0625 - 273.15);
        break;
    case 0x50:
        printf("  [0x%02hhx] CO2: %d\n", r0, w);
        break;
    default:
        printf("  [0x%02hhx] (unknown): %d\n", r0, w);
    }

cleanup:
    libusb_close(handle);
    return r;
}

void print_devs(libusb_device** devs)
{
	int i;
	libusb_device* dev;

	for (i = 0; (dev = devs[i]) != NULL; ++i)
    {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
        {
			fprintf(stderr, "libusb_get_device_descriptor error %d\n", r);
			return;
		}

        if (desc.idVendor == 0x04d9 && desc.idProduct == 0xa052)
        {
            process_dev(dev, &desc);
        }
	}
}

int main()
{
	libusb_device** devs;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int) cnt;

	print_devs(devs);

	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
}
