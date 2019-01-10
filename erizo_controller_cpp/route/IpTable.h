#ifndef __IP_TABLE_IMP_H__
#define __IP_TABLE_IMP_H__

#include <string>
#include "IP_TABLE.h"
#include "CommDef.h"
#include "Const.h"
namespace edu {
namespace iptable {
class IpTable : public IP_TABLE {
public:
  IpTable();
	bool loadIspIpDataFile(std::string filename, std::string delim = " ");
	ISPType getIspType(unsigned long ip);
  AreaType getAreaType(unsigned long ip);
  IP_TABLE_VALUE getIpTableValue(unsigned long ip);
	static IpTable& instance();
  void addNewIsp(const std::string& name, ISPType isp);
  void removeIsp(const std::string& name);
  void addNewArea(const std::string& name, AreaType isp);
  void removeArea(const std::string& name);

  std::map<std::string, ISPType>& getIspMap() { return m_ispMap; };
  std::map<std::string, AreaType>& getAreaMap() { return m_areaMap; };
  bool areaNameIsExist(const std::string& name){return (m_areaMap.find(name.c_str()) != m_areaMap.end()); };
  //大ISP:电信 网通
  bool isBigIsp(ISPType isp) { return isp == CTL || isp == CNC;}
  //是否已被iptable划分好
  bool isIpPartitioned(unsigned long ip);

  static void setDefault(ISPType isp, AreaType area);

private:
	// 初始化
	static ISPType m_defaultIsp;
  static AreaType m_defaultArea;

  std::map<std::string, ISPType> m_ispMap;
  std::map<std::string, AreaType> m_areaMap;
};
}
}
#endif // __IP_TABLE_IMP_H__
