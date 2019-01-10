#ifndef __IP_TABLE_H__
#define __IP_TABLE_H__

/*
 * 2006/12/29 by zqwang
 * 
 */

#include <map>
#include <assert.h>
#include "CommDef.h"
#include "Const.h"
using namespace edu::common;

namespace edu {
namespace iptable {
enum ISPType {
	AUTO_DETECT = 0,
	CTL       	= 1,    //电信
	CNC       	= 2,    //网通
	MUTIL     	= 3,    //双线
	CNII      	= 4,    //铁通
	EDU       	= 8,    //教育网
	WBN       	= 16,   //长城宽带
	MOB       	= 32,   //移动
	BGP       	= 64,   //BGP
	ASIA        = 128,  //亚洲
	SA_ISP      = 256,  //南美 SA > SA_ISP 由于跟另一个枚举值声明冲突。
	EU        	= 512,  //欧洲
	NA        	= 1024, //北美
	TEST      	= 2048,
	OA        	= 4096,  //大洋洲国家
	AF        	= 8192,  // 非洲国家
	INTRANET  	= 32768,
	MAX_ISP   	= 65536  //THIS MUST BE THE MAXIMUM NUMBER AMONG AVAILABLE ISPS!!! (1<<16)
};

enum AreaType {//同网络类型的定义数字必需连续(分配的时候会用)
	AREA_UNKNOWN  = 0,
	CTL_EAST      = 16,     //电信东区     10000  (1<<4) + 0
	CTL_WEST      = 17,     //电信西区     10001  (1<<4) + 1
	CTL_SOUTH     = 18,     //电信南区     10010  (1<<4) + 2
	CTL_NORTH     = 19,     //电信北区     10011  (1<<4) + 3
	CTL_X         = 20,     //电信未知省份（目前分电信东区）
	CNC_NE        = 32,     //网通东北     100000 (2<<4) + 0
	CNC_NC        = 33,     //网通华北     100001 (2<<4) + 1
	CNC_SC        = 34,     //网通南方     100010 (2<<4) + 2
	CNC_I         = 35,     //网通小运营商 （目前分网通华北）
	CNC_X         = 36,     //联通未知省份 （目前分网通华北）
	CNC_EC        = 37,     //华东：上海、江苏、浙江、安徽、江西、山东、湖北（目前分网通华北）
	CNC_SW        = 38,     //西南：四川、云南、贵州、重庆、西藏（目前分网通华北）
	CNC_NW        = 39,     //西北：宁夏 、新疆、青海、陕西 、甘肃（目前分网通华北）
	CNII_AREA     = 64,     //铁通        	1000000 (4<<4) + 1
	EDU_AREA      = 128,    //教育网     	10000000 (8<<4) + 0
	WBN_AREA      = 256,    //长城宽带 		100000000 (16<<4) + 0
	MOB_AREA      = 512,    //移动    		1000000000 (32<<4) + 0
	BGP_AREA      = 1024,   //BGP     		10000000000 (64<<4) + 0
	HK_AREA       = 2048,   //香港分区
	PH_AREA       = 2049,   //菲律宾分区
	TW_AREA       = 2050,   //台湾分区
	BRA_AREA      = 4096,   //巴西分区 
	EU_AREA       = 8192,   //欧洲分区
	NA_AREA       = 16384,  //北美分区
	TEST_AREA     = 32768,  //test分区
	OA_AREA       = 65536,   //大洋洲分区（目前分电信东区）
	AF_AREA       = 131072,   //非洲分区（目前分电信东区）
	AREA_INVALID  = 0xFFFFFFFF
};

struct IP_TABLE_KEY
{
public:
	unsigned long     from; // ip 地址范围
	unsigned long     to;

public:
	IP_TABLE_KEY()
	{

	}

	IP_TABLE_KEY( const IP_TABLE_KEY& data )
	{
		assert( data.to >= data.from );
		from = data.from;
		to   = data.to;
	}

	IP_TABLE_KEY( unsigned long _from, unsigned long _to )
	{
		assert( _to >= _from );
		from = _from;
		to   = _to;
	}

	explicit IP_TABLE_KEY( unsigned long ip )
	{
		from = ip;
		to   = ip;
	}

	bool operator==( const IP_TABLE_KEY& data ) const
	{
		assert( data.to >= data.from  );
		assert( to >= from );

		// 存在包含关系即相等
		if(    ( data.from >= from )
			&& ( data.to   <= to   ) )
		{
			// data == this
			// data 在 this 的区间内
			return true;
		}

		// 根据对称性，需要保证 this == data
		if(    ( from >= data.from )
			&& ( to   <= data.to   ) )
		{
			// this == data
			// this 在 data 的区间内
			return true;
		}

		return false;
	}

	bool operator!=( const IP_TABLE_KEY& data ) const
	{
		assert( data.to >= data.from  );
		assert( to >= from );

		// 2008/02/21 by zqwang
		// 不存在包含关系就不相等
		if(    ( data.from >= from )
			&& ( data.to   <= to   ) )
		{
			// data == this
			// data 在 this 的区间内
			return false;
		}

		// 根据对称性，需要保证 this != data
		if(    ( from >= data.from )
			&& ( to   <= data.to   ) )
		{
			// this == data
			// this 在 data 的区间内
			return false;
		}

		return true;
	}

	// 2008/02/21 by zqwang
	// 注意对称性
	// 如果A > B，那么B < A
	bool operator> ( const IP_TABLE_KEY& data ) const
	{
		assert( data.to >= data.from );
		assert( to >= from );

		// 是否相等
		if( this->operator ==( data ) )
		{
			return false;
		}

		// 大于
		// to > data.to => from > data.from，所以 < 一定不成立，没有二义性
		return to > data.to;
	}

	bool operator< ( const IP_TABLE_KEY& data ) const
	{
		assert( data.to  >= data.from  );
		assert( to >= from );

		// 是否相等
		if( this->operator ==( data ) )
		{
			return false;
		}

		// 小于
		// from < data.from => to < data.to，所以 > 一定不成立，没有二义性
		return from < data.from;
	}
};

struct IP_TABLE_VALUE
{
public:
  	ISPType            isp;
  	AreaType           area;

	IP_TABLE_VALUE()
		: isp( AUTO_DETECT )
    , area( AREA_UNKNOWN )
	{

	}

	explicit IP_TABLE_VALUE( ISPType _isp , AreaType _area)
		: isp( _isp )
    , area( _area )
	{

	}

	IP_TABLE_VALUE& operator=( IP_TABLE_VALUE data )
	{
		isp = data.isp;
    	area = data.area;
		return *this;
	}
};

class IP_TABLE
{
public:
	typedef std::map< IP_TABLE_KEY, IP_TABLE_VALUE >                 IP_TABLE_MAP;
	typedef std::map< IP_TABLE_KEY, IP_TABLE_VALUE >::iterator       IP_TABLE_MAP_IT;
	typedef std::map< IP_TABLE_KEY, IP_TABLE_VALUE >::const_iterator IP_TABLE_MAP_CONST_IT;

public:
	inline unsigned int size() const                                            { return m_table.size();                                                                                }
	inline IP_TABLE_MAP_IT begin()                                              { return m_table.begin();                                                                               }
	inline IP_TABLE_MAP_IT end()                                                { return m_table.end();                                                                                 }
	inline IP_TABLE_MAP_CONST_IT begin() const                                  { return m_table.begin();                                                                               }
	inline IP_TABLE_MAP_CONST_IT end() const                                    { return m_table.end();                                                                                 }

	inline void insert( const IP_TABLE_KEY& key, const IP_TABLE_VALUE& value )  { m_table.insert( IP_TABLE_MAP::value_type( key, value ) );                                             }
	inline void insert( unsigned long from, unsigned long to, ISPType value , AreaType area)   { m_table.insert( IP_TABLE_MAP::value_type( IP_TABLE_KEY( from, to ), IP_TABLE_VALUE( value , area) ) );   	}
	inline void erase( IP_TABLE_MAP_IT it )                                     { m_table.erase( it );              	                                                                }
	inline void clear()                                                         { m_table.clear();                                                                                      }

	inline IP_TABLE_MAP_IT find( const IP_TABLE_KEY& key )                      { return m_table.find( key );                                                                           }
	inline IP_TABLE_MAP_CONST_IT find( const IP_TABLE_KEY& key ) const          { return m_table.find( key );                                                                           }
	inline IP_TABLE_MAP_IT find( unsigned long ip )	                            { return m_table.find( IP_TABLE_KEY( ip ) );	                                                        }
	inline IP_TABLE_MAP_CONST_IT find( unsigned long ip ) const                 { return m_table.find( IP_TABLE_KEY( ip ) );                                                            }

	// 2006/12/29
	inline IP_TABLE_VALUE getValue( unsigned long ip) const
	{
		IP_TABLE_MAP_CONST_IT it = this->find( ip );
		if( it != this->end() )
		{
			// find
			return it->second;
		}
		else
		{
      		IP_TABLE_VALUE default_v;
      		default_v.area = AREA_UNKNOWN;
      		default_v.isp = AUTO_DETECT;
			return default_v;
		}
	}

public:
	IP_TABLE();

	~IP_TABLE();

private:
	IP_TABLE_MAP     m_table;
};

}
}
#endif // __IP_TABLE_H__
