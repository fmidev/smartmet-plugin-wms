#include "TextUtility.h"
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct TableAttributes
{
  TableAttributes();
  struct
  {
    Attributes frame;
    Attributes text;
  } title;
  struct
  {
    Attributes frame;
    std::vector<Attributes> text;
  } header;
  struct
  {
    Attributes frame;
    std::vector<Attributes> text;
  } columns;
  struct
  {
    Attributes frame;
    Attributes text;
  } footer;

  text_style_t titleTextStyle() const;
  text_style_t headerTextStyle(unsigned int col);
  text_style_t columnTextStyle(unsigned int col);
  text_style_t footerTextStyle() const;
};

struct TableCell
{
  TableCell() = default;
  TableCell(const std::string& theData, const text_style_t& theTextStyle);
  text_dimension_t dimension;
  unsigned int yoffset = 0;
  std::vector<std::string> rows;  // there can be several rows in a cell
};

using RowCellInfo = std::vector<TableCell>;

class TextTable
{
 public:
  TextTable(const text_style_t& theTextStyle, unsigned int theX, unsigned int theY);
  TextTable(const Json::Value& tableAttributes, unsigned int theX, unsigned int theY);

  void addTitle(const std::string& theData);
  void addHeader(const std::vector<std::string>& theData);
  void addFooter(const std::string& theData);
  void addData(const std::vector<std::string>& theData);
  void addContent(const Json::Value& tableDataJson);
  void makeTable(const Attributes& theFrameAttributes,
                 const Attributes& theTextAttributes,
                 CTPP::CDT& theGlobals,
                 CTPP::CDT& theLayersCdt,
                 State& theState) const;

 private:
  void addTableAttributes(const Json::Value& attributes, const std::string& tablePart);

  std::map<std::pair<unsigned int, unsigned int>, TableCell>
      itsTableContent;  // texts and dimensions
  unsigned int itsXCoord;
  unsigned int itsYCoord;
  unsigned int itsTableWidth;
  unsigned int itsDataAreaHeight;
  std::vector<unsigned int> itsColumnWidths;
  std::vector<unsigned int> itsRowHeights;
  TableCell itsTitleCellInfo;
  RowCellInfo itsHeaderCellInfo;
  TableCell itsFooterCellInfo;
  TableAttributes itsAttributes;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
