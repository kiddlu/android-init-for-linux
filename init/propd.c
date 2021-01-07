//
// Copyright 2007 The Android Open Source Project
//
// Property sever.  Mimics behavior provided on the device by init(8) and
// some code built into libc.

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>

#include "propd.h"

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif

static bool set_property(const char* key, const char* value);

static list_declare(prop_list);

/*
 * Set default values for several properties.
 */
static void set_default_properties(void)
{
    static const struct {
        const char* key;
        const char* value;
    } propList[] = {
        { "ro.proccess.name", "propd" },
        { "ro.os.name", "GNU/Linux" },
        { "ro.test.string", "HelloWorld" },
    };

    for (int i = 0; i < NELEM(propList); i++)
        set_property(propList[i].key, propList[i].value);
}

/*
 * Get the value of a property.
 *
 * "valueBuf" must hold at least PROPERTY_VALUE_MAX bytes.
 *
 * Returns "true" if the property was found.
 */
static bool get_property(const char* key, char* valueBuf)
{
    assert(key != NULL);
    assert(valueBuf != NULL);

	struct listnode *node;
	Property *prop;


    list_for_each(node, &prop_list) {
        prop = node_to_item(node, Property, plist);
        if (strcmp(prop->key, key) == 0) {
            if (strlen(prop->value) >= PROPERTY_VALUE_MAX) {
                fprintf(stderr,
                    "GLITCH: properties table holds '%s' '%s' (len=%d)\n",
                    prop->key, prop->value, (int) strlen(prop->value));
                abort();
            }
            assert(strlen(prop->value) < PROPERTY_VALUE_MAX);
            strcpy(valueBuf, prop->value);
            return true;
        }
    }

    //printf("Prop: get [%s] not found\n", key);
    return false;
}

/*
 * Set the value of a property, replacing it if it already exists.
 *
 * If "value" is NULL, the property is removed.
 *
 * If the property is immutable, this returns "false" without doing
 * anything.  (Not implemented.)
 */
static bool set_property(const char* key, const char* value)
{
	struct listnode *node;
	Property *prop;
    assert(key != NULL);
    //assert(value != NULL);

    list_for_each(node, &prop_list) {
        prop = node_to_item(node, Property, plist);
        if (strcmp(prop->key, key) == 0) {
            if (value != NULL) {
                //printf("Prop: replacing [%s]: [%s] with [%s]\n",
                 //   prop->key, prop->value, value);
                strcpy(prop->value, value);
            } else {
                //printf("Prop: removing [%s]\n", prop->key);
                list_remove(node);
				free(prop);
            }
            return true;
        }
    }

    //printf("Prop: adding [%s]: [%s]\n", key, value);
    Property *new = malloc(sizeof(Property));
    strcpy(new->key, key);
    strcpy(new->value, value);
	list_add_tail(&prop_list, &(new->plist));

    return true;
}
/*
static void write_peristent_property(const char *name, const char *value)
{
    const char *tempPath = PERSISTENT_PROPERTY_DIR "/.temp";
    char path[PATH_MAX];
    int fd, length;

    snprintf(path, sizeof(path), "%s/%s", PERSISTENT_PROPERTY_DIR, name);

    fd = open(tempPath, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
        ERROR("Unable to write persistent property to temp file %s errno: %d\n", tempPath, errno);
        return;   
    }
    write(fd, value, strlen(value));
    close(fd);

    if (rename(tempPath, path)) {
        unlink(tempPath);
        ERROR("Unable to rename persistent property file %s to %s\n", tempPath, path);
    }
}
*/

static bool create_list_file(const char* fileName)
{

    struct stat sb;
    bool result = false;
    FILE *fp = NULL;
    int cc;
	char lineBuf[PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX + 20];

    cc = stat(fileName, &sb);
    if (cc < 0) {
        if (errno != ENOENT) {
            printf(
                "Unable to stat '%s' (errno=%d)\n", fileName, errno);
            goto bail;
        }
    } else {
        /* don't touch it if it's not a socket */
        if (!(S_ISREG(sb.st_mode))) {
            printf(
                "File '%s' exists and is not a reg file\n", fileName);
            goto bail;
        }

        /* remove the cruft */
        if (unlink(fileName) < 0) {
            printf(
                "Unable to remove '%s' (errno=%d)\n", fileName, errno);
            goto bail;
        }
    }	

	fp = fopen(fileName, "w");

	struct listnode *node;
	Property *prop;

    list_for_each(node, &prop_list) {
        prop = node_to_item(node, Property, plist);
		memset(lineBuf, 0x00, sizeof(lineBuf));
		sprintf(lineBuf, "%s: [%s]\n", prop->key, prop->value);
		//printf("%s,%s\n", prop.key, prop.value);
        fputs(lineBuf, fp);
    }

    if (fp != NULL)
        fclose(fp);

	return true;

bail:
    if (fp != NULL)
        fclose(fp);
    return result;
}


/*
 * Create a UNIX domain socket, carefully removing it if it already
 * exists.
 */
static int create_property_socket(const char* fileName)
{
    struct stat sb;
    bool result = false;
    int sock = -1;
    int cc;

    cc = stat(fileName, &sb);
    if (cc < 0) {
        if (errno != ENOENT) {
            printf(
                "Unable to stat '%s' (errno=%d)\n", fileName, errno);
            goto bail;
        }
    } else {
        /* don't touch it if it's not a socket */
        if (!(S_ISSOCK(sb.st_mode))) {
            printf(
                "File '%s' exists and is not a socket\n", fileName);
            goto bail;
        }

        /* remove the cruft */
        if (unlink(fileName) < 0) {
            printf(
                "Unable to remove '%s' (errno=%d)\n", fileName, errno);
            goto bail;
        }
    }

    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(
            "UNIX domain socket create failed (errno=%d)\n", errno);
        goto bail;
    }

    /* bind the socket; this creates the file on disk */
    strcpy(addr.sun_path, fileName);    // max 108 bytes
    addr.sun_family = AF_UNIX;
    cc = bind(sock, (struct sockaddr*) &addr, SUN_LEN(&addr));
    if (cc < 0) {
        printf("AF_UNIX bind failed for '%s' (errno=%d)\n", fileName, errno);
        goto bail;
    }

    cc = listen(sock, 5);
    if (cc < 0) {
        printf("AF_UNIX listen failed (errno=%d)\n", errno);
        goto bail;
    }

    result = sock;
    sock = -1;
    return result;

bail:
    if (sock >= 0)
        close(sock);
    return result;
}

/*
 * Handle a client request.
 *
 * Returns true on success, false if the fd should be closed.
 */
static bool handle_request(int fd)
{
    char reqBuf[PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX];
    char valueBuf[1 + PROPERTY_VALUE_MAX];
    ssize_t actual;

    memset(valueBuf, 'x', sizeof(valueBuf));        // placate valgrind

    /* read the command byte; this determines the message length */
    actual = read(fd, reqBuf, 1);
    if (actual <= 0)
        return false;

    if (reqBuf[0] == kSystemPropertyGet) {
        actual = read(fd, reqBuf, PROPERTY_KEY_MAX);

        if (actual != PROPERTY_KEY_MAX) {
            fprintf(stderr, "Bad read on get: %d of %d\n",
                (int) actual, PROPERTY_KEY_MAX);
            return false;
        }
        if (get_property(reqBuf, valueBuf+1))
            valueBuf[0] = 1;
        else
            valueBuf[0] = 0;
        //printf("GET property [%s]: (found=%d) [%s]\n",
        //    reqBuf, valueBuf[0], valueBuf+1);
        if (write(fd, valueBuf, sizeof(valueBuf)) != sizeof(valueBuf)) {
            fprintf(stderr, "Bad write on get\n");
            return false;
        }
    } else if (reqBuf[0] == kSystemPropertySet) {
        actual = read(fd, reqBuf, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX);
        if (actual != PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX) {
            fprintf(stderr, "Bad read on set: %d of %d\n",
                (int) actual, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX);
            return false;
        }
        //printf("SET property '%s'\n", reqBuf);
        if(memcmp(reqBuf,"ctl.",4) == 0) {
            handle_control_message(reqBuf+4, reqBuf + PROPERTY_KEY_MAX);
			valueBuf[0] = 1;
		} else {
        	if (set_property(reqBuf, reqBuf + PROPERTY_KEY_MAX))
            	valueBuf[0] = 1;
        	else
            	valueBuf[0] = 0;
		}
        if (write(fd, valueBuf, 1) != 1) {
            fprintf(stderr, "Bad write on set\n");
            return false;
        }
    } else if (reqBuf[0] == kSystemPropertyList) {
        /* TODO */
        //assert(false);
		create_list_file(SYSTEM_PROPERTY_LIST_NAME);
        valueBuf[0] = 1;
        if (write(fd, valueBuf, 1) != 1) {
            fprintf(stderr, "Bad write on set\n");
            return false;
        }			
    } else {
        fprintf(stderr, "Unexpected request %d from prop client\n", reqBuf[0]);
        return false;
    }

    return true;
}


void handle_property_set_fd(int listen_sock)
{
            struct sockaddr_un from;
            socklen_t fromlen;
            int newSock;
			int fd;

            fromlen = sizeof(from);
            newSock = accept(listen_sock, (struct sockaddr*) &from, &fromlen);
            if (newSock < 0) {
                printf(
                    "AF_UNIX accept failed (errno=%d)\n", errno);
            } else {
                //printf("new props connection on %d --> %d\n",
                //    mListenSock, newSock);

				fd = newSock;
            }


            bool ok = true;


            ok = handle_request(fd);

            if (ok) {

            } else {
                //printf("--- closing %d\n", fd);
                close(fd);
            }
}

#if 0
static void load_properties(char *data)
{
    char *key, *value, *eol, *sol, *tmp;

    sol = data;
    while((eol = strchr(sol, '\n'))) {
        key = sol;
        *eol++ = 0;
        sol = eol;

        value = strchr(key, '=');
        if(value == 0) continue;
        *value++ = 0;

        while(isspace(*key)) key++;
        if(*key == '#') continue;
        tmp = value - 2;
        while((tmp > key) && isspace(*tmp)) *tmp-- = 0;

        while(isspace(*value)) value++;
        tmp = eol - 2;
        while((tmp > value) && isspace(*tmp)) *tmp-- = 0;

        property_set(key, value);
    }
}

static void load_properties_from_file(const char *fn)
{
    char *data;
    unsigned sz;

    data = read_file(fn, &sz);

    if(data != 0) {
        load_properties(data);
        free(data);
    }
}

static void load_persistent_properties()
{
    DIR* dir = opendir(PERSISTENT_PROPERTY_DIR);
    struct dirent*  entry;
    char path[PATH_MAX];
    char value[PROP_VALUE_MAX];
    int fd, length;

    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp("persist.", entry->d_name, strlen("persist.")))
                continue;
#if HAVE_DIRENT_D_TYPE
            if (entry->d_type != DT_REG)
                continue;
#endif
            /* open the file and read the property value */
            snprintf(path, sizeof(path), "%s/%s", PERSISTENT_PROPERTY_DIR, entry->d_name);
            fd = open(path, O_RDONLY);
            if (fd >= 0) {
                length = read(fd, value, sizeof(value) - 1);
                if (length >= 0) {
                    value[length] = 0;
                    set_property(entry->d_name, value);
                } else {
                    printf("Unable to read persistent property file %s errno: %d\n", path, errno);
                }
                close(fd);
            } else {
                printf"Unable to open persistent property file %s errno: %d\n", path, errno);
            }
        }
        closedir(dir);
    } else {
        printf("Unable to open persistent property directory %s errno: %d\n", PERSISTENT_PROPERTY_DIR, errno);
    }
    
    persistent_properties_loaded = 1;
}
#endif

void property_init(void)
{
    set_default_properties();
	//load_properties_from_file(PROP_PATH_SYSTEM_DEFAULT);
}

int start_property_service(void)
{
    int fd = create_property_socket(SYSTEM_PROPERTY_PIPE_NAME);
    /* Read persistent properties after all default values have been loaded. */
    //load_persistent_properties();

    if(fd < 0) return -1;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    fcntl(fd, F_SETFL, O_NONBLOCK);


    return fd;
}

int property_set(const char *key, const char *value)
{
	set_property(key, value);
}
