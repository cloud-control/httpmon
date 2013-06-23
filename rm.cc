#include <arpa/inet.h>
#include <boost/program_options.hpp>
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

double inline now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_usec / 1000000 + tv.tv_sec;
}

bool processMessage(int s,
	VirtualManager &vmm,
	const std::string virtualBridge,
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
		strcpy(arpreq.arp_dev, virtualBridge.c_str());
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

	//fprintf(stderr, "[%f] got message from %s(%s): %s\n", now(), inet_ntoa(sin.sin_addr), vmName.c_str(), buf);

	/* Store performance data */
	vmToPerformance[vmName] = std::max(std::min(atof(buf), 1.0), -1.0);

	return true;
}

bool rebalancePlatform(VirtualManager &vmm,
	double platformSize,
	double epsilonRm,
	std::map<std::string, double> &vmToPerformance,
	std::map<std::string, double> &vmToVp)
{
	std::vector<std::string> vms = vmm.listVms();
	vms.erase(vms.begin()); /* leave Dom-0 alone */

	/* Compute sum of matching values */
	double sumFik = 0;
	for (auto vm : vms)
		sumFik += vmToPerformance[vm]; /* zero if vm is not in map, i.e., SL-unaware */

	/* Update caps */
	int numNewVms = 0;
	for (auto vm : vms) {
		if (vmToVp[vm] == 0) /* new VM */ {
			numNewVms++;
			fprintf(stderr, "[%f] vm=%s new\n", now(), vm.c_str());
		}
		else if (vmToPerformance[vm] < 0)
			vmToVp[vm] -= epsilonRm * (vmToPerformance[vm] - vmToVp[vm] * sumFik);
	}

	/* Deal with new VMs */
	for (auto vm : vms) {
		if (vmToVp[vm] == 0) /* new VM */
			vmToVp[vm] = 1.0 / vms.size();
		else
			vmToVp[vm] *= 1.0 * (vms.size() - numNewVms) / vms.size();
	}

	/* Limit vp */
	for (auto vm : vms) {
		if (vmToVp[vm] < 0.125)
			vmToVp[vm] = 0.125;
		else if (vmToVp[vm] > 1)
			vmToVp[vm] = 1;
	}

	/* Rescale to make sure the sum is 1 */
	double sumVp = 0;
	for (auto vm : vms) sumVp += vmToVp[vm];
	for (auto vm : vms) vmToVp[vm] /= sumVp; // XXX: Can sumVp ever be zero?

	/* Apply new caps and report outcome*/
	for (auto vm : vms) {
		double cap = vmToVp[vm] * platformSize;
		fprintf(stderr, "[%f] vm=%s perf=%f vp=%f cap=%f\n",
			now(),
			vm.c_str(),
			vmToPerformance[vm],
			vmToVp[vm],
			cap);
		vmm.setVmCap(vm, cap); /* XXX: We might want to avoid useless changes here */
	}

	return false;
}

int main(int argc, char **argv)
{
	/* Parameters to program */
	double epsilonRm;
	double controlInterval;
	int nCpus;
	int listenPort;
	std::string virtualBridge;

	/* Parse command-line for epsilonRm, control interval and platform size */
	namespace po = boost::program_options;
	po::options_description desc("Game-theoretical resource manager for service-level-aware Cloud applications");
	desc.add_options()
		("help", "produce help message")
		("port", po::value<int>(&listenPort)->default_value(2712), "UDP port to listen on for receiving matching values")
		("epsilon", po::value<double>(&epsilonRm)->default_value(0.01), "use this value for epsilonRm; see paper for its meaning")
		("ncpus", po::value<int>(&nCpus)->default_value(4), "maximum number of processors available for allocating to VMs")
		("interval", po::value<double>(&controlInterval)->default_value(5), "control interval in seconds")
		("bridge", po::value<std::string>(&virtualBridge)->default_value("virbr0"), "use this (virtual) bridge for IP-to-MAC resolution")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	/* Set up some passive data structures */
	std::map<uint32_t, std::string> ipToVmCache;
	std::map<std::string, double> vmToPerformance;
	std::map<std::string, double> vmToVp;

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
	sa.sin_port = htons(listenPort);
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
		int waitForMs = (controlInterval - (now() - lastControl)) * 1000 + 1;
		if (waitForMs > 0)
			ret = poll(fds, 1, waitForMs);
		if (ret == -1) /* why would this ever happen? */
			perror("Error during poll()");

		/* Did we receive a packet? If so, process it. */
		if (ret > 0)
			processMessage(s, vmm, virtualBridge, ipToVmCache, vmToPerformance);

		/* Should we run the controller? If so, run it. */
		if (now() - lastControl > controlInterval) {
			rebalancePlatform(vmm, 100 * nCpus, epsilonRm, vmToPerformance, vmToVp);
			lastControl = now();
			/* Require VMs to report performance before next control interval */
			vmToPerformance.clear();
		}
	}
}
