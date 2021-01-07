#ifndef _PROPD_H
#define _PROPD_H

//#include "properties.h"
#include "init.h"

#define PROPERTY_KEY_MAX   32
#define PROPERTY_VALUE_MAX  92

#define SYSTEM_PROPERTY_PIPE_NAME       "/tmp/linux-sysprop"
#define SYSTEM_PROPERTY_LIST_NAME       "/tmp/linux-sysprop-list"

enum {
    kSystemPropertyUnknown = 0,
    kSystemPropertyGet,
    kSystemPropertySet,
    kSystemPropertyList
};

/* one property entry */
typedef struct property {
    char    key[PROPERTY_KEY_MAX];
    char    value[PROPERTY_VALUE_MAX];

	struct listnode plist;
}Property;


void handle_property_set_fd(int listen_sock);
int start_property_service(void);
void property_init(void);
unsigned char property_set(const char *key, const char *value);
#endif//_PROPD_H