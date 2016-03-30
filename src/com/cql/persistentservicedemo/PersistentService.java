package com.cql.persistentservicedemo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.IBinder;
import android.util.Log;

public class PersistentService extends Service {

	public final static String TAG = "PersistentService";
	//public String mService = "com.cql.persistentservicedemo/com.cql.persistentservicedemo.PersistentService";
	
	@Override
	public IBinder onBind(Intent intent) {
		return null;
	}

	@Override
	public void onCreate() {
		Log.d(TAG,"Service onCreate");
		copyAssetFile2BinDir("am_native");
		
		if ( NativeObserverProcess.isNativeObserverProcessExisted() == 0 ) {
			Log.d(TAG,"NativeObserverProcess has existed");
			NativeObserverProcess.tellNativeObserverSerivceRestarted();
		} else {
			Log.d(TAG,"create native observer process");
			NativeObserverProcess.createNativeObserverProcess(this.getPackageName(),
					"com.cql.persistentservicedemo.PersistentService");
		}
	}
	
	@Override
	public void onDestroy() {
		Log.d(TAG, "Service onDestroy");
		this.startService(new Intent(this, PersistentService.class));
	}
	
	private void copyAssetFile2BinDir(String filename) {
		File dir = new File("/data/data/com.cql.persistentservicedemo/bin");
		if ( !dir.isDirectory() ) {
			dir.mkdir();
			dir.setExecutable(true,false);
			dir.setReadable(true, false);
			dir.setWritable(true, false);
			Log.d(TAG,"create dir: "+dir);
		}
		
		File file = new File(dir, filename);

		if ( file != null && !file.exists() ) {
			
			AssetManager am = this.getAssets();
			InputStream in = null;
			OutputStream out = null;
			
			try {
				file.createNewFile();
				file.setExecutable(true,false);
				file.setReadable(true, false);
				file.setWritable(true, false);
				
				in = am.open(filename);
				out = new FileOutputStream(file);
				while( in.available() > 0 ) {
					byte[] data = new byte[1024];
					int size = in.read(data);
					out.write(data,0,size);
				}
				
				Log.d(TAG,"create file: "+file);
				
			} catch (Exception e) {
				e.printStackTrace();
			} finally {
				if ( in != null ) {
					try {
						Log.d(TAG,"close in: "+in);
						in.close();
						in = null;
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
				
				if ( out != null ) {
					try {
						Log.d(TAG,"close out: "+out);
						out.close();
						out = null;
					} catch (IOException e) {
						e.printStackTrace();
					}
				}	
			}
		}
	}
	
}
