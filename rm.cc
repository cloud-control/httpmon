#include <arpa/inet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "VirtualManager.hh"

int main(int argc, char **argv)
{
	VirtualManager vmm;

	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("Fatal error, socket() failed");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(2712);
	sa.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (const sockaddr *)&sa, sizeof(sa)) == -1) {
		perror("Fatal error, bind() failed");
		exit(EXIT_FAILURE);
	}

	for (;;) { /* controller should never exit */
		char buf[1024];

		struct sockaddr_in sin;
		socklen_t sinlen = sizeof(sin);
		int len = recvfrom(s, buf, 1024, 0, (sockaddr*)&sin, &sinlen);

		if (len > 0) {
			buf[len] = 0;

			struct arpreq arpreq;
			arpreq.arp_flags = 0;
			memcpy(&arpreq.arp_pa, &sin, sizeof(struct sockaddr_in));
			strcpy(arpreq.arp_dev, "virbr0");
			if (ioctl(s, SIOCGARP, &arpreq) == -1) {
				perror("Error, unable to retrieve MAC address");
			}
			const unsigned char *mac = (unsigned char *)&arpreq.arp_ha.sa_data;
			std::string vmName = vmm.lookUpVmByMac(mac);

			fprintf(stderr, "Got message from %s(%s): %s\n", inet_ntoa(sin.sin_addr), vmName.c_str(), buf);
		}
		else {
			perror("Error, recvfrom() failed");
		}
	}
}
