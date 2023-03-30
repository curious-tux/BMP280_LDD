#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

int main(void)
{
	int temp_fd,press_fd;
	char temp[]="0.0";
	char press[]="00000000.00";
	if( (temp_fd = open("/proc/temp", O_RDONLY)) < 0)
	{
		perror("temp open");
		exit(EXIT_FAILURE);
	}
	if( read(temp_fd, temp, sizeof(temp)) < 0)
	{
		perror("temp read");
		exit(EXIT_FAILURE);
	}
	printf("temp :%s",temp);
	close(temp_fd);
	if( (press_fd = open("/proc/press", O_RDONLY)) < 0)
	{
		perror("press open");
		exit(EXIT_FAILURE);
	}
	if( read(press_fd, press, sizeof(press)) < 0)
	{
		perror("press read");
		exit(EXIT_FAILURE);
	}
	printf("press :%s",press);
	close(press_fd);
	return 0;
}




