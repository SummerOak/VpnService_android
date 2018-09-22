package com.summer.netcore;

import android.app.Activity;
import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

/**
 * Created by summer on 12/06/2018.
 */

public class NetCoreIface {
    private static final String TAG = "VpnCore.Iface";

    private static Notification sNotification;

    public static int sNotificationID = 1;

    private static boolean sInited = false;

    public static int init(Context context){
        int r = VpnCore.init(context);
        if (r == 0) {
            sInited = true;
        }

        return r;
    }

    public static boolean isServerRunning(){
        return VpnServer.sRunning;
    }

    public static void setListener(IListener l){
        VpnCore.setListener(l);
    }

    public static void setForgroundNotifycation(int id, Notification notifycation){
        sNotification = notifycation;
        sNotificationID = id;
    }

    public static int startVpn(Context context){

        if(!sInited){
            Log.e(TAG, "startVpn before init success, call init first.");
            return 1;
        }

        try{
            Intent intent = new Intent(context,DummyActivity.class);
            context.startActivity(intent);
        }catch (Throwable t){
            t.printStackTrace();
            return 1;
        }

        return 0;
    }

    public static int stopVpn(Context context){

        if(!sInited){
            return 0;
        }

        try {
            Intent intent = new Intent(context, VpnServer.class);
            intent.setAction(VpnServer.ACT_STOP);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }
        }catch (Throwable t){
            t.printStackTrace();
            return 1;
        }

        return 0;
    }

    static Notification getForegroundNotifycation(){
        return sNotification;
    }

    static int getForegroundNotificationId(){
        return sNotificationID;
    }

    public interface IListener{
        void onEnable();
        void onDisable();
        void onConnectCreate(int id,int uid,byte protocol, String dest, int destPort);
        void onConnectDestroy(int id, int uid);
        void onConnectState(int id, byte state);
        void onTrafficAccept(int id, int accept, long totalAccept, int flag, int seq, int ack);
        void onTrafficBack(int id, int back, long totalBack, int flag, int seq, int ack);
        void onTrafficSent(int id, int sent, long totalSent, int flag);
        void onTrafficRecv(int id, int recv, long totalRecv, int flag);
    }

}
