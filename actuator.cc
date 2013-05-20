#include <boost/program_options.hpp>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

int main(int argc, char **argv)
{
	namespace po = boost::program_options;

	/*
	 * Initialize
	 */
	std::string vmName;
	int cap = 100;

	/*
	 * Parse command-line
	 */
	po::options_description desc("Simple code to control Virtual Machines (VMs) "
		"running on this Physical Machine (PM)");
	desc.add_options()
		("help", "produce help message")
		("vm", po::value<std::string>(&vmName), "choose the VM to control")
		("cap", po::value<int>(&cap), "set cap")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	/*
	 * List domains
	 */
	std::cerr << "Currently running VMs" << std::endl;

	/* Connect to hypervisor */
	virConnectPtr conn = virConnectOpen(NULL);
	if (conn == NULL) {
		throw std::runtime_error("Unable to connect to hypervisor");
	}

	/* List domains */
	int numVms = virConnectNumOfDomains(conn);
	int vms[numVms];
	numVms = virConnectListDomains(conn, vms, numVms);
	if (numVms == -1) {
		throw std::runtime_error("Call to virConnectListDomains() failed.");
	}

	for (int i = 0; i < numVms; i++) {
		virDomainPtr domain = virDomainLookupByID(conn, vms[i]);
		std::cerr << "- " << virDomainGetName(domain);

		int numParams = 10; /* way too many */
		virTypedParameter params[numParams];
		virDomainGetSchedulerParameters(domain, params, &numParams);

		for (int i = 0; i < numParams; i++) {
			std::cerr << " " << params[i].field << "=" << params[i].value.ui;
		}
		std::cerr << std::endl;
	}

	/*
	 * Set cap
	 */
	if (!vmName.empty()) {
		virDomainPtr domain = virDomainLookupByName(conn, vmName.c_str());
		if (domain == NULL) {
			throw std::runtime_error(std::string() +
				"Unable to find domain by name: " +
				virGetLastError()->message);
		}

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

		std::cerr << "New scheduling parameters: ";
		for (int i = 0; i < numParams; i++) {
			std::cerr << " " << params[i].field << "=" << params[i].value.ui;
		}
		std::cerr << std::endl;
	}
}
