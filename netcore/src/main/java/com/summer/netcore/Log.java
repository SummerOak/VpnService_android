package com.summer.netcore;

/**
 * Created by summer on 13/06/2018.
 */

class Log {

    public static final int d(String tag,String log){
        if(Config.DEBUG_LEV != Config.DEBUG_LEV_RELEASE){
            return android.util.Log.d(tag,log);
        }

        return 0;
    }

    public static final int e(String tag,String log){
        return android.util.Log.e(tag,log);
    }

    public static final int i(String tag,String log){

        if(Config.DEBUG_LEV != Config.DEBUG_LEV_RELEASE){
            return android.util.Log.i(tag,log);
        }

        return 0;
    }

    public static final String getStackTraceString(Throwable t){
        return android.util.Log.getStackTraceString(t);
    }

}
