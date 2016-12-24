using System.Collections;
using System.Collections.Generic;
using System;
using UnityEngine;
using System.Runtime.InteropServices;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void MyDelegate(string str);



public class Forwarder : MonoBehaviour {

    [DllImport("forwarder")]
    public static extern void SetDebugFunction(IntPtr fp);
    [DllImport("forwarder")]
    private static extern int initENet();
    [DllImport("forwarder")]
    private static extern void release();
    [DllImport("forwarder")]
    private static extern int version();
    [DllImport("forwarder")]
    private static extern void setupLogger(string filename);
    [DllImport("forwarder")]
    private static extern void setDebug(bool debug);
    [DllImport("forwarder")]
    private static extern int setProtocolRule(int serverId, int protocol, string rule);
    [DllImport("forwarder")]
    private static extern void initServers(string sConfig);
    [DllImport("forwarder")]
    private static extern int createServer(string sConfig);
    [DllImport("forwarder")]
    private static extern int sendText(int serverId, int clientId, string data);
    [DllImport("forwarder")]
    private static extern int getCurEvent();
    [DllImport("forwarder")]
    private static extern int getCurProcessServerID();
    [DllImport("forwarder")]
    private static extern int getCurProcessClientID();
    [DllImport("forwarder")]
    private static extern void getCurProcessPacket(ref IntPtr data, ref int len);
    [DllImport("setTimeout")]
    private static extern void setTimeout(int serverId, int timeoutLimit, int timeoutMinimum, int timeoutMaximum);
    [DllImport("setTimeout")]
    private static extern void setPingInterval(int serverId, int interval);
    [DllImport("forwarder")]
    private static extern void pollOnceByServerID(int serverId);
    [DllImport("forwarder")]
    private static extern string getStatInfo();

    private class ServerConfg {
        public int id;
        public string desc;
        public string netType;
        public int port;
        public int peers;
        public bool encrypt;
        public string encryptkey;
        public bool base64;
        public bool compress;
        public string address;
        public bool isClient;
        public bool reconnect;
    };
    private int serverId;

    void Start()
    {
        MyDelegate callback_delegate = new MyDelegate(CallBackFunction);
        IntPtr intptr_delegate = Marshal.GetFunctionPointerForDelegate(callback_delegate);
        // SetDebugFunction(intptr_delegate);

        initENet();
        //version();
        setupLogger("");
        setDebug(false);

        ServerConfg config = new ServerConfg();
        config.id = 1;
        config.desc = "client_enet";
        config.netType = "enet";
        config.port = 9999;
        config.peers = 200;
        config.encrypt = true;
        config.encryptkey = "abcdefghabcdefgh";
        config.base64 = true;
        config.compress = true;
        config.address = "localhost";
        config.isClient = true;
        config.reconnect = true;
        serverId = createServer(JsonUtility.ToJson(config));
        Debug.Log("[forwarder] createServer:" + serverId);

        setProtocolRule(serverId, 2, "Process");
    }

    // Update is called once per frame
    void Update()
    {
        pollOnceByServerID(serverId);
        int evt = getCurEvent();
        if (evt > 0) {
            Debug.Log("evt:" + evt);
            switch (evt)
            {
                case 1: // connected
                    setTimeout(serverId, 0, 1000, 2000);
                    setPingInterval(serverId, 1000);
                    Debug.Log("connected");
                    break;
                case 2: // disconnected
                    Debug.Log("disconnected");
                    break;
                case 3: // message
                    Debug.Log("message");
                    IntPtr packet = IntPtr.Zero;
                    int len = 0;
                    getCurProcessPacket(ref packet, ref len);
                    Debug.Log("received: " + len);
                    if (len > 0) {
                        byte[] data = new byte[len];
                        Marshal.Copy(packet, data, 0, len);
                        string s = System.Text.Encoding.UTF8.GetString(data, 0, data.Length);
                        Debug.Log("received: " + s);
                    }
                    int ret = sendText(serverId, 0, "world");
                    Debug.Log("ret:" + ret);
                    break;
            } 
        }
    }
    static void CallBackFunction(string str)
    {
        Debug.Log("[forwarder] " + str);
    }

}
