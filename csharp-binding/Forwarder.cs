using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;
using System.Linq;
using WebSocketSharp;



/*
|  1 byte	    |  1 byte			|		1   byte		|		1 byte				|
|  Version		|  Length of Header	|	ProtocolType		|		 hash				|
|									4 bytes												|
|									headerFlag											|
|                                   n bytes                                             |
|                           dynamic data sequence by flag                               |

*/

enum Protocol {
	Unknown = 0,
	SysCmd = 1,
	Forward = 2,
	Process = 3,
	BatchForward = 4,
};

enum HeaderFlag {
	IP = 1 << 0, // IPv4 address
	HostID = 1 << 1, // send from/to which host
	ClientID = 1 << 2, // send from/to which client of host
	SubID = 1 << 3, // SysCmd's subID
	// type flag
	Base64 = 1 << 4,
	Encrypt = 1 << 5,
	Compress = 1 << 6,
	Broadcast = 1 << 7,
	ForceRaw = 1 << 8, // No Base64、Encrypt、Compress
	PacketLen = 1 << 9,
};


static class Const {
	public static int HeaderVersion = 1;
	public static int HeaderBaseLength = 8;
	public static int HeaderDataLength = 0xff;
	public static Dictionary<HeaderFlag, int> FlagToBytes = new Dictionary<HeaderFlag, int>();
	public static Dictionary<HeaderFlag, string> FlagToStr = new Dictionary<HeaderFlag, string>();

	static Const() {
		FlagToBytes[HeaderFlag.IP] = 4;
		FlagToBytes[HeaderFlag.HostID] = 1;
		FlagToBytes[HeaderFlag.ClientID] = 4;
		FlagToBytes[HeaderFlag.SubID] = 1;
		FlagToBytes[HeaderFlag.Base64] = 0;
		FlagToBytes[HeaderFlag.Encrypt] = 0;
		FlagToBytes[HeaderFlag.Compress] = 4;
		FlagToBytes[HeaderFlag.Broadcast] = 0;
		FlagToBytes[HeaderFlag.ForceRaw] = 0;
		FlagToBytes[HeaderFlag.PacketLen] = 4;

		FlagToStr[HeaderFlag.IP] = "IP";
		FlagToStr[HeaderFlag.HostID] = "HostID";
		FlagToStr[HeaderFlag.ClientID] = "ClientID";
		FlagToStr[HeaderFlag.SubID] = "SubID";
		FlagToStr[HeaderFlag.Base64] = "Base64";
		FlagToStr[HeaderFlag.Encrypt] = "Encrypt";
		FlagToStr[HeaderFlag.Compress] = "Compress";
		FlagToStr[HeaderFlag.Broadcast] = "Broadcast";
		FlagToStr[HeaderFlag.ForceRaw] = "ForceRaw";
		FlagToStr[HeaderFlag.PacketLen] = "PacketLen";
	}
}

// small endian
class ForwardHeader {
	public byte[] m_Buf;
	public ForwardHeader() {
		m_Buf = Enumerable.Repeat((byte)0x00, Const.HeaderBaseLength + Const.HeaderDataLength).ToArray();
		setVersion(Const.HeaderVersion);
	}
	public ForwardHeader(ref byte[] buf) {
		m_Buf = buf;
	}
	public void setBuf(ref byte[] buf) {
		m_Buf = buf;
	}
	public void setVersion(int version) {
		Buffer.SetByte(m_Buf, 0, (byte)version);
		// m_Buf[0] = (byte)version;
	}
	public int getVersion() {
		return Buffer.GetByte (m_Buf, 0);
	}
	public int getHeaderLength() {
		return Buffer.GetByte (m_Buf, 1);
	}
	public void setHeaderLength(int l) {
		Buffer.SetByte(m_Buf, 1, (byte)l);
	}
	public void resetHeaderLength() {
		int dataSize = calDataSize();
		int length = dataSize + Const.HeaderBaseLength;
		setHeaderLength(length);
	}
	public Protocol getProtocol() {
		return (Protocol)m_Buf[2];
	}
	public void setProtocol(Protocol p) {
		m_Buf[2] = (byte)p;
	}
	public bool isFlagOn(HeaderFlag f) {
		int flag = GetInt32(4); 
		return (flag & (int)(f)) > 0;
	}
	public void setFlag(int flag) {
		WriteInt32 (4, flag);
	}

	public int GetInt32(int index) {
		return System.BitConverter.ToInt32(m_Buf, index); 
	}

	public void WriteInt32(int index, int val)
	{
		byte[ ] bytes = BitConverter.GetBytes( val );
		WriteBytes(index, bytes);
	}


	public char GetInt8(int index) {
		return System.BitConverter.ToChar(m_Buf, index); 
	}

	public void WriteInt8(int index, char val)
	{
		byte[ ] bytes = BitConverter.GetBytes( val );
		WriteBytes(index, bytes);
	}

	public void WriteBytes(int index, byte[] bytes)
	{
		for (int i = 0; i < bytes.Length; i++) {
			m_Buf [index + i] = bytes [i];
		}
	}

	public void cleanFlag() {
		setFlag (0);
	}
	public void setFlag(HeaderFlag f, bool on) {
		int flag = GetInt32(4); 
		if (on) {
			flag |= (int)f;
		} else {
			flag &= ~((int)f);
		}
		setFlag(flag);
	}
	public int getFlagPos(HeaderFlag f) {
		int flag = GetInt32(4); 
		int count = 0;
		for (int i = 0; i < 32; i++) {
			int _f = 1 << i;
			if (_f == (int)f) {
				return count;
			} else if ((flag & _f) > 0) {
				count += Const.FlagToBytes[(HeaderFlag)_f];
			}
		}
		return 0;
	}
	public int calDataSize() {
		int flag = GetInt32(4); 
		int bytesNum = 0;
		for (int i = 0; i < 32; i++) {
			int _f = 1 << i;
			if ((flag & _f) > 0) {
				bytesNum += Const.FlagToBytes[(HeaderFlag)_f];
			}
		}
		return bytesNum;
	}
	public int getHostID() {
		return (int)GetInt8(Const.HeaderBaseLength + getFlagPos(HeaderFlag.HostID));
	}
	public void setHostID(int hostID) {
		WriteInt8(Const.HeaderBaseLength + getFlagPos(HeaderFlag.HostID), (char)hostID);
	}
	public int getClientID() {
		return GetInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.ClientID));
	}
	public void setClientID(int clientID) {
		WriteInt32 (Const.HeaderBaseLength + getFlagPos(HeaderFlag.ClientID), clientID);
	}
	public int getSubID() {
		return GetInt8(getFlagPos(HeaderFlag.HostID));
	}
	public void setSubID(int subID) {
		WriteInt8(getFlagPos(HeaderFlag.HostID), (char)subID);
	}
	public int getIP() {
		return GetInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.IP));
	}
	public void setIP(int ip) {
		WriteInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.IP), ip);
	}
	public int getUncompressedSize() {
		return GetInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.Compress));
	}
	public void setUncompressedSize(int size) {
		WriteInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.Compress), size);
	}
	public int getPacketLength() {
		return GetInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.PacketLen));
	}
	public void setPacketLength(int len) {
		WriteInt32(Const.HeaderBaseLength + getFlagPos(HeaderFlag.PacketLen), len);
	}
	public string getHeaderDebugInfo() {
		string info = "";
		foreach (var flag in Const.FlagToBytes) {
			info += Const.FlagToStr[flag.Key];
			if (isFlagOn(flag.Key)) {
				info += " on";
			} else {
				info += " off";
			}
			info += "\n";
		}
		info += "headerLen = " + getHeaderLength();
		info += "packetLen = " + getPacketLength();
		return info;
	}
}


[Serializable]
public class FwdPair {
	public int forwardServerId;
	public int forwardClientId;
}

[Serializable]
public class Header {
	public string _cmd;
	public int _queIndex;
	public int _queStatus;
}


[Serializable]
public class PacketData {
	public Header header;
	public string _cmd;
	public Dictionary<string, string> data;
	public FwdPair fwdPair;
}

delegate void Callback();


public class Forwarder : MonoBehaviour {

	private WebSocket ws;

	private bool isConnected ;

	private int m_Count;

	private ArrayList m_Packets;

	private Dictionary<int, Callback> m_Callbacks;

	public string address = "localhost";

	public int port = 8080;



	void Start () {
		ws = null;
		isConnected = false;
		m_Count = 0;
		m_Packets = new ArrayList();
		m_Callbacks = new Dictionary<int, Callback>();
		string url = "ws://" + address + ":" + port + "/";
		Debug.Log ("Forwarder Start, url: " + url);
		ws = new WebSocket (url);

		ws.OnMessage += (sender, e) => {
			Debug.Log ("OnMessage");
			Debug.Log (e.Data);
		};

		ws.OnError += (sender, e) => {
			Debug.Log ("OnError");
			isConnected = false;
			Disconnect();
		};
		ws.OnClose += (sender, e) => {
			Debug.Log ("onClose");
			isConnected = false;
		};
		ws.OnOpen += (sender, e) => {
			Debug.Log ("onOpen");
			isConnected = true;
		};
		Connect ();

		Invoke ("Disconnect", 3);
	}

	void Connect() {
		ws.Connect ();
	}

	void Disconnect() {
		ws.Close ();
	}

	bool IsConnected() {
		return isConnected;
	}

	void OnSendJson(string textData) {
		ws.Send (textData);
	}

	void onSendBytes(byte[] bytesData) {
		ws.Send (bytesData);
	}

	void onSendMsg(string cmd, PacketData dPacketData) {
		ForwardHeader outHeader = new ForwardHeader();
		outHeader.setProtocol(Protocol.Forward);
		outHeader.setFlag(HeaderFlag.Broadcast, true);
		if (dPacketData.fwdPair !=null && dPacketData.fwdPair.forwardServerId > 0) {
			dPacketData.fwdPair = null;
			outHeader.setFlag(HeaderFlag.HostID, true);
			outHeader.setHostID(dPacketData.fwdPair.forwardServerId);
			outHeader.setFlag(HeaderFlag.ClientID, true);
			outHeader.setClientID(dPacketData.fwdPair.forwardClientId);
		}
		outHeader.resetHeaderLength();
		string sData = JsonUtility.ToJson(dPacketData);
		byte[] dataBuf = System.Text.Encoding.Default.GetBytes ( sData );
		byte[] buf = new byte[outHeader.getHeaderLength() + dataBuf.Length];
		Array.Copy(outHeader.m_Buf, 0, buf, 0, outHeader.getHeaderLength()); 
		Array.Copy(dataBuf, 0, buf, outHeader.getHeaderLength(), dataBuf.Length); 
		onSendBytes(buf);
	}

	void sendMessage(string cmd, Dictionary<string, string> dData, Callback callback, FwdPair fwdPair) {
		PacketData dPacketData = pushPacket(cmd, dData, fwdPair);
		if (dPacketData == null) {
			return;
		}
		if (callback != null) {
			m_Callbacks[dPacketData.header._queIndex] = callback;
		}
		flushMessage();
	}

	PacketData pushPacket(string cmd, Dictionary<string, string> dData, FwdPair fwdPair) {
		int _queIndex = m_Count;
		m_Count += 1;
		PacketData dPacketData = new PacketData();
		dPacketData.header._cmd = cmd;
		dPacketData.header._queIndex = _queIndex;
		dPacketData.header._queStatus = 0; //0:未发送，1:发送， 2:已收到反馈，可删除
		dPacketData.data = dData;
		if (fwdPair != null) {
			dPacketData.fwdPair = fwdPair;
		}
		// if (m_onPushPacket) m_onPushPacket(dPacketData);
		m_Packets.Add(dPacketData);
		return dPacketData;
	}

	void flushMessage() {
		// 发送队列中的包
		for (int i = 0; i < m_Packets.Count; i++) {
			PacketData dPacketData = (PacketData)m_Packets[i];
			if (dPacketData.header._queStatus > 0) {
				continue;
			}
			if (IsConnected()) {
				dPacketData.header._queStatus = 1;
				string cmd = dPacketData.header._cmd;
				dPacketData._cmd = cmd;
				onSendMsg(cmd, dPacketData);
			} else {
				Invoke ("flushMessage", 1);
				return;
			}
		}
	}
}