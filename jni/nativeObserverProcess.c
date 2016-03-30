#include <unistd.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define TAG "nativeObserver"

#define LOGV(TAG,...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,__VA_ARGS__)
#define LOGD(TAG,...) __android_log_print(ANDROID_LOG_DEBUG  , TAG,__VA_ARGS__)
#define LOGI(TAG,...) __android_log_print(ANDROID_LOG_INFO   , TAG,__VA_ARGS__)
#define LOGW(TAG,...) __android_log_print(ANDROID_LOG_WARN   , TAG,__VA_ARGS__)
#define LOGE(TAG,...) __android_log_print(ANDROID_LOG_ERROR  , TAG,__VA_ARGS__)

#define CLASS_PATH "com/cql/persistentservicedemo/NativeObserverProcess"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

static char gPackageName[256] = {0};
static char gServiceName[256] = {0};
static char gPackageDir[256] = {0};

static char gFifoPath[256] = {0};
static char gServerFifoPath[256] = {0};

static pthread_mutex_t gMutext = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gCondition = PTHREAD_COND_INITIALIZER;

static int waitServiceProcessDied() {
	int dummy[1];
	int fd = open(gFifoPath,O_RDONLY);

	if ( fd != -1 ) {
		LOGD(TAG,"open %s success,now wait service process died",gFifoPath);
		int ret = read(fd,dummy,1);
		close(fd);
		LOGD(TAG,"waitServiceProcessDied exit : %d",ret);
		return 0;
	} else {
		LOGD(TAG,"open %s failed,%s",gFifoPath,strerror(errno));
		return -1;
	}
}

static int reStartServiceProcess() {
	int ret = 0;
	pid_t pid = fork();
	if ( pid == 0 ) {
		char path[256] = {0};
		sprintf(path,"/data/data/%s/bin/am_native",gPackageName);
		LOGD(TAG,"%s %s/%s",path,gPackageName,gServiceName);

		execlp(path,
				path,
				gPackageName,
				gServiceName,
				(char*)0);
		LOGD(TAG,"execlp error: %s",strerror(errno));
		exit(errno);
	} else {
		LOGD(TAG,"wait child pid(%d) to restart service",pid);
		wait(&ret);
		LOGD(TAG,"child exit: ret = %d,%s",ret,strerror(ret));
	}
    return ret;
}

static int isServiceProcessStarted() {
	pthread_mutex_lock(&gMutext);

	struct timespec t;
	int ret = clock_gettime(CLOCK_REALTIME, &t);
	if ( ret == -1 ) {
		LOGD(TAG,"isServiceProcessStarted : %s",strerror(errno));
		return -1;
	}

	t.tv_sec += 20;
	LOGD(TAG,"isServiceProcessStarted : wait 20s for service restart");
	ret = pthread_cond_timedwait(&gCondition,&gMutext,&t);

	pthread_mutex_unlock(&gMutext);
	if ( ret == 0 ) {
		LOGD(TAG,"service process restart in 20s");
		return 0;
	} else {
		LOGD(TAG,"isServiceProcessStarted : %s",strerror(errno));
		return -1;
	}
}

static void createFifo() {
	umask(0);

	//create fifo_observerService to observer service status
	sprintf(gFifoPath,"%s/fifo_observerService",gPackageDir);
	if ( access(gFifoPath, X_OK) == 0 ) {
		LOGD(TAG,"createFifo %s has existed",gFifoPath);
		//remove(gFifoPath);
	} else {
		int ret = mkfifo(gFifoPath, S_IRWXU|S_IRWXG|S_IRWXO);
		if ( ret == -1 ) {
			LOGD(TAG,"createFifo %s failed: %s",gFifoPath,strerror(errno));
		} else {
			LOGD(TAG,"createFifo %s success",gFifoPath);
		}
	}

	//service process can send a msg to this fifo to get the status of observer status
	sprintf(gServerFifoPath,"%s/fifo_server",gPackageDir);
	if ( access(gServerFifoPath, X_OK) == 0 ) {
		LOGD(TAG,"createFifo %s has existed",gServerFifoPath);
	} else {
		int ret = mkfifo(gServerFifoPath, S_IRWXU|S_IRWXG|S_IRWXO);
		if ( ret == -1 ) {
			LOGD(TAG,"createFifo %s failed: %s",gServerFifoPath,strerror(errno));
		} else {
			LOGD(TAG,"createFifo %s success",gServerFifoPath);
		}
	}
}

static void notifyServiceProcessHasRestarted() {
	pthread_mutex_lock(&gMutext);
	LOGD(TAG,"notifyServiceProcessHasRestarted");
	pthread_cond_broadcast(&gCondition);
	pthread_mutex_unlock(&gMutext);

}

static void* thread_loop(){

	int dummy[1];

	LOGD(TAG,"ServerThread opening %s",gServerFifoPath);
	int serverfd = open(gServerFifoPath,O_RDONLY);
	if ( serverfd == -1 ) {
		LOGD(TAG,"thread_loop open %s O_RDONLY failed",gServerFifoPath);
		return;
	}

	int dummyFd = open(gServerFifoPath,O_WRONLY);
	if ( dummyFd == -1 ) {
		LOGD(TAG,"open %s O_WRONLY failed",gServerFifoPath);
		return;
	}

	while(1) {
		LOGD(TAG,"Waiting read from %s",gServerFifoPath);
		int ret = read(serverfd,dummy,sizeof(dummy));
		LOGD(TAG,"read :%d bytes ( value = %d) from  %s",ret,dummy[0],gServerFifoPath);

		switch( dummy[0] ) {
		case 1:{
			notifyServiceProcessHasRestarted();
		}break;

		default:{

		}break;

		}
	}

	LOGD(TAG,"ServerThread exit");

}

static void createServerThread() {
	LOGD(TAG,"createServerThread");
	pthread_t thread;
	int ret = pthread_create(&thread,NULL,thread_loop,"Server thread");
	if ( ret == -1 ) {
		LOGD(TAG,"create server thread failed: %s",strerror(errno));
	}
}

static int createObserverProcess(const char*packageName,const char* serviceName) {
	int ret = -1;
	if ( packageName != NULL ) {
		sprintf(gPackageDir,"/data/data/%s",packageName);
		ret = chmod(gPackageDir, 0777);
		LOGD(TAG,"chmod directory: %s 0777,ret = %d",gPackageDir,ret);

		if ( ret == 0 ) {
			pid_t observerPID = fork();
			if ( observerPID == 0 ) {
				LOGD(TAG,"createObserverProcess  pid = %d",getpid());

				ret = setsid();
				chdir("/");

				createServerThread();

				while( waitServiceProcessDied() == 0 ) {
					do {
						reStartServiceProcess();
					} while ( isServiceProcessStarted() != 0 );
				}
			} else if ( observerPID >= 0 ) {
				int fd = open(gFifoPath,O_WRONLY);
				if ( fd != -1 ) {
					LOGD(TAG,"Service process open fifo success");
				} else {
					LOGD(TAG,"Service process open fifo failed: %s",strerror(errno));
				}
			} else {
				LOGD(TAG,"createObserverProcess fork child failed");
			}
		} else {

		}
	} else {

	}
	return ret;
}


static int isNativeObserverProcessExisted(JNIEnv *env, jobject thiz) {
	int ret = -1;
	int dummy[1]={1};

	LOGD(TAG,"isNativeObserverProcessExisted opening :%s",gServerFifoPath);
	int serverFd = open(gServerFifoPath,O_WRONLY|O_NONBLOCK);
	if ( serverFd == -1 ) {
		LOGD(TAG,"isNativeObserverProcessExisted open %s O_WRONLY failed,%s",
				gServerFifoPath,strerror(errno));
		return -1;
	}

	close(serverFd);
	LOGD(TAG,"NativeObserverProcessExisted true");
	return 0;
}

static int createNativeObserverProcess(JNIEnv *env,
		jobject thiz,
		jstring packageName,
		jstring serviceName) {
	int ret = 0;
	LOGD(TAG,"createNativeObserverProcess package name: %s serviceName : %s",gPackageName,gServiceName);
	ret = createObserverProcess(gPackageName,gServiceName);
	LOGD(TAG,"createNativeObserverProcess ret = %d",ret);
	return ret;
}

static int tellNativeObserverSerivceRestarted(JNIEnv *env, jobject thiz) {

	// first we should notify native observer process,we restarted
	int ret = -1;
	int dummy[1]={1};

	LOGD(TAG,"tellNativeObserverSerivceRestarted opening :%s",gServerFifoPath);
	int serverFd = open(gServerFifoPath,O_WRONLY);
	if ( serverFd == -1 ) {
		LOGD(TAG,"tellNativeObserverSerivceRestarted open %s O_WRONLY failed,%s",
				gServerFifoPath,strerror(errno));
		return -1;
	}

	LOGD(TAG,"tellNativeObserverSerivceRestarted write :%s",gServerFifoPath);
	if ( write(serverFd,dummy,sizeof(dummy)) != sizeof(dummy) ) {
		LOGD(TAG,"isNativeObserverProcessExisted write %s failed,%s",
				gServerFifoPath,strerror(errno));
		return -1;
	}

	close(serverFd);

	//open this fifo,so that observer process can listen our died message
	int fd = open(gFifoPath,O_WRONLY);
	if ( fd != -1 ) {
		LOGD(TAG,"tellNativeObserverSerivceRestarted open fifo success");
	} else {
		LOGD(TAG,"tellNativeObserverSerivceRestarted open fifo failed: %s",strerror(errno));
	}
}

static void native_setup(JNIEnv *env,
		jobject thiz,
		jstring packageName,
		jstring serviceName ) {


	LOGD(TAG,"native_setup called");
	const char *packageUtf = (*env)->GetStringUTFChars(env,packageName,NULL);
	if ( packageUtf != NULL ) {
		memset(gPackageName,0x0,NELEM(gPackageName));
		strncpy(gPackageName,packageUtf,strlen(packageUtf));
		(*env)->ReleaseStringUTFChars(env,packageName,packageUtf);
	} else {
		return;
	}

	const char *utfStr = (*env)->GetStringUTFChars(env,serviceName,NULL);
	if ( utfStr != NULL ) {
		memset(gServiceName,0x0,NELEM(gServiceName));
		memcpy(gServiceName,utfStr,strlen(utfStr));
		(*env)->ReleaseStringUTFChars(env,serviceName,utfStr);
	} else {
		return;
	}

	sprintf(gPackageDir,"/data/data/%s",gPackageName);
	int ret = chmod(gPackageDir, 0777);
	LOGD(TAG,"native_setup chmod directory: %s 0777,ret = %d",gPackageDir,ret);

	createFifo();
}

static JNINativeMethod sMethods[] = {
    { "isNativeObserverProcessExisted","()I",(void *)isNativeObserverProcessExisted },
    { "createNativeObserverProcess","(Ljava/lang/String;Ljava/lang/String;)I",(void *)createNativeObserverProcess },
    { "tellNativeObserverSerivceRestarted","()I",(void *)tellNativeObserverSerivceRestarted },
    { "native_setup","(Ljava/lang/String;Ljava/lang/String;)V",(void *)native_setup },

};


 jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if ((*vm)->GetEnv(vm,(void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    	LOGD(TAG,"JNI_OnLoad failed");
        goto bail;
    }

    jclass clazz = (*env)->FindClass(env,CLASS_PATH);
    LOGD(TAG,"JNI_OnLoad find %s",CLASS_PATH);

    (*env)->RegisterNatives(env,
    		clazz,
    		sMethods,
    		NELEM(sMethods));


    result = JNI_VERSION_1_4;
    LOGD(TAG,"JNI_OnLoad sucess");
bail:
    return result;
}
