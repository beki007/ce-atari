#ifndef _MOUNTER_H_
#define _MOUNTER_H_

#include <string>
#include "datatypes.h"

#define MOUNTER_ACTION_MOUNT			0
#define MOUNTER_ACTION_UMOUNT			1
#define MOUNTER_ACTION_RESTARTNETWORK	2
#define MOUNTER_ACTION_SYNC             3
#define MOUNTER_ACTION_MOUNT_ZIP        4

#define MOUNTACTION_STATE_NOT_STARTED   0
#define MOUNTACTION_STATE_IN_PROGRESS   1
#define MOUNTACTION_STATE_DONE          2

typedef struct {
    int     id;
    BYTE    state;
    DWORD   changeTime;
} TMountActionState;

typedef struct {
	int			action;
	bool		deviceNotShared;
	
	std::string	devicePath;				// used when deviceNotShared is true
	
	struct {							// used when deviceNotShared is false
		std::string host;
		std::string hostDir;
		bool		nfsNotSamba;
        
        std::string username;
        std::string password;
	} shared;

	std::string mountDir;				// location where it should be mounted
    
    int     mountActionStateId;         // TMountActionState.id, which should be updated on change in this mount action
} TMounterRequest;

extern "C" {
	int   mountAdd(TMounterRequest &tmr);
	void *mountThreadCode(void *ptr);
}

class Mounter 
{
public:
	bool mountShared(char *host, char *hostDir, bool nfsNotSamba, char *mountDir, char *username, char *password);
	bool mountDevice(char *devicePath, char *mountDir);
    void mountZipFile(char *zipFilePath, char *mountDir);
	void umountIfMounted(char *mountDir);
	void restartNetwork(void);
	void sync(void);

private:
	bool mount(char *mountCmd, char *mountDir);

	bool isAlreadyMounted(char *source);
	bool isMountdirUsed(char *mountDir);
	bool tryUnmount(char *mountDir);
	
	void createSource(char *host, char *hostDir, bool nfsNotSamba, char *source);
	
	bool mountDumpContains(char *searchedString);
    bool wlan0IsPresent(void);

    void copyTextFileToLog(char *path);
};

#endif

