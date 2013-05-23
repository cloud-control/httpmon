#include <arpa/inet.h>
#include <map>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "VirtualManager.hh"

#define MAX_MESSAGE_SIZE 1024 /*< maximum number of bytes accepted through UDP */
#define CONTROL_INTERVAL 5 /*< how often should platform be re-balanced */
#define VIRTUAL_BRIDGE "virbr0" /*< the bridge on which VMs are connected, for IP-to-MAC translation */

double inline now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_usec / 1000000 + tv.tv_sec;
}

bool processMessage(int s,
	VirtualManager &vmm,
	std::map<uint32_t, std::string> &ipToVmCache,
	std::map<std::string, double> &vmToPerformance)
{
	char buf[MAX_MESSAGE_SIZE + 1 /* for null terminator */];

	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	int len = recvfrom(s, buf, 1024, 0, (sockaddr*)&sin, &sinlen);

	if (len <= 0) {
		perror("Error, recvfrom() failed");
		return false;
	}

	/* "Convert" buffer to C string */
	buf[len] = 0;

	/*
	 * Retrieve VM's name
	 */
	std::string vmName;

	/* First, check cache */
	if (ipToVmCache.count(sin.sin_addr.s_addr) == 1) {
		vmName = ipToVmCache[sin.sin_addr.s_addr];
	}
	else {
		/* IP not in cache */
		/* Map IP to MAC address by doing an ARP query */
		struct arpreq arpreq;
		arpreq.arp_flags = 0;
		memcpy(&arpreq.arp_pa, &sin, sizeof(struct sockaddr_in));
		strcpy(arpreq.arp_dev, "virbr0");
		if (ioctl(s, SIOCGARP, &arpreq) == -1) {
			perror("Error, unable to retrieve MAC address");
			return false;
		}
		const unsigned char *mac = (unsigned char *)&arpreq.arp_ha.sa_data;

		/* Then map the MAC to a VM using the hypervisor-provided functions */
		vmName = vmm.lookUpVmByMac(mac);

		/* Update the cache */
		ipToVmCache[sin.sin_addr.s_addr] = vmName;
	}

	fprintf(stderr, "Got message from %s(%s): %s\n", inet_ntoa(sin.sin_addr), vmName.c_str(), buf);

	/* Store performance data */
	vmToPerformance[vmName] = atof(buf);

	return true;
}

bool rebalancePlatform(VirtualManager &vmm, std::map<std::string, double> &vmToPerformance)
{
	fprintf(stderr, "Rebalancing platform\n");

	std::vector<std::string> vms = vmm.listVms();
	for (auto vm : vms) {
		fprintf(stderr, "- %s: %f\n", vm.c_str(), vmToPerformance[vm]);
	}
	return false;
}

int main(int argc, char **argv)
{
	/* Set up some passive data structures */
	std::map<uint32_t, std::string> ipToVmCache;
	std::map<std::string, double> vmToPerformance;

	/* Class to chat with hypervisor */
	VirtualManager vmm;

	/* Create UDP socket */
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("Fatal error, socket() failed");
		exit(EXIT_FAILURE);
	}

	/* Start listening */
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(2712);
	sa.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (const sockaddr *)&sa, sizeof(sa)) == -1) {
		perror("Fatal error, bind() failed");
		exit(EXIT_FAILURE);
	}

	/* Packet loop */
	double lastControl = now();
	for (;;) {
		/* Wait for a packet to arrive or the next control interval */
		int ret = 0; /* as if poll() had timed out */
		struct pollfd fds[1] = {{ s, POLLIN }};
		int waitForMs = (CONTROL_INTERVAL - (now() - lastControl)) * 1000 + 1;
		fprintf(stderr, "Waiting for %dms\n", waitForMs);
		if (waitForMs > 0)
			ret = poll(fds, 1, waitForMs);
		if (ret == -1) /* why would this ever happen? */
			perror("Error during poll()");

		/* Did we receive a packet? If so, process it. */
		if (ret > 0)
			processMessage(s, vmm, ipToVmCache, vmToPerformance);

		/* Should we run the controller? If so, run it. */
		if (now() - lastControl > CONTROL_INTERVAL) {
			rebalancePlatform(vmm, vmToPerformance);
			lastControl = now();
		}
	}
}
