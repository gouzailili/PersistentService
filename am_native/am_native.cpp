#define LOG_TAG "am_native"

#include <cutils/log.h>
#include <unistd.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <utils/String8.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace android;

static char *console_name = "/dev/console";

static void open_console()
{
    int fd;
    if ((fd = open(console_name, O_RDWR)) < 0) {
        fd = open("/dev/null", O_RDWR);
    }
    ioctl(fd, TIOCSCTTY, 0);
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
}

/*

    public ComponentName startService(IApplicationThread caller, Intent service,
            String resolvedType, int userId) throws RemoteException
    {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeInterfaceToken(IActivityManager.descriptor);
        data.writeStrongBinder(caller != null ? caller.asBinder() : null);
        service.writeToParcel(data, 0);
        data.writeString(resolvedType);
        data.writeInt(userId);
        mRemote.transact(START_SERVICE_TRANSACTION, data, reply, 0);
        reply.readException();
        ComponentName res = ComponentName.readFromParcel(reply);
        data.recycle();
        reply.recycle();
        return res;
    }
*/

int START_SERVICE_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION + 33;

int main(int argsn,char **argsv)
{
    char args[300] = {0};
    char* P = args;
    
    for (int i = 0;i<argsn;i++) {
        sprintf(P,"%s ",argsv[i]);
        P += strlen(P);
    }
    ALOGD("am_native: %s\n",args);
    
    if ( argsn != 3 ) {
        ALOGD("am_native args count = %d,error",argsn);
        exit(0);
    }

    String16 packageName(argsv[1]);
    String16 serviceName(argsv[2]);
    
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> am = sm->getService(String16("activity"));
    if (am != NULL) {
        ALOGD("get activity service in native success");
        Parcel data, reply;
        data.writeInterfaceToken(String16("android.app.IActivityManager"));
        data.writeStrongBinder(NULL);

        //write service data
        //write action
        data.writeString16(NULL, 0);
        //write Uri
        data.writeInt32(0);
        //write type
        data.writeString16(NULL, 0);
        //write flag
        data.writeInt32(0);
        //write package
        data.writeString16(NULL, 0);
        //write component 
        data.writeString16(packageName);
        data.writeString16(serviceName);

        data.writeInt32(0);
        data.writeInt32(0);
        data.writeInt32(0);
        data.writeInt32(0);
        data.writeInt32(-1);

        data.writeString16(NULL, 0);
        data.writeInt32(0);

        status_t ret = am->transact(START_SERVICE_TRANSACTION, data, &reply);
    }
    
    return 0;
    
}

int amSendBroadCast()
{
    return 0;
}
