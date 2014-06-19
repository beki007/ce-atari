#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../acsidatatrans.h"
#include "../translated/gemdos_errno.h"
#include "floppysetup.h"

#define UPLOAD_PATH "/tmp/"

FloppySetup::FloppySetup()
{
    dataTrans   = NULL;
    up          = NULL;
    imageSilo   = NULL;
    translated  = NULL;

    currentUpload.fh = NULL;

    currentDownload.fh = NULL;
}

FloppySetup::~FloppySetup()
{

}

void FloppySetup::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void FloppySetup::setImageSilo(ImageSilo *imgSilo)
{
    imageSilo = imgSilo;
}

void FloppySetup::setTranslatedDisk(TranslatedDisk *td)
{
    translated = td;
}

void FloppySetup::processCommand(BYTE *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "FloppySetup::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(imageSilo == 0) {
        Debug::out(LOG_ERROR, "FloppySetup::processCommand was called without valid imageSilo, can't do image setup stuff!");
        dataTrans->setStatus(FDD_ERROR);
        dataTrans->sendDataAndStatus();
        return;
    }

    switch(cmd[4]) {
        case FDD_CMD_IDENTIFY:                          // return identification string
            dataTrans->addDataBfr((unsigned char *)"CosmosEx floppy setup", 21, true);       // add identity string with padding
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_GETSILOCONTENT:                    // return the filenames and contents of current floppy images in silo
            BYTE bfr[512];
            imageSilo->dumpStringsToBuffer(bfr);

            dataTrans->addDataBfr(bfr, 512, true);
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_UPLOADIMGBLOCK_START:      uploadStart();              break;

        case FDD_CMD_UPLOADIMGBLOCK_FULL:       
        case FDD_CMD_UPLOADIMGBLOCK_PART:       uploadBlock();              break;

        case FDD_CMD_UPLOADIMGBLOCK_DONE_OK:    
        case FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL:  uploadEnd(false);           break;

        case FDD_CMD_SWAPSLOTS:                 imageSilo->swap(cmd[5]);    dataTrans->setStatus(FDD_OK);	break;
        case FDD_CMD_REMOVESLOT:                imageSilo->remove(cmd[5]);  dataTrans->setStatus(FDD_OK);	break;

        case FDD_CMD_NEW_EMPTYIMAGE:            newImage();                 break;

        case FDD_CMD_DOWNLOADIMG_START:         downloadStart();            break;
        case FDD_CMD_DOWNLOADIMG_GETBLOCK:      downloadGetBlock();         break;
        case FDD_CMD_DOWNLOADIMG_DONE:          downloadDone();             break;
    }

    dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
}

void FloppySetup::uploadStart(void)
{
    int index = cmd[5];

    if(index < 0 || index > 2) {                            // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(!translated) {                                       // can't work without this!
        return;
    }

    if(currentUpload.fh != NULL) {                          // if file was open but not closed, close it and don't copy it...
        fclose(currentUpload.fh);
    }

	memset(bfr64k, 0, 512);
    dataTrans->recvData(bfr64k, 512);                       // receive file name into this buffer
    std::string atariFilePath = (char *) bfr64k;
    std::string hostPath;
    std::string pathWithHostSeparators;

    // the following block should device, if on-device-copy of file is enough (when using translated drive), or should upload from ST
    bool doOnDeviceCopy = false;

    // try to convert atari path to host path (will work for translated drives, will fail for native)
    bool res = translated->createHostPath(atariFilePath, hostPath);

    if(res) {                                               // good? file is on translated drive
        if(translated->hostPathExists(hostPath)) {          // file exists? do on-device-copy of file
            pathWithHostSeparators = hostPath;
            doOnDeviceCopy = true;
        } 
    }
     
    if(!doOnDeviceCopy) {                                   // if we got here, then the file is either not on translated drive, or does not exist
        hostPath = "";
        
        pathWithHostSeparators = atariFilePath;             // convert atari path to host path separators
        translated->pathSeparatorAtariToHost(pathWithHostSeparators);
    }

    std::string path, file;
    Utils::splitFilenameFromPath(pathWithHostSeparators, path, file);

    path = UPLOAD_PATH + file;
	
    FILE *f = NULL;

    if(!doOnDeviceCopy) {                                   // if not doing on-device-copy, try to open the file
        f = fopen((char *) path.c_str(), "wb");

        if(!f) {                                            // failed to open file?
            Debug::out(LOG_ERROR, "FloppySetup::uploadStart - failed to open file %s", (char *) path.c_str());
    
            dataTrans->setStatus(FDD_ERROR);
            return;
        }
    }

    // store params
    currentUpload.slotIndex             = index;
    currentUpload.fh                    = f;
    currentUpload.atariSourcePath       = atariFilePath;            // atari path:                      C:\bla.st
    currentUpload.hostSourcePath        = hostPath;                 // host path for translated drives: /mnt/sda/bla.st
    currentUpload.hostDestinationPath   = path;                     // host destination:                /tmp/bla.st
    currentUpload.file                  = file;                     // just file name:                  bla.st

    // do on-device-copy if needed
    if(doOnDeviceCopy) {                                            // if doing on-device-copy...
        res = Utils::copyFile(hostPath, path);                      // copy the file

        if(res) {                                                   // if file was copied
            cmd[4] = FDD_CMD_UPLOADIMGBLOCK_DONE_OK;
            cmd[5] = index;

            uploadEnd(true);                                        // pretend that we just uploaded it, this will also setStatus(...)
        } else {                                                    // if file copying failed, fail
            dataTrans->setStatus(FDD_ERROR);
        }
    } else {                                                        // if not doing on-device-copy, return with success
        dataTrans->setStatus(FDD_OK);
    }
}

void FloppySetup::uploadBlock(void)
{
    dataTrans->recvData(bfr64k, 64 * 1024);                 // get 64kB of data from ST

    BYTE slotIndex  = cmd[5] >> 6;                          // get slot # to which the upload should go
    //BYTE blockNo    = cmd[5] & 0x3f;                    

    if(slotIndex != currentUpload.slotIndex) {              // got open one slot, but trying to upload to another? fail!
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // determine how much data should we write to file
    size_t blockSize = 0;
    BYTE *ptr = NULL;

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_FULL) {             // get full block? transfer 64kB
        blockSize   = 64 * 1024;
        ptr         = bfr64k;
    } else if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_PART) {      // get partial block? Get block size as the 1st word in block
        blockSize   = (((WORD) bfr64k[0]) << 8) | ((WORD) bfr64k[1]);
        ptr         = bfr64k + 2;
    } else {                                                // this should never happen
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // write the data
    size_t written = fwrite(ptr, 1, blockSize, currentUpload.fh);

    if(written != blockSize) {                              // when didn't write all the data, fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    dataTrans->setStatus(FDD_OK);                           // when all data was written
}

void FloppySetup::uploadEnd(bool isOnDeviceCopy)
{
    if(currentUpload.fh != 0) {                             // file was open?
        fclose(currentUpload.fh);                           // close it
        currentUpload.fh = NULL;
    }

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL) {        // if we're done with fail, don't do anything else
        dataTrans->setStatus(FDD_OK);
        return;
    }

    // we're here, the image upload succeeded, the following will also encode the image...
    imageSilo->add(currentUpload.slotIndex, currentUpload.file, currentUpload.hostDestinationPath, currentUpload.atariSourcePath, currentUpload.hostSourcePath, true);

    // now finish with OK status
    if(isOnDeviceCopy) {                                        // for on-device-copy send special status
        dataTrans->setStatus(FDD_UPLOADSTART_RES_ONDEVICECOPY);
    } else {                                                    // for normal upload send OK status
        dataTrans->setStatus(FDD_OK);                           
    }
}

void FloppySetup::newImage(void)
{
    int index = cmd[5];

    if(index < 0 || index > 2) {                        // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(currentUpload.fh != NULL) {                      // if file was open but not closed, close it and don't copy it...
        fclose(currentUpload.fh);
    }

    char file[128];
	getNewImageName(file);
    std::string fileStr = file;

    std::string path = UPLOAD_PATH + fileStr;

    // open the file
    FILE *f = fopen((char *) path.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        Debug::out(LOG_ERROR, "FloppySetup::newImage - failed to open file %s", (char *) path.c_str());
    
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // create default boot sector (copied from blank .st image created in Steem)
    char sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    char sect0end[]     = {0x00, 0x97, 0xc7};

    char bfr[512];
    memset(bfr, 0, 512);

    memcpy(bfr, sect0start, sizeof(sect0start));                        // copy the start of default boot sector to start of buffer
    memcpy(bfr + 512 - sizeof(sect0end), sect0end, sizeof(sect0end));   // copy the end of default boot sector to end of buffer

    fwrite(bfr, 1, 512, f);

    // create the empty rest of the file
    memset(bfr, 0, 512);

    int totalSectorCount = (9*80*2) - 1;                                // calculate the count of sectors on a floppy - 2 sides, 80 tracks, 9 spt, minus the already written boot sector

    for(int i=0; i<totalSectorCount; i++) {
        fwrite(bfr, 1, 512, f);
    }

    fclose(f);

    // we're here, the image creation succeeded
    std::string empty;
    imageSilo->add(index, fileStr, path, empty, empty, true);

    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::getNewImageName(char *nameBfr)
{
	char fileName[24];

	for(int i=0; i<100; i++) {
		sprintf(fileName, "newimg%d.st", i);						// create new filename 
		
		std::string fnameWithPath = UPLOAD_PATH;
		fnameWithPath += fileName;									// this will be filename with path
		
		if(imageSilo->containsImage(fileName)) {					// if this file is already in silo, skip it
			continue;
		}
		
		if(translated->hostPathExists(fnameWithPath)) {				// if this file does exist, delete it (it's not in silo)
			unlink(fnameWithPath.c_str());
		}
		
		break;														// break this cycle and thus use this filename
	}
	
	strcpy(nameBfr, fileName);
}

void FloppySetup::downloadStart(void)
{
    int index = cmd[5];

    currentDownload.fh = NULL;

    if(index < 0 || index > 2) {                        // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    SiloSlot *ss = imageSilo->getSiloSlot(index);       // get silo slot

    if(ss->imageFile.empty()) {                         // silo slot is empty?
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    FILE *fh = fopen(ss->hostDestPath.c_str(), "rb");

    if(fh == NULL) {                                    // couldn't open file?
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    currentDownload.fh          = fh;
    currentDownload.imageFile   = ss->imageFile;

    dataTrans->addDataBfr((BYTE *) ss->imageFile.c_str(), ss->imageFile.length() + 1, true);     // send filename to ST
    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::downloadGetBlock(void)
{



}

void FloppySetup::downloadDone(void)
{
    int index = cmd[5];

    if(index < 0 || index > 2) {                        // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(currentDownload.fh != NULL) {                    // if file is open, close it
        fclose(currentDownload.fh);
        currentDownload.fh = NULL;
    }

    dataTrans->setStatus(FDD_OK);
}


