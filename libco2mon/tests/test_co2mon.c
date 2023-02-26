#include <stdio.h>
#include <stdlib.h>
#include "co2mon.h"

int main()
{
	int r = co2mon_init(-1);
	if (r < 0)
	{
		fprintf(stderr, "Failed to initialize co2mon library\n");
		return EXIT_FAILURE;
	}

	co2mon_device dev = co2mon_open_device();
	if (dev == NULL)
	{
		fprintf(stderr, "Failed to open CO2 monitor device\n");
		return EXIT_FAILURE;
	}

	co2mon_data_t magic_table = { 0 };
	if (!co2mon_send_magic_table(dev, magic_table))
	{
		fprintf(stderr, "Failed to send magic table to CO2 monitor device\n");
		return EXIT_FAILURE;
	}

	for (int i = 0; i < 20; i++) {
		co2mon_data_t result;
		int r = co2mon_read_data(dev, magic_table, result);
		if (r <= 0)
		{
			fprintf(stderr, "Error while reading data from device\n");
			return EXIT_FAILURE;
		}
	}

	co2mon_close_device(dev);
	co2mon_exit();
	return EXIT_SUCCESS;
}
