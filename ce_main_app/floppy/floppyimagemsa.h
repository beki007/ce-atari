// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEMSA_H
#define FLOPPYIMAGEMSA_H

#include "floppyimage.h"
#include "gmock/gmock.h"

class FloppyImageMsa: public FloppyImage
{
public:
    virtual bool open(const char *fileName);
    virtual bool save(const char *fileName);

protected:
    virtual bool loadImageIntoMemory(void);
};

class MockFloppyImageMsa: public FloppyImageMsa
{
public:
    MOCK_METHOD1(open, bool (const char *fileName));
    MOCK_METHOD1(save, bool (const char *fileName));
    MOCK_METHOD0(loadImageIntoMemory, bool ());

};

#endif // FLOPPYIMAGEST_H
