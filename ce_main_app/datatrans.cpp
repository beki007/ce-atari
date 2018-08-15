#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "datatrans.h"
#include "native/scsi_defs.h"

DataTrans::DataTrans()
{
    retryMod = new RetryModule();

    buffer          = new BYTE[ACSI_BUFFER_SIZE];        // 1 MB buffer
    recvBuffer      = new BYTE[ACSI_BUFFER_SIZE];

    memset(buffer,      0, ACSI_BUFFER_SIZE);            // init buffers to zero
    memset(recvBuffer,  0, ACSI_BUFFER_SIZE);

    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    dataDirection   = DATA_DIRECTION_READ;

    dumpNextData    = false;
}

DataTrans::~DataTrans()
{
    delete retryMod;
    delete []buffer;
    delete []recvBuffer;
}

void DataTrans::clear(bool clearAlsoDataDirection)
{
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;

    if(clearAlsoDataDirection) {
        dataDirection   = DATA_DIRECTION_READ;
    }

    dumpNextData    = false;
}

void DataTrans::setStatus(BYTE stat)
{
    status          = stat;
    statusWasSet    = true;
}

void DataTrans::addDataByte(BYTE val)
{
    buffer[count] = val;
    count++;
}

void DataTrans::addDataDword(DWORD val)
{
    buffer[count    ] = (val >> 24) & 0xff;
    buffer[count + 1] = (val >> 16) & 0xff;
    buffer[count + 2] = (val >>  8) & 0xff;
    buffer[count + 3] = (val      ) & 0xff;

    count += 4;
}

void DataTrans::addDataWord(WORD val)
{
    buffer[count    ] = (val >> 8) & 0xff;
    buffer[count + 1] = (val     ) & 0xff;

    count += 2;
}

void DataTrans::addDataBfr(const void *data, DWORD cnt, bool padToMul16)
{
    memcpy(&buffer[count], data, cnt);
    count += cnt;

    if(padToMul16) {                    // if should pad to multiple of 16
        padDataToMul16();
    }
}

void DataTrans::addDataCString(const char *data, bool padToMul16)
{
    // include null terminator in byte count
    addDataBfr(data, strlen(data) + 1, padToMul16);
}

void DataTrans::padDataToMul16(void)
{
    int mod = count % 16;           // how many we got in the last 1/16th part?
    int pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?

    if(mod != 0) {                  // if we should pad something
        memset(&buffer[count], 0, pad); // set the padded bytes to zero and add this count
        count += pad;

        // if((count % 512) != 0) {     // if it's not a full sector
        //     pad += 2;                // padding is greater than just to make mod16 == 0, to make the data go into ram
        // }
    }
}

DWORD DataTrans::getCount(void)
{
    return count;
}

void DataTrans::addZerosUntilSize(DWORD finalBufferCnt)
{
    while(count < finalBufferCnt) {         // add zeros until we don't have enough
        addDataByte(0);
    }
}

// get data from Hans
bool DataTrans::recvData(BYTE *data, DWORD cnt)
{
    bool res;

    dataDirection = DATA_DIRECTION_WRITE;                   // let the higher function know that we've done data write -- 130 048 Bytes
    res = recvData_start(cnt);                              // first send the command and tell Hans that we need WRITE data

    if(!res) {                                              // failed to start?
        return false;
    }

    res = recvData_transferBlock(data, cnt);                // get data from Hans
    return res;
}

void DataTrans::dumpDataOnce(void)
{
    dumpNextData = true;
}

void DataTrans::sendDataToFd(int fd)
{
    if(dataDirection == DATA_DIRECTION_WRITE) {
        count = 0;
        return;
    }

    if(count == 0) {    // if there's no data to send, send single zero byte
        buffer[0]   = 0;
        count       = 1;
    }

    WORD length = count;
    write(fd, &length, 2);      // first word - length of data to be received

    write(fd, buffer, count);   // then the data...
    count = 0;
}

void DataTrans::dumpData(void)
{
    Debug::out(LOG_DEBUG, "sendDataAndStatus: %d bytes", count);
    BYTE *src = buffer;

    WORD dumpCnt = 0;

    int lines = count / 32;
    if((count % 32) != 0) {
        lines++;
    }

    for(int i=0; i<lines; i++) {
        char bfr[1024];
        char *b = &bfr[0];

        for(int j=0; j<32; j++) {
            int val = (int) *src;
            src++;
            sprintf(b, "%02x ", val);
            b += 3;

            dumpCnt++;
            if(dumpCnt >= count) {
                break;
            }
        }

        Debug::out(LOG_DEBUG, "%s", bfr);
    }
}
