package com.cql.persistentservicedemo;

public class NativeObserverProcess {
	static {
		System.loadLibrary("nativeObserver");
		native_setup("com.cql.persistentservicedemo",
				"com.cql.persistentservicedemo.PersistentService");
	}
	
	public native static int 		isNativeObserverProcessExisted();
	public native static int		createNativeObserverProcess(String packageName,String service);
	public native static int		tellNativeObserverSerivceRestarted();
	public native static void		native_setup(String packageName,String service);
}
