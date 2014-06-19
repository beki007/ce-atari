#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <map>

#include "utils.h"
#include "global.h"
#include "conspi.h"
#include "acsidatatrans.h"

#include "devfinder.h"
#include "devchangeshandler.h"

#include "native/translatedbootmedia.h"
#include "native/scsi.h"
#include "translated/translateddisk.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "config/configstream.h"

#include "floppy/floppyimagefactory.h"
#include "floppy/mfmdecoder.h"
#include "floppy/mfmcachedimage.h"
#include "floppy/floppysetup.h"

#include "version.h"

#ifdef ONPC
#include <termios.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

class CCoreThread: public ISettingsUser, public DevChangesHandler
{
public:
    CCoreThread();
    virtual ~CCoreThread();

	void resetHansAndFranz(void);
    void run(void);

    #ifdef ONPC
    void runOnPc(void);
    int  serialSetup(termios *ts);
    int  readBuffer(int fd, BYTE *bfr, WORD cnt);
    #endif

    void sendHalfWord(void);
    virtual void reloadSettings(int type);    								// from ISettingsUser

	virtual void onDevAttached(std::string devName, bool isAtariDrive);		// from DevChangesHandler
	virtual void onDevDetached(std::string devName);						// from DevChangesHandler
	
private:
    bool shouldRun;
    bool running;

    //-----------------------------------
    // data connection to STM32 slaves over SPI
    CConSpi         *conSpi;
    AcsiDataTrans   *dataTrans;

    //-----------------------------------
    // settings and config stuff
    ConfigStream            *confStream;
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    DevFinder		devFinder;
    Scsi            *scsi;
    TranslatedDisk  *translated;
    BYTE            acsiIDevType[8];
    BYTE            enabledIDbits;
    bool            setEnabledIDbits;
    BYTE            sdCardAcsiId;

    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    //-----------------------------------
    // mount and attach stuff
	std::multimap<std::string, std::string> mapDeviceToHostPaths;
	bool gotDevTypeRaw;
	bool gotDevTypeTranslated;

	void attachDevAsTranslated(std::string devName);
	void mountAndAttachSharedDrive(void);

    //-----------------------------------
    // handle FW version
    void handleFwVersion(int whichSpiCs);
    int bcdToInt(int bcd);

    //-----------------------------------
    // floppy stuff
    FloppySetup         floppySetup;
    ImageSilo           floppyImageSilo;
    bool                setEnabledFloppyImgs;
    int                 lastFloppyImageLed;

    void handleSendTrack(void);
    void handleSectorWritten(void);

};

#endif // CCORETHREAD_H
