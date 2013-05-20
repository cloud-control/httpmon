#ifndef VIRTUAL_MANAGER_HH
#define VIRTUAL_MANAGER_HH

#include <string>
#include <vector>

/* XXX: ugly, forward declarations from libvirt */
typedef struct _virConnect virConnect;
typedef virConnect *virConnectPtr;

class VirtualManager
{
public:
	VirtualManager();
	~VirtualManager();

	std::vector<std::string> listVms();
	std::string lookUpVmByMac(const unsigned char mac[6]);
	void setVmCap(std::string name, int cap);

private:
	virConnectPtr conn;
};

#endif /* VIRTUAL_MANAGER_HH */
