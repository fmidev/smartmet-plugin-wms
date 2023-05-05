#include "WMSLegendGraphicInfo.h"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi)
{
  ost << "LegendGraphicInfo:\n";
  if(lgi.info.empty())
	ost << " info: -";
  else
	{
	  ost << " info: \n";
	  for(const auto& item : lgi.info)
		ost << " " << item.first << " ->\n" << item.second.toStyledString() << std::endl;
	}
  if(lgi.text_lengths.empty())
	ost << " text_lengths: -";
  else
	{
	  ost << " text_lengths: \n";
	  for(const auto& item : lgi.text_lengths)
		ost << " " << item.first << ": " << item.second << std::endl;
	}

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const NamedLegendGraphicInfo& nlgi)
{
  if(nlgi.empty())
	ost << "NamedLegendGraphicInfo: - \n";
  else
	{
	  ost << "NamedLegendGraphicInfo:\n";
	  for(const auto& item : nlgi)
		{
		  ost << " " << item.first << ":\n";
		  for(const auto& item2 : item.second)
			ost << item2 << std::endl;
		}
	}

  return ost;
}
  
std::ostream& operator<<(std::ostream& ost, const LegendGraphicResultPerLanguage& lgr)
{
  if(lgr.empty())
	ost << "LegendGraphicResultPerLanguage: - \n";
  else
	{
	  ost << "LegendGraphicResultPerLanguage:\n";
	  for(const auto& item : lgr)
		{
		  ost << " " << item.first << ":\n";
		  ost << " width: " << item.second.width << std::endl;
		  ost << " height: " << item.second.height << std::endl;
		  for(const auto& item2 : item.second.legendLayers)
			{
			  ost << " legendLayer: " << item2 << std::endl;
			}
		}
	}

  return ost;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
