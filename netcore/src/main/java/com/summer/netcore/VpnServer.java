package com.summer.netcore;

import android.app.Notification;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.VpnService;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Pair;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by summer on 16/03/2018.
 */

public class VpnServer extends VpnService implements Runnable{
    private static String TAG = "VpnCore.VpnServer";

    public static String ACT_START = "START";
    public static String ACT_STOP = "STOP";

    private String mLocalIP = "192.168.0.100";

    private Thread mThread;
    public static boolean sRunning = false;

    private ParcelFileDescriptor mFd;

    @Override
    public void onCreate() {
        super.onCreate();

        if(NetCoreIface.getForegroundNotifycation() == null){
            stopSelf();
        }

    }



    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        VpnCore.init(getApplicationContext());
        if(intent != null){
            if(ACT_START.equals(intent.getAction())){
                sRunning = true;
                int notificationID = NetCoreIface.getForegroundNotificationId();
                Notification notification = NetCoreIface.getForegroundNotifycation();
                startForeground(notificationID, notification);
                if(mThread == null){
                    mThread = new Thread(this);
                    mThread.start();
                }
            }else if(ACT_STOP.equals(intent.getAction())){
                VpnCore.terminate();
                stop();
            }
        }


        return START_STICKY;
    }

    private List<String> packagesUnderCtrl(){
        List<String> r = new ArrayList<>();
        PackageManager pm = getApplicationContext().getPackageManager();
        List<Pair<String, VpnConfig.AVAIL_CTRLS>> ctrls = VpnConfig.getCtrls(VpnConfig.CtrlType.APP, VpnConfig.CTRL_BITS.BASE);
        for(Pair<String, VpnConfig.AVAIL_CTRLS> u:ctrls){
            try {
                int uid = Integer.valueOf(u.first);
                String[] pkgs = pm.getPackagesForUid(uid);
                if(pkgs != null && pkgs.length > 0 && pkgs != null && !"".equals(pkgs[0])){
                    r.add(pkgs[0]);
                }
            }catch (Throwable t){
                t.printStackTrace();
            }
        }

        return r;
    }

    private void deleteExceededCap(){
        String dir = VpnConfig.getConfig(Config.CAP_OUTPUT_DIR, null);
        if(dir != null){
            File fDir = new File(dir);
            if(fDir.isDirectory()){
                File[] recs = fDir.listFiles();
                if(recs != null && recs.length > 5){
                    recs[0].delete();
                    recs[1].delete();
                }
            }
        }

    }

    @Override
    public void run() {
        deleteExceededCap();

        Builder builder = new Builder()
                .setSession("VPNService")
                .addAddress(mLocalIP, 24)
                .addRoute("0.0.0.0", 0);

        String dns = VpnConfig.getConfig(Config.DNS_SERVER, "");
        if(IPUtils.isIpv4Address(dns) || IPUtils.isIpv6Address(dns)){
            try {
                builder.addDnsServer(dns);
            }catch (Throwable t){
                t.printStackTrace();
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                List<String> pkgsUnderCtrl = packagesUnderCtrl();
                if(pkgsUnderCtrl != null){
                    for(String pkg:pkgsUnderCtrl){
                        builder.addAllowedApplication(pkg);
                    }
                }
            } catch (PackageManager.NameNotFoundException e) {
                e.printStackTrace();
            }
        }

        mFd = builder.establish();

        Log.d(TAG,"vpn start...");
        int ret = VpnCore.start(this,mFd.getFd());

        Log.d(TAG,"vpn stopping, ret " + ret);
        mThread = null;
    }

    private void stop(){
        if(mFd != null){
            try {
                mFd.close();
                mFd = null;
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        sRunning = false;
        stopForeground(false);
//        stopSelf();

    }



}
