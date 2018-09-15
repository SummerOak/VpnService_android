package com.summer.netcore;

/**
 * Created by summer on 15/06/2018.
 */

public class Config {

    public static final byte DEBUG_LEV_RELEASE  = 0;
    public static final byte DEBUG_LEV_DEBUG    = 1;


    public static final byte DEBUG_LEV = DEBUG_LEV_RELEASE;

    /**
     * int typed value for log level
     */
    public static int LOG_LEVEL = 0;

    /**
     * int typed value for ipv4 address of proxy
     */
    public static int PROXY_ADDR = 1;

    /**
     * int typed value for port of proxy
     */
    public static int PROXY_PORT = 2;

    /**
     * int typed value for verify methods
     */
    public static int SOCKS5_VERIFY = 3;

    /**
     * string typed value for username of socks5 proxy
     */
    public static int SOCKS5_USERNAME = 4;

    /**
     * string typed value for password of socks5 proxy
     */
    public static int SOCKS5_PASSWORD = 5;

    /**
     * int typed value for proxy ip version
     */
    public static int PROXY_IPVER = 6;

    public static int PROXY_DNS_SERVER = 7;

    public static int CAP_OUTPUT_DIR = 8;

    public static int DNS_SERVER = 1001;

    public static int CTRL_PACKAGES = 1002;

    public static int CTRL_IP = 1003;

    public static int CTRL_DOMAIN = 1004;

    static {
        VpnConfig.addSetting(new VpnConfig.Setting(Config.LOG_LEVEL));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.PROXY_ADDR));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.PROXY_IPVER));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.PROXY_PORT));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.SOCKS5_VERIFY, String.valueOf(VpnConfig.S5Verify.NONE)));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.SOCKS5_USERNAME));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.SOCKS5_PASSWORD));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.PROXY_DNS_SERVER));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.DNS_SERVER));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.CTRL_PACKAGES));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.CTRL_IP));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.CTRL_DOMAIN));
        VpnConfig.addSetting(new VpnConfig.Setting(Config.CAP_OUTPUT_DIR));
    }


}
