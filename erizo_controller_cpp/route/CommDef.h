#ifndef __COMMON_DEF_H__
#define __COMMON_DEF_H__

namespace edu{
namespace common{
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
	HK   = 128, // 香港
	BRA  = 256, // 巴西
	EU   = 512,  //欧洲
    NA   = 1024, //北美
	TEST = 2048,
	OA   = 4096,  //大洋洲国家
	AF   = 8192,  // 非洲国家
};
}
}

#endif