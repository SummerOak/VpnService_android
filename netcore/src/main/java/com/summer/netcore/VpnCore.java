package com.summer.netcore;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.VpnService;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import java.lang.annotation.Native;
import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by summer on 15/03/2018.
 */

class VpnCore {
    private static final String TAG = "VpnCore";

    private static VpnService sVpnSvr = null;
    private static WeakReference<NetCoreIface.IListener> sListener = null;
    private static MyHandler sH;
    private static ObjectPool<MsgData> sMsgDataPool;

    private static boolean sInited = false;

    static int init(Context context){
        if(sInited){
            return 0;
        }

        if(!loadLib() || !initMessege()){
            return 1;
        }

        VpnConfig.init(context);

        sInited = true;
        return 0;
    }

    private static boolean loadLib(){
        try{
            System.loadLibrary("vpncore");
            return true;
        }catch (Throwable t){
            t.printStackTrace();
        }
        return false;
    }

    private static boolean initMessege(){
        sH = new MyHandler(Looper.getMainLooper());
        sMsgDataPool = new ObjectPool<>(new ObjectPool.IConstructor<MsgData>() {
            @Override
            public MsgData newInstance(Object... params) {
                MsgData e = new MsgData();
                initialize(e, params);
                return e;
            }

            @Override
            public void initialize(MsgData e, Object... params) {
                e.id        = (int)params[0];
                e.uid       = (int)params[1];
                e.protocol  = (byte)params[2];
                e.state     = (byte)params[3];
                e.accept    = (long)params[4];
                e.back      = (long)params[5];
                e.sent      = (long)params[6];
                e.recv      = (long)params[7];
                e.size      = (int)params[8];
                e.flag      = (int)params[9];
                e.seq       = (int)params[10];
                e.ack       = (int)params[11];
                e.dest      = (String)params[12];
                e.destPort  = (int)params[13];
            }

        }, 10);
        return true;
    }

    static void setListener(NetCoreIface.IListener l){
        sListener = new WeakReference<>(l);
    }

    static int start(VpnService svr,int tun){
        sVpnSvr = svr;

        if(sListener != null && sListener.get() != null){
            sH.obtainMessage(Event.VPN_START).sendToTarget();
        }

        int ret = start(tun);// block here until terminate be called or something wrong happened

        if(ret == 0 && sListener != null && sListener.get() != null){
            sH.obtainMessage(Event.VPN_STOP).sendToTarget();
        }

        return ret;
    }

    static int terminate(){
        sVpnSvr = null;
        int ret = stop();

        return ret;
    }

    static void setShellConfig(int key, String value){
        if(value != null && !"".equals(value)){
            setConfig(key,value);
        }
    }

    /**
     * set configs, all configurations must be init before invocation of start
     * @param key
     * @param value
     * @return
     */
    private static native int setConfig(int key,String value);


    private static native String getSystemProperty(String key);

    /**
     * this invocation will never return until a fatal error occur or stop invoked.
     * @param tun
     * @return
     */
    private static native int start(int tun);


    /**
     * stop
     * @return the result code of this operation
     */
    private static native int stop();

    /**
     * do module test
     * @return 0 if all success otherwise 1
     */
    public static native int moduleTest();

    public static int protect(int socket){
        return sVpnSvr!=null&&sVpnSvr.protect(socket)?0:1;
    }

    /**
     *  The callback of connections info from native;
     *  Maybe it is too many arguments in here! But since this method will be called in a high frequency,
     *  in order to minimize the calls from native to java, we put all related arguments together.
     *
     * @param event the event id
     * @param id    the connection id
     * @param uid   user id of the owner of this connection
     * @param protocol the protocol of this connection
     * @param state the connection state, different protocol has different set of states;
     * @param accept the total accept bytes of this connection;
     * @param back  total bytes feedback to client
     * @param sent  total bytes this connection sent out
     * @param recv  total bytes this connection received from remote
     * @param size  the bytes of this package
     * @param flag  different protocols has different meanings; For tcp, it is represent the TCP flag in TCP header;
     * @return  Not used currently;
     */
    public static int onConnInfo(int event,int id,int uid, byte protocol,byte state,
                                 long accept,long back, long sent, long recv,
                                 int size, int flag,
                                 int seq, int ack, String dest, int destPort
                                ){
        Log.d(TAG,"onConnInfo: event " + event + " id " + id + " uid " + uid + " protocol " + protocol + " state " + state
                + " accept " + accept + " back " + back + " sent " + sent + " recv " + recv);


        if(sH != null){
            sH.obtainMessage(event,sMsgDataPool.obtain(id,uid, protocol,state,accept,back,sent,recv, size, flag, seq, ack, dest, destPort)).sendToTarget();
        }

        return 0;
    }


    public static String getAppName(int uid){
        if(sVpnSvr != null){
            PackageManager pm = sVpnSvr.getApplication().getPackageManager();
            String[] pkgs = pm.getPackagesForUid(uid);

            if(pkgs != null){
                for(String pkg:pkgs){
                    Log.d(TAG,"pkg: " + pkg);
                }

                return pkgs[0];
            }
        }

        return "";
    }

    /**
     * query what control strategy should take effect on connection with params below,
     *
     * @param uid
     * @param ipver
     * @param ip
     * @param port
     * @param protocol
     * @param domain
     * @return
     */
    public static int queryControlStrategy(int uid, int ipver, String ip, int port, byte protocol, String domain){

        int ctrl = VpnConfig.getCtrl(VpnConfig.CtrlType.APP, String.valueOf(uid));
        if((ctrl&(~VpnConfig.CTRL_BITS.BASE)) > 0){
            return  ctrl;
        }

        ctrl = VpnConfig.getCtrl(VpnConfig.CtrlType.IP, ip);
        if((ctrl&(~VpnConfig.CTRL_BITS.BASE)) > 0){
            return  ctrl;
        }

        if(domain != null && !"".equals(domain)){
            ctrl = VpnConfig.getCtrl(VpnConfig.CtrlType.DOMAIN, domain);
            if((ctrl&(~VpnConfig.CTRL_BITS.BASE)) > 0){
                return  ctrl;
            }
        }

        return VpnConfig.CTRL_BITS.BASE;
    }


    private static class Event{
        final static int VPN_START              = 1;
        final static int VPN_STOP               = 2;
        final static int CONN_BORN              = 1001;
        final static int CONN_DIE               = 1002;
        final static int CONN_TRAFFIC_ACCEPT    = 1003;
        final static int CONN_TRAFFIC_BACK      = 1004;
        final static int CONN_TRAFFIC_SENT      = 1005;
        final static int CONN_TRAFFIC_RECV      = 1006;
        final static int CONN_STATE             = 1007;
    }

    private static class MsgData{
        int id = 0;
        int uid = 0;
        byte protocol = 0;
        byte state = 0;
        long accept = 0, back = 0, sent = 0, recv = 0;
        int size;
        int flag;
        int seq = 0, ack = 0;
        String dest;
        int destPort;
    }



    private static class MyHandler extends Handler{

        MyHandler(Looper looper){
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            MsgData e = (MsgData)msg.obj;
            int w = msg.what;

            NetCoreIface.IListener l = sListener==null?null:sListener.get();
            if(l != null){
                switch (w){
                    case Event.VPN_START:{
                        l.onEnable();
                        break;
                    }
                    case Event.VPN_STOP:{
                        l.onDisable();
                        break;
                    }
                    case Event.CONN_BORN:{
                        l.onConnectCreate(e.id, e.uid, e.protocol, e.dest, e.destPort);
                        break;
                    }
                    case Event.CONN_DIE:{
                        l.onConnectDestroy(e.id,e.uid);
                        break;
                    }
                    case Event.CONN_TRAFFIC_ACCEPT:{
                        l.onTrafficAccept(e.id,e.size, e.accept, e.flag, e.seq, e.ack);
                        break;
                    }
                    case Event.CONN_TRAFFIC_BACK:{
                        l.onTrafficBack(e.id,e.size, e.back, e.flag, e.seq, e.ack);
                        break;
                    }
                    case Event.CONN_TRAFFIC_SENT:{
                        l.onTrafficSent(e.id,e.size, e.sent, e.flag);
                        break;
                    }
                    case Event.CONN_TRAFFIC_RECV:{
                        l.onTrafficRecv(e.id,e.size, e.recv, e.flag);
                        break;
                    }
                    case Event.CONN_STATE:{
                        l.onConnectState(e.id,e.state);
                        break;
                    }
                }
            }

            sMsgDataPool.recycle(e);

        }
    }


}
