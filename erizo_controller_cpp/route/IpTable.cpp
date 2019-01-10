#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>

#include "IpTable.h"
#include "Utility.h"
#include "CommDef.h"
#include "Const.h"

using namespace std;
using namespace edu::common;
namespace edu
{
namespace iptable
{
ISPType IpTable::m_defaultIsp = CTL;
AreaType IpTable::m_defaultArea = CTL_EAST;

IpTable::IpTable()
{
  m_ispMap["AF"] = AF;
  //m_ispMap["ASIA"] = ASIA;
  m_ispMap["BGP"] = BGP;
  m_ispMap["CNC"] = CNC;
  m_ispMap["CNII"] = CNII;
  m_ispMap["CTL"] = CTL;
  m_ispMap["EDU"] = EDU;
  m_ispMap["EU"] = EU;
  m_ispMap["MOB"] = MOB;
  m_ispMap["NA"] = NA;
  m_ispMap["OA"] = OA;
  //m_ispMap["SA"] = SA_ISP;
  m_ispMap["TEST"] = TEST;
  m_ispMap["WBN"] = WBN;
  // m_ispMap["INTRANET"] = INTRANET;

  m_areaMap["AF"] = AF_AREA;
  m_areaMap["BRA"] = BRA_AREA;
  m_areaMap["CNC_EC"] = CNC_EC;
  m_areaMap["CNC_I"] = CNC_I;
  m_areaMap["CNC_NC"] = CNC_NC;
  m_areaMap["CNC_NE"] = CNC_NE;
  m_areaMap["CNC_NW"] = CNC_NW;
  m_areaMap["CNC_SC"] = CNC_SC;
  m_areaMap["CNC_SW"] = CNC_SW;
  m_areaMap["CNC_X"] = CNC_X;
  m_areaMap["CTL_EAST"] = CTL_EAST;
  m_areaMap["CTL_NORTH"] = CTL_NORTH;
  m_areaMap["CTL_SOUTH"] = CTL_SOUTH;
  m_areaMap["CTL_WEST"] = CTL_WEST;
  m_areaMap["CTL_X"] = CTL_X;
  m_areaMap["EDU"] = EDU_AREA;
  m_areaMap["EU"] = EU_AREA;
  m_areaMap["HK"] = HK_AREA;
  m_areaMap["MOB"] = MOB_AREA;
  m_areaMap["NA"] = NA_AREA;
  m_areaMap["OA"] = OA_AREA;
  m_areaMap["PH"] = PH_AREA;
  m_areaMap["TEST"] = TEST_AREA;
  m_areaMap["TW"] = TW_AREA;
  m_areaMap["WBN"] = WBN_AREA;
}

bool IpTable::loadIspIpDataFile(string filename, string delim)
{
  std::ifstream ifile(filename.c_str());
  std::string line;
  ISPType ispType;
  AreaType areaType;
  std::string isp_str, area_str;
  int lineNum = 0;

  int fd = access(filename.c_str(), F_OK);
  if (fd != 0)
  {
    return false;
  }

  if (!ifile.good())
    return false;

  while (std::getline(ifile, line))
  {
    lineNum++;
    line = StringUtil::trim(line);
    if (line.empty())
      continue;

    std::vector<string> retVec;
    StringUtil::split(line, delim, &retVec);
    if (retVec.size() < 4)
      continue;
    
    isp_str = StringUtil::trim(retVec[2]);
    area_str = StringUtil::trim(retVec[3]);

    if (m_ispMap.find(isp_str.c_str()) == m_ispMap.end())
      continue;
    if (m_areaMap.find(area_str.c_str()) == m_areaMap.end())
      continue;

    ispType = m_ispMap[isp_str];
    areaType = m_areaMap[area_str];

    // 分析Ip范围条目 e.g. 202.99.102.128 - 202.99.102.191

    unsigned long ipFrom = ntohl(inet_addr(StringUtil::trim(retVec[0]).c_str()));
    unsigned long ipTo = ntohl(inet_addr(StringUtil::trim(retVec[1]).c_str()));

    if (ipFrom == 0xffffffff || ipTo == 0xffffffff)
      continue;
    insert(ipFrom, ipTo, ispType, areaType);
  }
  return true;
}

ISPType IpTable::getIspType(unsigned long ip)
{
  IP_TABLE_VALUE IP_T = getValue(ntohl(ip));
  if (IP_T.isp != AUTO_DETECT)
    return IP_T.isp;
  return m_defaultIsp;
}

AreaType IpTable::getAreaType(unsigned long ip)
{
  IP_TABLE_VALUE IP_T = getValue(ntohl(ip));
  if (IP_T.area != AREA_UNKNOWN)
    return IP_T.area;
  return m_defaultArea;
}

IP_TABLE_VALUE IpTable::getIpTableValue(unsigned long ip)
{
  IP_TABLE_VALUE IP_T = getValue(ntohl(ip));

  if (IP_T.isp == AUTO_DETECT)
    IP_T.isp = m_defaultIsp;

  if (IP_T.area == AREA_UNKNOWN)
    IP_T.area = m_defaultArea;

  return IP_T;
}

IpTable &IpTable::instance()
{
  static IpTable instance;
  return instance;
}

void IpTable::addNewIsp(const std::string &name, ISPType isp)
{
  m_ispMap[name] = isp;
}

void IpTable::removeIsp(const std::string &name)
{
  m_ispMap.erase(name);
}

void IpTable::addNewArea(const std::string &name, AreaType isp)
{
  m_areaMap[name] = isp;
}

void IpTable::removeArea(const std::string &name)
{
  m_areaMap.erase(name);
}

bool IpTable::isIpPartitioned(unsigned long ip)
{
  IP_TABLE_VALUE IP_T = getValue(ntohl(ip));
  return IP_T.isp != AUTO_DETECT;
}

void IpTable::setDefault(ISPType isp, AreaType area)
{
  m_defaultIsp = isp;
  m_defaultArea = area;
}

//end
} // namespace iptable
} // namespace edu
