using System.Collections;
using System.Collections.Generic;
using System;
using UnityEngine;
using System.Runtime.InteropServices;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void MyDelegate(string str);



public class Forwarder : MonoBehaviour {

    [DllImport("__Internal")]
    public static extern void SetDebugFunction(IntPtr fp);
    [DllImport("__Internal")]
    private static extern int initENet();
    [DllImport("__Internal")]
    private static extern void release();
    [DllImport("__Internal")]
    private static extern int version();
    [DllImport("__Internal")]
    private static extern void setupLogger(string filename);
    [DllImport("__Internal")]
    private static extern void setDebug(bool debug);
    [DllImport("__Internal")]
    private static extern int setProtocolRule(int serverId, int protocol, string rule);
    [DllImport("__Internal")]
    private static extern void initServers(string sConfig);
    [DllImport("__Internal")]
    private static extern int createServer(string sConfig);
    [DllImport("__Internal")]
    private static extern int sendText(int serverId, int clientId, string data);
    [DllImport("__Internal")]
    private static extern int getCurEvent();
    [DllImport("__Internal")]
    private static extern int getCurProcessServerID();
    [DllImport("__Internal")]
    private static extern int getCurProcessClientID();
    [DllImport("__Internal")]
    private static extern void getCurProcessPacket(ref IntPtr data, ref int len);
    [DllImport("__Internal")]
    private static extern void setTimeout(int serverId, int timeoutLimit, int timeoutMinimum, int timeoutMaximum);
    [DllImport("__Internal")]
    private static extern void setPingInterval(int serverId, int interval);
    [DllImport("__Internal")]
    private static extern void pollOnceByServerID(int serverId);
    [DllImport("__Internal")]
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
        Debug.Log("hello world");
        // MyDelegate callback_delegate = new MyDelegate(CallBackFunction);
        Debug.Log("debug p 0");
        // IntPtr intptr_delegate = Marshal.GetFunctionPointerForDelegate(callback_delegate);
        // SetDebugFunction(intptr_delegate);
        Debug.Log("debug p 1");
        initENet();
        Debug.Log("debug p 2");
        //version();
        setupLogger("");
        Debug.Log("debug p 3");
        setDebug(false);
        Debug.Log("debug p 4");
        ServerConfg config = new ServerConfg();
        config.id = 1;
        config.desc = "client_enet";
        config.netType = "enet";
        config.port = 20002;
        config.peers = 200;
        config.encrypt = true;
        config.encryptkey = "abcdefghabcdefgh";
        config.base64 = true;
        config.compress = true;
        config.address = "192.168.18.88";
        config.isClient = true;
        config.reconnect = true;
        serverId = createServer(JsonUtility.ToJson(config));
        Debug.Log("[forwarder] createServer:" + serverId);
        Debug.Log("debug p 5");
        setProtocolRule(serverId, 2, "Process");
        Debug.Log("debug p 6");
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
