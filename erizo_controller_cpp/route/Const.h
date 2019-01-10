#ifndef CROSS_PLANTFORM_CONST_H_
#define CROSS_PLANTFORM_CONST_H_

#define LBS_SERVER_PORT1 1001
#define LBS_SERVER_PORT2 2001
#define LBS_SERVER_PORT3 3001

#define MAX_SVID_NUM 20
#define SOCKET_TIMEOUT 60000
#define MYCOLLECTION_FOLDER 1000000000
#define PUBLICCHANNEL_FOLDER 1000000001
#define RECEMMONDCHANNEL_FOLDER 1000000002
#define HOT_FOLDER 1000000003
#define UDP_OFFSET 10000
#define INVALID_UID 0xffffffff

#define NONE_SERVER_ID 0xfffffff
#define NONE_SERVER_ID64 int64_t(-1)

#define MAX_CHANNEL_SIZE 3
#ifdef WIN32
#define FAV_FOLDER		_T("1000000000")
#define FAV_PUBLIC		_T("1000000001")
#define FAV_RECOMMEND	_T("1000000002")
#define FAV_HOT			_T("1000000003")
#define KICKOUT_PID _T("")
#endif

#define SEQPAGE					1000
#define PACKET_SEQ_INCREMENT		2

#define DEF_SESSIONKEY_LENGTH 16

#define ADMIN_CHANNEL 0

namespace protocol{
	enum E_ONLINESTAT{ONLINE, OFFLINE};
}


namespace protocol{
	namespace im{
		enum E_WAITCHECKLIST_OP{ ACCEPT, NOTACCEPT };
		enum E_ITEMTYPE{	BUDDY,	FOLDER};
	}
}



namespace protocol {
namespace uinfo {
enum SEX {
	female,
	male
};

}
}

namespace protocol{
namespace login{
enum LinkStatus {
	//sync_uinfo = 100,
	//sync_ulist = 180,
	LINK_INIT,
	LINK_LBS,
	EXCHANGE_PASSWORD,
	LBS_ERROR,
	LINK_AUTH,
	LOGIN_SUCCESS,
	PASSWD_ERROR,
	SERVER_ERROR,
	NET_BROKEN,
	TIMEOUT,
	KICKOFF,
	LOGOUT,
	UNKNOWN,
	PROTOCOL_OLD,
	LINK_GETMAIL,
	NON_EMAIL,
	LBS_SHUTDOWN,
	RELOGIN_SUCCESS,
	SERVER_REDIRECT,
	USER_NONEXIST,
	UDB_NOTENABLE,
	//success = 200,
	//	passwd_err = 401,
	//	
	// timeout	  = 405,
	//	unknow	  = 406
};
}
}
/*
enum ISPType{
	AUTO_DETECT = 0,
	CTL  = 1,	 //电信
	CNC  = 2,	 //网通
	MULTI= 3,  //双线
	CNII = 4,	 //铁通
	EDU  = 8,	 //教育网
	WBN  = 16, //长城宽带
	MOB  = 32, //移动
	BGP  = 64,  //BGP
	HK  = 128, // 香港
	BRA = 256, // 巴西
    EU   = 512,  //欧洲
    NA   = 1024, //北美
    MAX_ISP   = 1024  //THIS MUST BE THE MAXIMUM NUMBER AMONG AVAILABLE ISPS!!! 
};
*/
enum AreaType{//同网络类型的定义数字必需连续(分配的时候会用)
	AREA_UNKNOWN = 0,
	CTL_EAST   = 16,  //电信东区	10000  (1<<4) + 0
	CTL_WEST   = 17,  //电信西区	10001  (1<<4) + 1
	CTL_SOUTH  = 18,  //电信南区	10010  (1<<4) + 2
	CTL_NORTH  = 19,  //电信北区	10011 (1<<4) + 3
    CTL_X      = 20,    //电信未知省份（目前分电信东区）
	CNC_NE	   = 32,  //网通东北   100000 (2<<4) + 0
	CNC_NC	   = 33,  //网通华北   100001 (2<<4) + 1
    CNC_SC     = 34,  //网通南方   100010 (2<<4) + 2
    CNC_I      = 35,     //网通小运营商 （目前分网通华北）
    CNC_X      = 36,    //联通未知省份 （目前分网通华北
	CNC_EC     = 37,    //华东：上海、江苏、浙江、安徽、江西、山东、湖北（目前分网通华北）
    CNC_SW     = 38,     //西南：四川、云南、贵州、重庆、西藏（目前分网通华北）
    CNC_NW     = 39,     //西北：宁夏 、新疆、青海、陕西 、甘肃（目前分网通华北）
	CNII_AREA  = 64,  //铁通	  1000000 (4<<4) + 1
	EDU_AREA   = 128, //教育网	 10000000 (8<<4) +0
	WBN_AREA   = 256, //长城宽带100000000 (16<<4) +0
	MOB_AREA   = 512, //移动   1000000000 (32<<4) + 0
	BGP_AREA   = 1024, //BGP	10000000000 (64<<4) + 0
	HK_AREA    = 2048, //香港
	PH_AREA    = 2049, //菲律宾分区
	TW_AREA    = 2050, //台湾分区
	BRA_AREA   = 4096,	//巴西
	EU_AREA    = 8192,	 //欧洲分区
	NA_AREA    = 16384,  //北美分区
	TEST_AREA     = 32768,    //test分区
	OA_AREA       = 65536,   //大洋洲分区（目前分电信东区）
	AF_AREA       = 131072,   //非洲分区（目前分电信东区）
	AREA_INVALID  = 0xFFFFFFFF
};
enum {
	SEL_NONE = 0,
	SEL_ALL = -1,
	SEL_READ = 1, 
	SEL_WRITE = 2, 
	SEL_RW = 3, //
	SEL_ERROR = 4,

	// notify only
	SEL_TIMEOUT = 8,

	// setup only, never notify
	// SEL_R_ONESHOT = 32, SEL_W_ONESHOT = 64, SEL_RW_ONESHOT = 96,
	SEL_CONNECTING = 128
};

enum ENUM_HASH_TYPE
{ 
	NETWORK_HASH = 0,
	GROUP_HASH = 1
};


#endif /*CROSS_PLANTFORM_CONST_H_*/
