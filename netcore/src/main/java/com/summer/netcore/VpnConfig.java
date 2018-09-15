package com.summer.netcore;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.util.*;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import java.util.regex.Pattern;

/**
 * Created by summer on 04/09/2018.
 */

public class VpnConfig {

    private static final String TAG = "VpnConfig";

    public static final class CTRL_BITS {
        public static final byte BASE = 1<<0;
        public static final byte PROXY = 1<<1;
        public static final byte BLOCK = 1<<2;
        public static final byte CAPTURE = 1<<3;
    }

    public enum  AVAIL_CTRLS{
        BASE(CTRL_BITS.BASE, "None"),
        PROXY(CTRL_BITS.BASE| CTRL_BITS.PROXY, "Proxy"),
        CAPTURE(CTRL_BITS.BASE| CTRL_BITS.CAPTURE, "Capture"),
        PROXY_CAPTURE(CTRL_BITS.BASE| CTRL_BITS.PROXY | CTRL_BITS.CAPTURE, "Pxy&Cap");

        AVAIL_CTRLS(int val, String desc){
            this.ctrls = val;
            this.name = desc;
        }

        public static AVAIL_CTRLS valueOf(int i){
            for(AVAIL_CTRLS c:AVAIL_CTRLS.values()){
                if(c.ctrls == i){
                    return c;
                }
            }
            return null;
        }

        public int ctrls;
        public String name;
    }
    public enum CtrlType{
        APP, DOMAIN, IP
    }

    public static final class S5Verify{
        public static final int NONE = 0xFFFFFF00;
        public static final int PWD = 0xFFFF02FF;
    }

    private static List<WeakReference<IListener>> sListeners = new ArrayList<>();

    private static boolean sInited = false;

    private static SharedPreferences sSP;

    private static final SparseArray<Setting> SETTINGS = new SparseArray<>();
    private static final Set<Integer> CORE_SETTINGS = new TreeSet<>();

    static {
        CORE_SETTINGS.add(Config.LOG_LEVEL);
        CORE_SETTINGS.add(Config.PROXY_ADDR);
        CORE_SETTINGS.add(Config.PROXY_IPVER);
        CORE_SETTINGS.add(Config.PROXY_PORT);
        CORE_SETTINGS.add(Config.SOCKS5_PASSWORD);
        CORE_SETTINGS.add(Config.SOCKS5_USERNAME);
        CORE_SETTINGS.add(Config.SOCKS5_VERIFY);
        CORE_SETTINGS.add(Config.PROXY_DNS_SERVER);
        CORE_SETTINGS.add(Config.CAP_OUTPUT_DIR);
    }

    static void addSetting(Setting setting){
        SETTINGS.put(setting.id, setting);
    }

    private static Map<Integer, String> sConfigs = new HashMap<>();
    private static Map<String,Integer> sPackageCtrls = new LinkedHashMap<>();
    private static Map<String,Integer> sDomainCtrls = new LinkedHashMap<>();
    private static Map<String,Integer> sIPCtrls = new LinkedHashMap<>();


    public static void init(Context context){
        if(!sInited){
            initDefaultCaputurePath(context);


            sSP = context.getSharedPreferences("vpn_core_config", Context.MODE_PRIVATE);

            BackgroundThread.get().schedule(new LoadConfigTask());

            sInited = true;
        }
    }

    private static void initDefaultCaputurePath(Context context){
        File files = context.getExternalFilesDir(null);
        if(files == null){
            android.util.Log.e(TAG,"init default capture output directory failed, external files dir is null");
            return;
        }
        String filesPath = files.getAbsolutePath();
        File dir = new File(filesPath + (filesPath.endsWith(Pattern.quote(File.separator))?"":File.separator) + "captures");
        if(!dir.exists()){
            try{
                if(!dir.mkdirs()){
                    android.util.Log.e(TAG,"init default capture output directory failed");
                }
            }catch (Throwable t){
                t.printStackTrace();
                android.util.Log.e(TAG,"init default capture output directory failed");
            }
        }

        String dirPath = dir.getAbsolutePath();
        if(!dirPath.endsWith(Pattern.quote(File.separator))){
            dirPath += File.separator;
        }
        SETTINGS.get(Config.CAP_OUTPUT_DIR).defaultvalue = dirPath;
    }

    public static boolean isValidateHost(String host){
        if(Patterns.WEB_URL.matcher(host).matches()){
            if(host.contains(":") || host.contains(";")){
                return false;
            }
            return true;
        }

        return false;
    }

    public static synchronized void addListener(IListener l){
        sListeners.add(new WeakReference<>(l));
    }

    public static synchronized void removeListener(IListener l){
        List<WeakReference<IListener>> dels = new ArrayList<>();
        for(WeakReference<IListener> wrfl:sListeners){
            if(wrfl.get() == null || wrfl.get() == l){
                dels.add(wrfl);
            }
        }
        sListeners.removeAll(dels);
    }

    public static synchronized String getConfig(int key, String def){
        if(sConfigs.containsKey(key)){
            return sConfigs.get(key);
        }

        return def;
    }

    public static synchronized void setConfig(int key, String value){
        String oval = sConfigs.get(key);
        if(value != null && value.equals(oval)){
            return;
        }

        sConfigs.put(key, value);

        BackgroundThread.get().schedule(new SaveConfigTask(key, value));
    }

    public static synchronized List<Pair<String,AVAIL_CTRLS>> getCtrls(CtrlType type, int ctrl){
        if(type == null){
            return null;
        }

        Map<String, Integer> ctrls = null;
        switch (type){
            case APP: ctrls = sPackageCtrls;  break;
            case DOMAIN: ctrls = sDomainCtrls;  break;
            case IP: ctrls = sIPCtrls; break;
        }

        if(ctrls != null){
            List<Pair<String,AVAIL_CTRLS>> r =  new ArrayList<>();
            Iterator<Map.Entry<String, Integer>> itr = ctrls.entrySet().iterator();
            while(itr.hasNext()) {
                Map.Entry<String, Integer> entry = itr.next();
                if((entry.getValue()&ctrl) == ctrl){
                    r.add(new Pair<>(entry.getKey(), AVAIL_CTRLS.valueOf(entry.getValue())));
                }
            }
            return r;
        }

        return null;
    }

    public static synchronized int getCtrl(CtrlType type, String target){
        if(type == null || target == null){
            return 0;
        }

        Map<String, Integer> ctrls = null;
        switch (type){
            case APP: ctrls = sPackageCtrls; break;
            case DOMAIN: ctrls = sDomainCtrls; break;
            case IP: ctrls = sIPCtrls; break;
        }

        Integer o = ctrls.get(target);
        return o==null?0:o;
    }

    public static synchronized void updateCtrl(CtrlType type, String target, AVAIL_CTRLS ctrl){

        if(type == null || target == null){
            return;
        }

        int key = 0;
        Map<String, Integer> ctrls = null;
        switch (type){
            case APP: ctrls = sPackageCtrls; key = Config.CTRL_PACKAGES; break;
            case DOMAIN: ctrls = sDomainCtrls; key = Config.CTRL_DOMAIN; break;
            case IP: ctrls = sIPCtrls; key = Config.CTRL_IP; break;
        }

        if(ctrls != null){
            if (ctrls.containsKey(target)) {
                if(ctrl == null){
                    ctrls.remove(target);
                    BackgroundThread.get().schedule(new SaveConfigTask(key, serializeCtrls(ctrls)));
                }else{
                    int o = ctrls.get(target);
                    if(o != ctrl.ctrls){
                        ctrls.put(target, ctrl.ctrls);
                        BackgroundThread.get().schedule(new SaveConfigTask(key, serializeCtrls(ctrls)));
                    }
                }

            }else if(ctrl != null){
                ctrls.put(target, ctrl.ctrls);
                BackgroundThread.get().schedule(new SaveConfigTask(key, serializeCtrls(ctrls)));
            }
        }


    }

    private static void parseCtrls(Map<String, Integer> result, String value){
        if(value == null || "".equals(value)){
            return;
        }

        String[] kvs = value.split(Pattern.quote(";"));
        if(kvs != null){
            for(String s:kvs){
                String[] kv = s.split(Pattern.quote(":"));
                if(kv != null && kv.length >= 2){
                    try {
                        result.put(kv[0], Integer.valueOf(kv[1]));
                    }catch (Throwable t){
                        t.printStackTrace();
                    }

                }
            }
        }
    }


    private static String serializeCtrls(Map<String, Integer> ctrls){
        StringBuilder sb = new StringBuilder();
        Iterator<Map.Entry<String, Integer>> itr = ctrls.entrySet().iterator();
        while(itr.hasNext()){
            Map.Entry<String, Integer> entry = itr.next();
            sb.append(entry.getKey()).append(":").append(entry.getValue()).append(";");
        }

        return sb.toString();
    }

    private static boolean isCoreConfig(int key){
        return CORE_SETTINGS.contains(key);
    }

    private static void onConfigSaveSucc(int key, String value){
        if(isCoreConfig(key)){
            VpnCore.setShellConfig(key, value);
        }

        List<IListener> ls = new ArrayList<>();
        List<WeakReference<IListener>> del = new ArrayList<>();
        for(WeakReference<IListener> wrl: sListeners){
            if(wrl != null && wrl.get() != null){
                ls.add(wrl.get());
            }else{
                del.add(wrl);
            }
        }

        sListeners.removeAll(del);

        for(IListener l:ls){
            l.onVpnConfigItemUpdated(key, value);
        }
    }

    private static void onConfigLoadSucc(Map<Integer, String> configs){
        sPackageCtrls.clear();
        sDomainCtrls.clear();
        sIPCtrls.clear();
        parseCtrls(sPackageCtrls, configs.get(Config.CTRL_PACKAGES));
        parseCtrls(sDomainCtrls, configs.get(Config.CTRL_DOMAIN));
        parseCtrls(sIPCtrls, configs.get(Config.CTRL_IP));

        for(int key:CORE_SETTINGS){
            VpnCore.setShellConfig(key, configs.get(key));
        }

        List<IListener> ls = new ArrayList<>();
        List<WeakReference<IListener>> del = new ArrayList<>();
        for(WeakReference<IListener> wrl: sListeners){
            if(wrl != null && wrl.get() != null){
                ls.add(wrl.get());
            }else{
                del.add(wrl);
            }
        }

        sListeners.removeAll(del);

        for(IListener l:ls){
            l.onVpnConfigLoaded();
        }
    }

    private static class SaveConfigTask implements Runnable{
        private int key;
        private String value;

        private SaveConfigTask(int key, String value){
            this.key = key;
            this.value = value;
        }

        @Override
        public void run() {
            SharedPreferences.Editor editor = sSP.edit();
            editor.putString(String.valueOf(key), value);
            editor.commit();

            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    onConfigSaveSucc(key, value);
                }
            });
        }
    }

    private static class LoadConfigTask implements Runnable{

        private LoadConfigTask(){

        }

        @Override
        public void run() {
            sConfigs.clear();

            for(int i=0;i<SETTINGS.size();i++){
                int key = SETTINGS.keyAt(i);
                Setting s = SETTINGS.get(key);
                sConfigs.put(key, sSP.getString(String.valueOf(key), s.defaultvalue));
            }

            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    onConfigLoadSucc(sConfigs);
                }
            });
        }
    }


    static class  Setting {

        Setting(int id){
            this(id, "");
        }

        Setting(int id, String defaultvalue){
            this.id = id;
            this.defaultvalue = defaultvalue;
        }

        public final int id;
        public String defaultvalue;
    }

    public interface IListener{
        void onVpnConfigLoaded();
        void onVpnConfigItemUpdated(int key, String value);
    }

}
