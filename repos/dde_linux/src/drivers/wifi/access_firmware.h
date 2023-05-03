#ifndef _ACCESS_FIRMWARE_H_
#define _ACCESS_FIRMWARE_H_

struct Stat_firmware_result
{
	bool success;
	size_t length;
};

Stat_firmware_result access_firmware(char const *path);


struct Read_firmware_result
{
	bool success;
};

Read_firmware_result read_firmware(char const *path, char *dst, size_t dst_len);

#endif /* _ACCESS_FIRMWARE_H_ */
