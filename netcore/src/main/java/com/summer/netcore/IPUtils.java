package com.summer.netcore;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * Created by summer on 05/09/2018.
 */

public class IPUtils {

    private static Pattern VALID_IPV4_PATTERN = null;
    private static Pattern VALID_IPV6_PATTERN = null;
    private static final String ipv4Pattern = "(([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.){3}([01]?\\d\\d?|2[0-4]\\d|25[0-5])";
    private static final String ipv6Pattern = "([0-9a-f]{1,4}:){7}([0-9a-f]){1,4}";

    static {
        try {
            VALID_IPV4_PATTERN = Pattern.compile(ipv4Pattern, Pattern.CASE_INSENSITIVE);
            VALID_IPV6_PATTERN = Pattern.compile(ipv6Pattern, Pattern.CASE_INSENSITIVE);
        } catch (PatternSyntaxException e) {
            e.printStackTrace();
        }
    }

    /**
     * Determine if the given string is a valid IPv4,  This method
     * uses pattern matching to see if the given string could be a valid IPv4 address.
     *
     * @param ipAddress A string that is to be examined to verify whether or not
     *  it could be a valid IP address.
     * @return <code>true</code> if the string is a value that is a valid IPv4 address,
     *  <code>false</code> otherwise.
     */
    public static boolean isIpv4Address(String ipAddress) {

        Matcher m1 = VALID_IPV4_PATTERN.matcher(ipAddress);
        return  m1.matches();
    }

    /**
     * Determine if the given string is a valid IPv6,  This method
     * uses pattern matching to see if the given string could be a valid IPv4 address.
     *
     * @param ipAddress A string that is to be examined to verify whether or not
     *  it could be a valid IP address.
     * @return <code>true</code> if the string is a value that is a valid IPv6 address,
     *  <code>false</code> otherwise.
     */
    public static boolean isIpv6Address(String ipAddress) {

        Matcher m1 = VALID_IPV6_PATTERN.matcher(ipAddress);
        return  m1.matches();
    }

}
