#ifndef SETTINGSRELOADPROXY_H
#define SETTINGSRELOADPROXY_H

#include "ISettingsUser.h"

#define SETTINGSUSER_NONE       0
#define SETTINGSUSER_ACSI       1
#define SETTINGSUSER_TRANSLATED 2

#define MAX_SETTINGS_USERS      10

typedef struct {
    ISettingsUser   *su;
    int             type;
} TSettUsr;

class SettingsReloadProxy
{
public:
    SettingsReloadProxy();

    void addSettingsUser(ISettingsUser *su, int type);
    void reloadSettings(int type);

private:
    TSettUsr settUser[MAX_SETTINGS_USERS];

};

#endif // SETTINGSRELOADPROXY_H