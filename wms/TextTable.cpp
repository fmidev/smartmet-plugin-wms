#include "TextTable.h"
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
text_dimension_t get_cell_dimension(const std::vector<std::string>& strings,
                                    const text_style_t& text_style)
{
  text_dimension_t cell_dimension;

  if (strings.empty())
    return cell_dimension;

  cell_dimension = getTextDimension(std::string(",jÄg"), text_style);
  cell_dimension.height *= 1.2;

  // find maximum width and height of rows
  unsigned int nRows = strings.size();
  for (unsigned int i = 0; i < nRows; i++)
  {
    std::string text = (strings[i]);
    boost::algorithm::trim(text);

    if (text.empty())
      continue;

    text_dimension_t tdim = getTextDimension(text, text_style);

    if (tdim.width * 1.2 > cell_dimension.width)
      cell_dimension.width = (tdim.width * 1.2);
    if (tdim.height * 1.2 > cell_dimension.height)
      cell_dimension.height = tdim.height * 1.2;
  }
  cell_dimension.height *= nRows;

  return cell_dimension;
}

CTPP::CDT get_rect_cdt(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  CTPP::CDT rect_cdt(CTPP::CDT::HASH_VAL);

  rect_cdt["start"] = "<rect";
  rect_cdt["end"] = "</rect>";
  rect_cdt["attributes"]["x"] = Fmi::to_string(x);
  rect_cdt["attributes"]["y"] = Fmi::to_string(y);
  rect_cdt["attributes"]["width"] = Fmi::to_string(w);
  rect_cdt["attributes"]["height"] = Fmi::to_string(h);

  return rect_cdt;
}

text_dimension_t get_row_dimension(const RowCellInfo& rci)
{
  text_dimension_t ret;
  for (auto ci : rci)
  {
    ret.width += ci.dimension.width;
    if (ci.dimension.height > ret.height)
      ret.height = ci.dimension.height;
  }

  return ret;
}
}  // namespace

text_style_t TableAttributes::titleTextStyle()
{
  return getTextStyle(title.text, text_style_t());
}

text_style_t TableAttributes::headerTextStyle(unsigned int col)
{
  unsigned int nColumns = columns.text.size();
  if (col < nColumns)
    return getTextStyle(header.text[col], text_style_t());
  else
    return getTextStyle(header.text[nColumns - 1], text_style_t());
}

text_style_t TableAttributes::columnTextStyle(unsigned int col)
{
  unsigned int nColumns = columns.text.size();
  if (col < nColumns)
    return getTextStyle(columns.text[col], text_style_t());
  else
    return getTextStyle(columns.text[nColumns - 1], text_style_t());
}

text_style_t TableAttributes::footerTextStyle()
{
  return getTextStyle(footer.text, text_style_t());
}

void TextTable::addTableAttributes(const Json::Value& attributes, std::string tablePart)
{
  Json::Value nulljson;

  auto json = attributes.get(tablePart, nulljson);

  if (!json.isNull())
  {
    auto frameJson = json.get("frame", nulljson);
    if (!frameJson.isNull())
    {
      Json::Value::Members members = frameJson.getMemberNames();
      for (auto name : members)
      {
        auto jsonValue = frameJson.get(name, nulljson);
        if (!jsonValue.isNull())
        {
          if (tablePart == "title")
            itsAttributes.title.frame.add(name, jsonValue.asString());
          else if (tablePart == "footer")
            itsAttributes.footer.frame.add(name, jsonValue.asString());
          else if (tablePart == "header")
            itsAttributes.header.frame.add(name, jsonValue.asString());
          else if (tablePart == "columns")
            itsAttributes.columns.frame.add(name, jsonValue.asString());
        }
      }
    }
    auto textJson = json.get("text", nulljson);
    if (tablePart == "title" || tablePart == "footer")
    {
      if (!textJson.isNull())
      {
        Json::Value::Members members = textJson.getMemberNames();
        for (auto name : members)
        {
          auto jsonValue = textJson.get(name, nulljson);
          if (!jsonValue.isNull())
          {
            if (tablePart == "title")
              itsAttributes.title.text.add(name, jsonValue.asString());
            else
              itsAttributes.footer.text.add(name, jsonValue.asString());
          }
        }
      }
    }
    else if (tablePart == "header" || tablePart == "columns")
    {
      if (textJson.isArray())
      {
        if (tablePart == "header")
          itsAttributes.header.text.clear();
        else
          itsAttributes.columns.text.clear();

        for (unsigned int i = 0; i < textJson.size(); i++)
        {
          const Json::Value& textJs = textJson[i];
          if (!textJs.isNull())
          {
            Json::Value::Members members = textJs.getMemberNames();
            Attributes textAttributes;
            for (auto name : members)
            {
              auto jsonValue = textJs.get(name, nulljson);
              if (!jsonValue.isNull())
                textAttributes.add(name, jsonValue.asString());
            }
            if (tablePart == "header")
              itsAttributes.header.text.push_back(textAttributes);
            else
              itsAttributes.columns.text.push_back(textAttributes);
          }
        }
      }
      else
      {
        throw Spine::Exception(
            BCP, "TextTable: Error: text-attribute of header and columns must be an array!");
      }
    }
  }
}

TextTable::TextTable(const text_style_t& theTextStyle, unsigned int theX, unsigned int theY)
    : itsXCoord(theX), itsYCoord(theY), itsTableWidth(0), itsDataAreaHeight(0)
{
}

TextTable::TextTable(const Json::Value& tableAttributes, unsigned int theX, unsigned int theY)
    : itsXCoord(theX), itsYCoord(theY), itsTableWidth(0), itsDataAreaHeight(0)
{
  try
  {
    if (tableAttributes.isNull())
      return;

    addTableAttributes(tableAttributes, "title");
    addTableAttributes(tableAttributes, "header");
    addTableAttributes(tableAttributes, "columns");
    addTableAttributes(tableAttributes, "footer");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "TextTable: Error parsing configuration file!");
  }
}

void TextTable::addTitle(const std::string& theData)
{
  boost::algorithm::split(itsTitleCellInfo.rows, theData, boost::algorithm::is_any_of("\n"));
  itsTitleCellInfo.dimension =
      get_cell_dimension(itsTitleCellInfo.rows, itsAttributes.titleTextStyle());
  itsTitleCellInfo.yoffset = 0;
  if (itsTitleCellInfo.dimension.width > itsTableWidth)
    itsTableWidth = itsTitleCellInfo.dimension.width;
}

void TextTable::addFooter(const std::string& theData)
{
  boost::algorithm::split(itsFooterCellInfo.rows, theData, boost::algorithm::is_any_of("\n"));
  itsFooterCellInfo.dimension =
      get_cell_dimension(itsFooterCellInfo.rows, itsAttributes.footerTextStyle());
  itsFooterCellInfo.yoffset = 0;
  if (itsFooterCellInfo.dimension.width > itsTableWidth)
    itsTableWidth = itsFooterCellInfo.dimension.width;
}

void TextTable::addHeader(const std::vector<std::string>& theData)
{
  unsigned int rowWidth = 0;
  for (unsigned int column = 0; column < theData.size(); column++)
  {
    const std::string& field = theData[column];
    TableCell cellInfo(field, itsAttributes.headerTextStyle(column));
    itsHeaderCellInfo.push_back(cellInfo);
    rowWidth += cellInfo.dimension.width;
    if (column < itsColumnWidths.size())
    {
      if (cellInfo.dimension.width > itsColumnWidths[column])
        itsColumnWidths[column] = cellInfo.dimension.width;
    }
    else
    {
      itsColumnWidths.push_back(cellInfo.dimension.width);
    }
  }
  if (rowWidth > itsTableWidth)
    itsTableWidth = rowWidth;
}

void TextTable::addData(const std::vector<std::string>& theData)
{
  unsigned int rowHeightMax = 0;
  unsigned int row = itsRowHeights.size();
  for (unsigned int column = 0; column < theData.size(); column++)
  {
    const std::string& field = theData[column];
    TableCell cellInfo;
    boost::algorithm::split(cellInfo.rows, field, boost::algorithm::is_any_of("\n"));
    cellInfo.dimension = get_cell_dimension(cellInfo.rows, itsAttributes.columnTextStyle(column));
    cellInfo.yoffset = itsDataAreaHeight + (cellInfo.dimension.height / cellInfo.rows.size());
    itsTableContent.insert(
        std::make_pair(std::pair<unsigned int, unsigned int>(row, column), cellInfo));
    if (cellInfo.dimension.height > rowHeightMax)
      rowHeightMax = cellInfo.dimension.height;
    if (row == 0 && column >= itsColumnWidths.size())  // first row
      itsColumnWidths.push_back(cellInfo.dimension.width);
    if (cellInfo.dimension.width > itsColumnWidths[column])
      itsColumnWidths[column] = cellInfo.dimension.width;
  }
  itsDataAreaHeight += rowHeightMax;
  itsRowHeights.push_back(rowHeightMax);

  unsigned int rowWidth = std::accumulate(itsColumnWidths.begin(), itsColumnWidths.end(), 0);
  if (rowWidth > itsTableWidth)
    itsTableWidth = rowWidth;
}

void TextTable::addContent(const Json::Value& tableDataJson)
{
  if (tableDataJson.isNull())
    return;

  using TableRow = std::vector<std::string>;
  Json::Value json, nulljson;
  // add title
  json = tableDataJson.get("title", nulljson);
  if (!json.isNull())
    addTitle(json.asString());

  // add footer
  json = tableDataJson.get("footer", nulljson);
  if (!json.isNull())
    addFooter(json.asString());

  // add header
  auto headerJson = tableDataJson.get("header", nulljson);
  if (!headerJson.isNull())
  {
    TableRow header;
    for (unsigned int i = 0; i < 256; i++)
    {
      json = headerJson.get("column" + Fmi::to_string(i), nulljson);
      if (json.isNull())
        break;
      header.push_back(json.asString());
    }
    if (header.size() > 0)
      addHeader(header);
  }

  // add data
  auto dataJson = tableDataJson.get("data", nulljson);
  if (!dataJson.isNull())
  {
    if (!dataJson.isArray())
      throw Spine::Exception(
          BCP, "IceMapLayer: Error in configuration: table_data.data attribute must be array!");

    for (unsigned int i = 0; i < dataJson.size(); i++)
    {
      auto rowJson = dataJson[i];
      if (!rowJson.isNull())
      {
        TableRow dataRow;
        for (unsigned int j = 0; j < 256; j++)
        {
          json = rowJson.get("column" + Fmi::to_string(j), nulljson);
          if (json.isNull())
            break;
          dataRow.push_back(json.asString());
        }
        if (dataRow.size() > 0)
          addData(dataRow);
      }
    }
  }
}

void TextTable::makeTable(const Attributes& theFrameAttributes,
                          const Attributes& theTextAttributes,
                          CTPP::CDT& theGlobals,
                          CTPP::CDT& theLayersCdt,
                          State& theState) const
{
  if (!itsTitleCellInfo.rows.empty())
  {
    // background rectangle for title
    CTPP::CDT rect_cdt =
        get_rect_cdt(itsXCoord, itsYCoord, itsTableWidth, itsTitleCellInfo.dimension.height);
    theState.addAttributes(theGlobals, rect_cdt, itsAttributes.title.frame);
    theLayersCdt.PushBack(rect_cdt);

    // title text
    addTextField(itsXCoord,
                 itsYCoord + (itsTitleCellInfo.dimension.height / itsTitleCellInfo.rows.size()) - 5,
                 itsTableWidth,
                 itsTitleCellInfo.rows,
                 itsAttributes.title.text,
                 theGlobals,
                 theLayersCdt,
                 theState);
  }

  text_dimension_t header_dimension = get_row_dimension(itsHeaderCellInfo);
  if (!itsHeaderCellInfo.empty())
  {
    // background rectangle for header
    CTPP::CDT rect_cdt = get_rect_cdt(itsXCoord,
                                      itsYCoord + itsTitleCellInfo.dimension.height,
                                      itsTableWidth,
                                      header_dimension.height);
    theState.addAttributes(theGlobals, rect_cdt, itsAttributes.header.frame);
    theLayersCdt.PushBack(rect_cdt);

    // vertical lines
    unsigned int xoffset = 0;
    for (unsigned int i = 0; i < itsColumnWidths.size(); i++)
    {
      unsigned int x = itsXCoord + xoffset;
      unsigned int y1 = itsYCoord + itsTitleCellInfo.dimension.height;
      unsigned int y2 = itsYCoord + itsTitleCellInfo.dimension.height + header_dimension.height;
      if (i < itsColumnWidths.size() - 1)
      {
        CTPP::CDT line_cdt(CTPP::CDT::HASH_VAL);
        line_cdt["start"] = "<line";
        line_cdt["end"] = "</line>";
        line_cdt["attributes"]["x1"] = Fmi::to_string(x + itsColumnWidths[i]);
        line_cdt["attributes"]["y1"] = Fmi::to_string(y1);
        line_cdt["attributes"]["x2"] = Fmi::to_string(x + itsColumnWidths[i]);
        line_cdt["attributes"]["y2"] = Fmi::to_string(y2);
        theState.addAttributes(theGlobals, line_cdt, itsAttributes.columns.frame);
        theLayersCdt.PushBack(line_cdt);
      }
      const TableCell cell = itsHeaderCellInfo[i];

      unsigned int attributeIndex =
          (i < itsAttributes.header.text.size() ? i : itsAttributes.header.text.size() - 1);

      addTextField(x + 2,
                   y1 + (header_dimension.height / cell.rows.size()) - 5,
                   itsColumnWidths[i],
                   cell.rows,
                   itsAttributes.header.text[attributeIndex],
                   theGlobals,
                   theLayersCdt,
                   theState);

      xoffset += itsColumnWidths[i];
    }
  }

  // background rectangle for table
  CTPP::CDT background_rect_cdt(CTPP::CDT::HASH_VAL);
  background_rect_cdt["start"] = "<rect";
  background_rect_cdt["end"] = "</rect>";
  background_rect_cdt["attributes"]["width"] = Fmi::to_string(itsTableWidth);
  background_rect_cdt["attributes"]["height"] = Fmi::to_string(itsDataAreaHeight);
  background_rect_cdt["attributes"]["x"] = Fmi::to_string(itsXCoord);
  background_rect_cdt["attributes"]["y"] =
      Fmi::to_string(itsYCoord + itsTitleCellInfo.dimension.height + header_dimension.height);
  theState.addAttributes(theGlobals, background_rect_cdt, itsAttributes.columns.frame);
  theLayersCdt.PushBack(background_rect_cdt);

  // horizontal lines
  unsigned int nRows = itsRowHeights.size();
  unsigned int nColumns = itsColumnWidths.size();
  unsigned int yoffset = 0;
  for (unsigned int i = 0; i < nRows - 1; i++)
  {
    yoffset += itsRowHeights[i];
    unsigned int x1 = itsXCoord;
    unsigned int x2 = itsXCoord + itsTableWidth;
    unsigned int y =
        itsYCoord + yoffset + itsTitleCellInfo.dimension.height + header_dimension.height;
    CTPP::CDT line_cdt(CTPP::CDT::HASH_VAL);
    line_cdt["start"] = "<line";
    line_cdt["end"] = "</line>";
    line_cdt["attributes"]["x1"] = Fmi::to_string(x1);
    line_cdt["attributes"]["y1"] = Fmi::to_string(y);
    line_cdt["attributes"]["x2"] = Fmi::to_string(x2);
    line_cdt["attributes"]["y2"] = Fmi::to_string(y);
    theState.addAttributes(theGlobals, line_cdt, itsAttributes.columns.frame);
    theLayersCdt.PushBack(line_cdt);
  }

  // vertical lines
  unsigned int xoffset = 0;
  for (unsigned int i = 0; i < nColumns - 1; i++)
  {
    xoffset += itsColumnWidths[i];
    unsigned int x = itsXCoord + xoffset;
    unsigned int y1 = itsYCoord + itsTitleCellInfo.dimension.height + header_dimension.height;
    unsigned int y2 =
        itsYCoord + itsTitleCellInfo.dimension.height + header_dimension.height + itsDataAreaHeight;
    CTPP::CDT line_cdt(CTPP::CDT::HASH_VAL);
    line_cdt["start"] = "<line";
    line_cdt["end"] = "</line>";
    line_cdt["attributes"]["x1"] = Fmi::to_string(x);
    line_cdt["attributes"]["y1"] = Fmi::to_string(y1);
    line_cdt["attributes"]["x2"] = Fmi::to_string(x);
    line_cdt["attributes"]["y2"] = Fmi::to_string(y2);
    theState.addAttributes(theGlobals, line_cdt, itsAttributes.columns.frame);
    theLayersCdt.PushBack(line_cdt);
  }

  // table texts
  for (unsigned int row = 0; row < nRows; row++)
  {
    unsigned int xoffset = 0;
    for (unsigned int column = 0; column < nColumns; column++)
    {
      const Attributes& textAttributes =
          (column < itsAttributes.columns.text.size() ? itsAttributes.columns.text[column]
                                                      : itsAttributes.columns.text.back());
      const TableCell& cell =
          itsTableContent.at(std::pair<unsigned int, unsigned int>(row, column));
      addTextField(itsXCoord + xoffset,
                   itsYCoord + cell.yoffset + itsTitleCellInfo.dimension.height +
                       header_dimension.height - 5,
                   itsColumnWidths[column],
                   cell.rows,
                   textAttributes,
                   theGlobals,
                   theLayersCdt,
                   theState);
      xoffset += itsColumnWidths[column];
    }
  }

  if (!itsFooterCellInfo.rows.empty())
  {
    // background rectangle for footer
    CTPP::CDT background_rect_cdt(CTPP::CDT::HASH_VAL);
    background_rect_cdt["start"] = "<rect";
    background_rect_cdt["end"] = "</rect>";
    background_rect_cdt["attributes"]["width"] = Fmi::to_string(itsTableWidth);
    background_rect_cdt["attributes"]["height"] =
        Fmi::to_string(itsFooterCellInfo.dimension.height);
    background_rect_cdt["attributes"]["x"] = Fmi::to_string(itsXCoord);
    background_rect_cdt["attributes"]["y"] =
        Fmi::to_string(itsYCoord + itsDataAreaHeight + itsTitleCellInfo.dimension.height +
                       header_dimension.height);
    theState.addAttributes(theGlobals, background_rect_cdt, itsAttributes.footer.frame);
    //    theState.addAttributes(theGlobals, background_rect_cdt, theFrameAttributes);
    theLayersCdt.PushBack(background_rect_cdt);

    // footer text
    addTextField(itsXCoord,
                 itsYCoord + itsDataAreaHeight + itsTitleCellInfo.dimension.height +
                     header_dimension.height +
                     (itsFooterCellInfo.dimension.height / itsFooterCellInfo.rows.size()) - 5,
                 itsTableWidth,
                 itsFooterCellInfo.rows,
                 itsAttributes.footer.text,
                 theGlobals,
                 theLayersCdt,
                 theState);
  }
}

TableCell::TableCell(const std::string& theData, const text_style_t& theTextStyle)
{
  boost::algorithm::split(rows, theData, boost::algorithm::is_any_of("\n"));
  dimension = get_cell_dimension(rows, theTextStyle);
  yoffset = 0;
}

TableAttributes::TableAttributes()
{
  // title
  title.frame.add("fill", "white");
  title.frame.add("stroke", "black");
  title.frame.add("stroke-width", "0.5");
  title.text.add("font-family", "Arial");
  title.text.add("font-size", "14");
  title.text.add("font-style", "normal");
  title.text.add("font-weight", "normal");
  title.text.add("text-anchor", "middle");

  // header
  header.frame = title.frame;
  Attributes col1;  // #1 column
  col1.add("font-family", "Arial");
  col1.add("font-size", "14");
  col1.add("font-style", "normal");
  col1.add("font-weight", "normal");
  col1.add("text-anchor", "left");
  Attributes col2;  // #2 column and later
  col2.add("font-family", "Arial");
  col2.add("font-size", "14");
  col2.add("font-style", "normal");
  col2.add("font-weight", "normal");
  col2.add("text-anchor", "middle");
  header.text.push_back(col1);
  header.text.push_back(col2);

  // footer
  footer.frame = title.frame;
  footer.text.add("font-family", "Arial");
  footer.text.add("font-size", "14");
  footer.text.add("font-style", "normal");
  footer.text.add("font-weight", "normal");
  footer.text.add("text-anchor", "left");

  // data columns (use same attributes as header)
  columns.frame = header.frame;
  col1.add("font-family", "Arial");
  col1.add("font-size", "14");
  col1.add("font-style", "normal");
  col1.add("font-weight", "normal");
  col1.add("text-anchor", "left");
  col2.add("font-family", "Arial");
  col2.add("font-size", "14");
  col2.add("font-style", "normal");
  col2.add("font-weight", "normal");
  col2.add("text-anchor", "middle");
  columns.text.push_back(col1);
  columns.text.push_back(col2);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
