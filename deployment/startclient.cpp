#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* arg[])
{
	char cmd[100] ;

	for(int i = 71 ; i <= 75 ; i++)
	{
		sprintf(cmd, "ssh root@172.16.20.%d \"/home/wave_subscriber 1000 20 1 5000 1 1800 > /tmp/client_output_wave.debug 2>&1 &\"", i) ;
		system(cmd) ;
	}
}