#include "VirtualManager.hh"

#include <stdexcept>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <tinyxml.h>

VirtualManager::VirtualManager()
{
	/* Connect to hypervisor */
	conn = virConnectOpen(NULL);
	if (conn == NULL) {
		throw std::runtime_error("Unable to connect to hypervisor");
	}
}

VirtualManager::~VirtualManager()
{
	virConnectClose(conn);
}

std::vector<std::string> VirtualManager::listVms()
{
	std::vector<std::string> ret;

	/* List domains */
	int numVms = virConnectNumOfDomains(conn);
	int vms[numVms];
	numVms = virConnectListDomains(conn, vms, numVms);
	if (numVms == -1) {
		throw std::runtime_error("Call to virConnectListDomains() failed.");
	}

	for (int i = 0; i < numVms; i++) {
		virDomainPtr domain = virDomainLookupByID(conn, vms[i]);
		ret.push_back(virDomainGetName(domain));
	}

	return ret;
}

std::string VirtualManager::lookUpVmByMac(const unsigned char lookedUpMac[6])
{
	std::string ret;

	/* List domains */
	int numVms = virConnectNumOfDomains(conn);
	int vms[numVms];
	numVms = virConnectListDomains(conn, vms, numVms);
	if (numVms == -1) {
		throw std::runtime_error("Call to virConnectListDomains() failed.");
	}

	for (int i = 0; i < numVms; i++) {
		virDomainPtr domain = virDomainLookupByID(conn, vms[i]);
		if (domain == NULL) continue;

		char *xmlDesc = virDomainGetXMLDesc(domain, 0);
		if (xmlDesc == NULL) continue;

		TiXmlDocument doc;
		doc.Parse(xmlDesc);
		TiXmlElement *devicesElement = NULL, *interfaceElement = NULL, *macElement = NULL;
		const char *macAddress = NULL;

		devicesElement = doc.RootElement()->FirstChildElement("devices");
		if (devicesElement)
			interfaceElement = devicesElement->FirstChildElement("interface");
		if (interfaceElement)
			macElement = interfaceElement->FirstChildElement("mac");
		if (macElement)
			macAddress = macElement->Attribute("address");
		if (!macAddress) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
		unsigned char currentMac[6];
		sscanf(macAddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			&currentMac[0], &currentMac[1], &currentMac[2], &currentMac[3], &currentMac[4], &currentMac[5]);
#pragma GCC diagnostic pop

		if (memcmp(currentMac, lookedUpMac, 6) == 0)
			return virDomainGetName(domain);
	}

	throw std::runtime_error("No VM with the requested MAC");
}

void VirtualManager::setVmCap(std::string name, int cap)
{
	virDomainPtr domain = virDomainLookupByName(conn, name.c_str());
	if (domain == NULL) {
		throw std::runtime_error(std::string() +
			"Unable to find domain by name: " +
			virGetLastError()->message);
	}

	int nvcpus = cap / 100 + ((cap % 100 == 0) ? 0 : 1);
	virDomainSetVcpus(domain, nvcpus);

	virTypedParameter param;
	strcpy(param.field, VIR_DOMAIN_SCHEDULER_CAP);
	param.type = VIR_TYPED_PARAM_UINT;
	param.value.ui = cap;
	if (virDomainSetSchedulerParameters(domain, &param, 1) == -1) {
		throw std::runtime_error(std::string() +
			"Unable to find domain by name: " +
			virGetLastError()->message);
	}

	int numParams = 10; /* way too many */
	virTypedParameter params[numParams];
	virDomainGetSchedulerParameters(domain, params, &numParams);
}
