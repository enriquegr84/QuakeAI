/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "UITable.h"

#include "UISkin.h"
#include "UIFont.h"

#include "Core/OS/OS.h"

#include "Application/System/System.h"
#include "Application/Settings.h"

/*
    UITable
*/

UITable::UITable(BaseUI* ui, int id, RectangleShape<2, int> rectangle) : 
    BaseUITable(id, rectangle), mUI(ui)
{
    mBlendState = std::make_shared<BlendState>();
    mBlendState->mTarget[0].enable = true;
    mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    //basic visual effect
    {
        // Create a vertex buffer for a single triangle.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/ColorEffectVS.glsl");
        path.push_back("Effects/ColorEffectPS.glsl");
#else
        path.push_back("Effects/ColorEffectVS.hlsl");
        path.push_back("Effects/ColorEffectPS.hlsl");
#endif
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extra =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extra->GetProgram())
            extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        std::shared_ptr<VisualEffect> effect =
            std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

        // Create the geometric object for drawing.
        mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

    std::shared_ptr<ResHandle>& resHandle =
        ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
    if (resHandle)
    {
        const std::shared_ptr<ImageResourceExtraData>& extra =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        extra->GetImage()->AutogenerateMipmaps();

        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
        resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

        const std::shared_ptr<ShaderResourceExtraData>& extraRes =
            std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
        if (!extraRes->GetProgram())
            extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        mEffect = std::make_shared<Texture2Effect>(
            ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
    }
}

UITable::~UITable()
{
    for (UITable::Row &row : mRows)
        delete[] row.cells;
}

void UITable::OnInit()
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    mFont = skin->GetFont();
    if (mFont)
    {
        mRowHeight = mFont->GetDimension(L"Ay")[1] + 4;
        mRowHeight = std::max(mRowHeight, 1);
    }

    const int s = skin->GetSize(DS_SCROLLBAR_SIZE);

    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ s, mRelativeRect.mExtent[1] };
    rect.mCenter = Vector2<int>{
        mRelativeRect.mExtent[0] - (int)round(s / 2.f),
        mRelativeRect.mExtent[1] / 2 };

    mScrollbar = mUI->AddScrollBar(false, true, rect, shared_from_this());
    mScrollbar->SetSubElement(true);
    mScrollbar->SetTabStop(false);
    mScrollbar->SetAlignment(UIA_LOWERRIGHT, UIA_LOWERRIGHT, UIA_UPPERLEFT, UIA_LOWERRIGHT);
    mScrollbar->SetVisible(false);
    mScrollbar->SetPosition(0);

    SetTabStop(true);
    SetTabOrder(-1);
    UpdateAbsolutePosition();

    System* system = System::Get();
    RectangleShape<2, int> relativeRect = mScrollbar->GetRelativePosition();
    int width = (int)((relativeRect.mExtent[0] / (2.f / 3.f)) * 
        system->GetDisplayDensity() * Settings::Get()->GetFloat("gui_scaling"));

    rect.mExtent = Vector2<int>{ width, relativeRect.mExtent[1] };
    rect.mCenter = Vector2<int>{
        relativeRect.GetVertice(RVP_LOWERRIGHT)[0] - (int)round(width / 2.f),
        relativeRect.GetVertice(RVP_UPPERLEFT)[1] + rect.mExtent[1] / 2
    };
    mScrollbar->SetRelativePosition(rect);
}

UITable::Option UITable::SplitOption(const std::string &str)
{
    size_t equalPos = str.find('=');
    if (equalPos == std::string::npos)
        return UITable::Option(str, "");

    return UITable::Option(str.substr(0, equalPos), str.substr(equalPos + 1));
}

void UITable::SetTextList(const std::vector<std::string> &content, bool transparent)
{
    Clear();

    if (transparent) 
    {
        mBackground.SetAlpha(0);
        mBorder = false;
    }

    mIsTextlist = true;
    int emptyStringIndex = AllocString("");

    mRows.resize(content.size());
    for (int i = 0; i < (int)content.size(); ++i) 
    {
        Row *row = &mRows[i];
        row->cells = new Cell[1];
        row->cellcount = 1;
        row->indent = 0;
        row->visibleIndex = i;
        mVisibleRows.push_back(i);

        Cell *cell = row->cells;
        cell->xmin = 0;
        cell->xmax = 0x7fff;  // something large enough
        cell->xpos = 6;
        cell->contentType = COLUMN_TYPE_TEXT;
        cell->contentIndex = emptyStringIndex;
        cell->tooltipIndex = emptyStringIndex;
        cell->color.Set(255, 255, 255, 255);
        cell->colorDefined = false;
        cell->reportedColumn = 1;

        // parse row content (color)
        const std::string &s = content[i];
        if (s[0] == '#' && s[1] == '#') 
        {
            // double # to escape
            cell->contentIndex = AllocString(s.substr(2));
        }
        else if (s[0] == '#' && s.size() >= 7 &&
            ParseColorString(s.substr(0, 7), cell->color, false)) 
        {
            // single # for color
            cell->colorDefined = true;
            cell->contentIndex = AllocString(s.substr(7));
        }
        else 
        {
            // no #, just text
            cell->contentIndex = AllocString(s);
        }

    }

    AllocationComplete();

    // Clamp scroll bar position
    UpdateScrollBar();
}

void UITable::SetTable(const TableOptions &options,
    const TableColumns &columns, std::vector<std::string> &content)
{
    Clear();

    // Naming conventions:
    // i is always a row index, 0-based
    // j is always a column index, 0-based
    // k is another index, for example an option index

    // Handle a stupid error case... (issue #1187)
    if (columns.empty()) 
    {
        TableColumn textColumn;
        textColumn.mType = "text";
        TableColumns newColumns;
        newColumns.push_back(textColumn);
        SetTable(options, newColumns, content);
        return;
    }

    // Handle table options
    SColor defaultColor(255, 255, 255, 255);
    int opendepth = 0;
    for (const Option &option : options) 
    {
        const std::string &name = option.mName;
        const std::string &value = option.mValue;
        if (name == "color")
            ParseColorString(value, mColor, false);
        else if (name == "background")
            ParseColorString(value, mBackground, false);
        else if (name == "border")
            mBorder = IsYes(value);
        else if (name == "highlight")
            ParseColorString(value, mHighlight, false);
        else if (name == "highlight_text")
            ParseColorString(value, mHighlightText, false);
        else if (name == "opendepth")
            opendepth = atoi(value.c_str());
        else
            LogError("Invalid table option: \"" + name + "\"" + " (value=\"" + value + "\")");
    }

    // Get number of columns and rows
    // note: error case columns.size() == 0 was handled above
    int colcount = (int)columns.size();
    LogAssert(colcount >= 1, "invalid number of columns");
    // rowcount = ceil(cellcount / colcount) but use integer arithmetic
    int rowcount = ((int)content.size() + colcount - 1) / colcount;
    LogAssert(rowcount >= 0, "invalid number of rows");
    // Append empty strings to content if there is an incomplete row
    int cellcount = rowcount * colcount;
    while (content.size() < (unsigned int)cellcount)
        content.emplace_back("");

    // Create temporary rows (for processing columns)
    struct TempRow 
    {
        // Current horizontal position (may different between rows due
        // to indent/tree columns, or text/image columns with width<0)
        int x;
        // Tree indentation level
        int indent;
        // Next cell: Index into mStrings or mImages
        int contentIndex;
        // Next cell: Width in pixels
        int contentWidth;
        // Vector of completed cells in this row
        std::vector<Cell> cells;
        // Stores colors and how long they last (maximum column index)
        std::vector<std::pair<SColor, int> > colors;

        TempRow() : x(0), indent(0), contentIndex(0), contentWidth(0) {}
    };
    TempRow *rows = new TempRow[rowcount];

    // Get em width. Pedantically speaking, the width of "M" is not
    // necessarily the same as the em width, but whatever, close enough.
    int em = 6;
    if (mFont)
        em = mFont->GetDimension(L"M")[0];

    int defaultTooltipIndex = AllocString("");

    std::map<int, int> activeImageIndices;

    // Process content in column-major order
    for (int j = 0; j < colcount; ++j) 
    {
        // Check column type
        ColumnType columntype = COLUMN_TYPE_TEXT;
        if (columns[j].mType == "text")
            columntype = COLUMN_TYPE_TEXT;
        else if (columns[j].mType == "image")
            columntype = COLUMN_TYPE_IMAGE;
        else if (columns[j].mType == "color")
            columntype = COLUMN_TYPE_COLOR;
        else if (columns[j].mType == "indent")
            columntype = COLUMN_TYPE_INDENT;
        else if (columns[j].mType == "tree")
            columntype = COLUMN_TYPE_TREE;
        else
            LogError("Invalid table column type: \"" + columns[j].mType + "\"");

        // Process column options
        int padding = (int)round(0.5 * em);
        int tooltipIndex = defaultTooltipIndex;
        int align = 0;
        int width = 0;
        int span = colcount;

        if (columntype == COLUMN_TYPE_INDENT) 
        {
            padding = 0; // default indent padding
        }
        if (columntype == COLUMN_TYPE_INDENT || columntype == COLUMN_TYPE_TREE) 
        {
            width = (int)round(em * 1.5); // default indent width
        }

        for (const Option &option : columns[j].mSettings) 
        {
            const std::string &name = option.mName;
            const std::string &value = option.mValue;
            if (name == "padding")
                padding = (int)(round(atof(value.c_str()) * em));
            else if (name == "tooltip")
                tooltipIndex = AllocString(value);
            else if (name == "align" && value == "left")
                align = 0;
            else if (name == "align" && value == "center")
                align = 1;
            else if (name == "align" && value == "right")
                align = 2;
            else if (name == "align" && value == "inline")
                align = 3;
            else if (name == "width")
                width = (int)(round(atof(value.c_str()) * em));
            else if (name == "span" && columntype == COLUMN_TYPE_COLOR)
                span = atoi(value.c_str());
            else if (columntype == COLUMN_TYPE_IMAGE &&
                !name.empty() && StringAllowed(name, "0123456789")) 
            {
                int contentIndex = AllocImage(value);
                activeImageIndices.insert(std::make_pair(atoi(name.c_str()), contentIndex));
            }
            else 
            {
                LogError("Invalid table column option: \"" + name + "\"" + 
                    " (value=\"" + value + "\")");
            }
        }

        // If current column type can use information from "color" columns,
        // find out which of those is currently active
        if (columntype == COLUMN_TYPE_TEXT) 
        {
            for (int i = 0; i < rowcount; ++i) 
            {
                TempRow *row = &rows[i];
                while (!row->colors.empty() && row->colors.back().second < j)
                    row->colors.pop_back();
            }
        }

        // Make template for new cells
        Cell newcell;
        newcell.contentType = columntype;
        newcell.tooltipIndex = tooltipIndex;
        newcell.reportedColumn = j + 1;

        if (columntype == COLUMN_TYPE_TEXT) 
        {
            // Find right edge of column
            int xmax = 0;
            for (int i = 0; i < rowcount; ++i) 
            {
                TempRow *row = &rows[i];
                row->contentIndex = AllocString(content[i * colcount + j]);
                const std::wstring &text = mStrings[row->contentIndex];
                row->contentWidth = mFont ? mFont->GetDimension(text)[0] : 0;
                row->contentWidth = std::max(row->contentWidth, width);
                int row_xmax = row->x + padding + row->contentWidth;
                xmax = std::max(xmax, row_xmax);
            }
            // Add a new cell (of text type) to each row
            for (int i = 0; i < rowcount; ++i) 
            {
                newcell.xmin = rows[i].x + padding;
                AlignContent(&newcell, xmax, rows[i].contentWidth, align);
                newcell.contentIndex = rows[i].contentIndex;
                newcell.colorDefined = !rows[i].colors.empty();
                if (newcell.colorDefined)
                    newcell.color = rows[i].colors.back().first;
                rows[i].cells.push_back(newcell);
                rows[i].x = newcell.xmax;
            }
        }
        else if (columntype == COLUMN_TYPE_IMAGE) 
        {
            // Find right edge of column
            int xmax = 0;
            for (int i = 0; i < rowcount; ++i) 
            {
                TempRow *row = &rows[i];
                row->contentIndex = -1;

                // Find contentIndex. Image indices are defined in
                // column options so check activeImageIndices.
                int imageIndex = atoi(content[i * colcount + j].c_str());
                std::map<int, int>::iterator imageIter =
                    activeImageIndices.find(imageIndex);
                if (imageIter != activeImageIndices.end())
                    row->contentIndex = imageIter->second;

                // Get texture object (might be NULL)
                std::shared_ptr<Texture2> image = nullptr;
                if (row->contentIndex >= 0)
                    image = mImages[row->contentIndex];

                // Get content width and update xmax
                row->contentWidth = image ? image->GetDimension(0) : 0;
                row->contentWidth = std::max(row->contentWidth, width);
                int row_xmax = row->x + padding + row->contentWidth;
                xmax = std::max(xmax, row_xmax);
            }
            // Add a new cell (of image type) to each row
            for (int i = 0; i < rowcount; ++i) 
            {
                newcell.xmin = rows[i].x + padding;
                AlignContent(&newcell, xmax, rows[i].contentWidth, align);
                newcell.contentIndex = rows[i].contentIndex;
                rows[i].cells.push_back(newcell);
                rows[i].x = newcell.xmax;
            }
            activeImageIndices.clear();
        }
        else if (columntype == COLUMN_TYPE_COLOR) 
        {
            for (int i = 0; i < rowcount; ++i) 
            {
                SColor cellcolor(255, 255, 255, 255);
                if (ParseColorString(content[i * colcount + j], cellcolor, true))
                    rows[i].colors.emplace_back(cellcolor, j + span);
            }
        }
        else if (columntype == COLUMN_TYPE_INDENT ||
            columntype == COLUMN_TYPE_TREE) {
            // For column type "tree", reserve additional space for +/-
            // Also enable special processing for treeview-type tables
            int contentWidth = 0;
            if (columntype == COLUMN_TYPE_TREE) 
            {
                contentWidth = mFont ? mFont->GetDimension(L"+")[0] : 0;
                mHasTreeColumn = true;
            }
            // Add a new cell (of indent or tree type) to each row
            for (int i = 0; i < rowcount; ++i) 
            {
                TempRow *row = &rows[i];

                int indentlevel = atoi(content[i * colcount + j].c_str());
                indentlevel = std::max(indentlevel, 0);
                if (columntype == COLUMN_TYPE_TREE)
                    row->indent = indentlevel;

                newcell.xmin = row->x + padding;
                newcell.xpos = newcell.xmin + indentlevel * width;
                newcell.xmax = newcell.xpos + contentWidth;
                newcell.contentIndex = 0;
                newcell.colorDefined = !rows[i].colors.empty();
                if (newcell.colorDefined)
                    newcell.color = rows[i].colors.back().first;
                row->cells.push_back(newcell);
                row->x = newcell.xmax;
            }
        }
    }

    // Copy temporary rows to not so temporary rows
    if (rowcount >= 1) 
    {
        mRows.resize(rowcount);
        for (int i = 0; i < rowcount; ++i) 
        {
            Row *row = &mRows[i];
            row->cellcount = (int)rows[i].cells.size();
            row->cells = new Cell[row->cellcount];
            memcpy((void*)row->cells, (void*)&rows[i].cells[0], row->cellcount * sizeof(Cell));
            row->indent = rows[i].indent;
            row->visibleIndex = i;
            mVisibleRows.push_back(i);
        }
    }

    if (mHasTreeColumn) 
    {
        // Treeview: convert tree to indent cells on leaf rows
        for (int i = 0; i < rowcount; ++i) 
        {
            if (i == rowcount - 1 || mRows[i].indent >= mRows[i + 1].indent)
                for (int j = 0; j < mRows[i].cellcount; ++j)
                    if (mRows[i].cells[j].contentType == COLUMN_TYPE_TREE)
                        mRows[i].cells[j].contentType = COLUMN_TYPE_INDENT;
        }

        // Treeview: close rows according to opendepth option
        std::set<int> mOpenedTrees;
        for (int i = 0; i < rowcount; ++i)
            if (mRows[i].indent < opendepth)
                mOpenedTrees.insert(i);
        SetOpenedTrees(mOpenedTrees);
    }

    // Delete temporary information used only during SetTable()
    delete[] rows;
    AllocationComplete();

    // Clamp scroll bar position
    UpdateScrollBar();
}

void UITable::Clear()
{
    // Clean up cells and rows
    for (UITable::Row &row : mRows)
        delete[] row.cells;
    mRows.clear();
    mVisibleRows.clear();

    // Get colors from skin
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    mColor = skin->GetColor(DC_BUTTON_TEXT);
    mBackground = skin->GetColor(DC_3D_HIGH_LIGHT);
    mHighlight = skin->GetColor(DC_HIGH_LIGHT);
    mHighlightText = skin->GetColor(DC_HIGH_LIGHT_TEXT);

    // Reset members
    mIsTextlist = false;
    mHasTreeColumn = false;
    mSelected = -1;
    mSelColumn = 0;
    mSelDoubleclick = false;
    mKeynavTime = 0;
    mKeynavBuffer = L"";
    mBorder = true;
    mStrings.clear();
    mImages.clear();
    mAllocStrings.clear();
    mAllocImages.clear();
}

std::string UITable::CheckEvent()
{
    int sel = GetSelected();
    LogAssert(sel >= 0, "invalid selected data");

    if (sel == 0) 
        return "INV";

    std::ostringstream os(std::ios::binary);
    if (mSelDoubleclick) 
    {
        os << "DCL:";
        mSelDoubleclick = false;
    }
    else 
    {
        os << "CHG:";
    }

    os << sel;
    if (!mIsTextlist) 
    {
        os << ":" << mSelColumn;
    }

    os << " CNT:";
    Row* row = &mRows[mVisibleRows[sel-1]];
    for (int j = 0; j < row->cellcount; ++j)
    {
        Cell* cell = &row->cells[j];
        if (cell->contentType == COLUMN_TYPE_TEXT)
            os << ToString(mStrings[cell->contentIndex]) << " ";
    }

    return os.str();
}

int UITable::GetSelected() const
{
    if (mSelected < 0)
        return 0;

    LogAssert(mSelected >= 0 && mSelected < (int)mVisibleRows.size(), "Invalid selected data");
    return mVisibleRows[mSelected] + 1;
}

void UITable::SetSelected(int index)
{
    int oldSelected = mSelected;

    mSelected = -1;
    mSelColumn = 0;
    mSelDoubleclick = false;

    --index; // Switch from 1-based indexing to 0-based indexing

    int rowcount = (int)mRows.size();
    if (rowcount == 0 || index < 0)
        return;

    if (index >= rowcount)
        index = rowcount - 1;

    // If the mSelected row is not visible, open its ancestors to make it visible
    bool selection_invisible = mRows[index].visibleIndex < 0;
    if (selection_invisible) 
    {
        std::set<int> mOpenedTrees;
        GetOpenedTrees(mOpenedTrees);
        int indent = mRows[index].indent;
        for (int j = index - 1; j >= 0; --j) 
        {
            if (mRows[j].indent < indent) 
            {
                mOpenedTrees.insert(j);
                indent = mRows[j].indent;
            }
        }
        SetOpenedTrees(mOpenedTrees);
    }

    if (index >= 0) 
    {
        mSelected = mRows[index].visibleIndex;
        LogAssert(mSelected >= 0 && mSelected < (int)mVisibleRows.size(), "Invalida selected data");
    }

    if (mSelected != oldSelected || selection_invisible)
        AutoScroll();
}

void UITable::SetOverrideFont(const std::shared_ptr<BaseUIFont>& font)
{
    if (mFont == font)
        return;

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    mFont = skin->GetFont();

    mRowHeight = mFont->GetDimension(L"Ay")[1] + 4;
    mRowHeight = std::max(mRowHeight, 1);

    UpdateScrollBar();
}

const std::shared_ptr<BaseUIFont>& UITable::GetOverrideFont() const
{
    return mFont;
}

UITable::DynamicData UITable::GetDynamicData() const
{
    DynamicData dyndata;
    dyndata.mSelected = GetSelected();
    dyndata.mScrollPos = mScrollbar->GetPosition();
    dyndata.mKeynavTime = mKeynavTime;
    dyndata.mKeynavBuffer = mKeynavBuffer;
    if (mHasTreeColumn)
        GetOpenedTrees(dyndata.mOpenedTrees);
    return dyndata;
}

void UITable::SetDynamicData(const DynamicData &dyndata)
{
    if (mHasTreeColumn)
        SetOpenedTrees(dyndata.mOpenedTrees);

    mKeynavTime = dyndata.mKeynavTime;
    mKeynavBuffer = dyndata.mKeynavBuffer;

    SetSelected(dyndata.mSelected);
    mSelColumn = 0;
    mSelDoubleclick = false;

    mScrollbar->SetPosition(dyndata.mScrollPos);
}

void UITable::UpdateAbsolutePosition()
{
    BaseUIElement::UpdateAbsolutePosition();
    UpdateScrollBar();
}

void UITable::Draw()
{
    if (!IsVisible())
        return;

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();

    Renderer* renderer = Renderer::Get();
    renderer->SetBlendState(mBlendState);

    // draw background
    bool drawBackground = mBackground.GetAlpha() > 0;
    if (mBorder)
        skin->Draw3DSunkenPane(mBackground, true, drawBackground, mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect);
    else if (drawBackground)
        skin->Draw2DRectangle(mBackground, mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect);

    // get clipping rect
    RectangleShape<2, int> clientClip(mAbsoluteRect);
    clientClip.mExtent[0] -= 2;
    clientClip.mExtent[1] -= 2;
    if (mScrollbar->IsVisible()) 
    {
        clientClip.mExtent[0] =
            mScrollbar->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - clientClip.GetVertice(RVP_UPPERLEFT)[0];
        clientClip.mCenter[0] = 
            mScrollbar->GetAbsolutePosition().GetVertice(RVP_UPPERLEFT)[0] - (int)round(clientClip.mExtent[0] / 2.f);
    }
    clientClip.ClipAgainst(mAbsoluteClippingRect);

    // draw visible rows

    int scrollPos = mScrollbar->GetPosition();
    int rowMin = scrollPos / mRowHeight;
    int rowMax = (scrollPos + mAbsoluteRect.mExtent[1] - 1) / mRowHeight + 1;
    rowMax = std::min(rowMax, (int)mVisibleRows.size());

    RectangleShape<2, int> rowRect(mAbsoluteRect);
    if (mScrollbar->IsVisible())
    {
        rowRect.mExtent[0] -= skin->GetSize(DS_SCROLLBAR_SIZE);
        rowRect.mCenter[0] -= skin->GetSize(DS_SCROLLBAR_SIZE) / 2;
    }
    rowRect.mCenter[1] = rowRect.GetVertice(RVP_UPPERLEFT)[1] + 
        (rowMin * mRowHeight - scrollPos) + mRowHeight / 2;
    rowRect.mExtent[1] = mRowHeight;

    for (int i = rowMin; i < rowMax; ++i) 
    {
        Row *row = &mRows[mVisibleRows[i]];
        bool isSel = i == mSelected;
        SColor color = mColor;
        if (isSel) 
        {
            skin->Draw2DRectangle(mHighlight, mVisualBackground, rowRect, &clientClip);
            color = mHighlightText;
        }

        for (int j = 0; j < row->cellcount; ++j)
            DrawCell(&row->cells[j], color, rowRect, clientClip);

        rowRect.mCenter[1] += mRowHeight;
    }

    // Draw children
    BaseUIElement::Draw();

    renderer->SetDefaultBlendState();
}

void UITable::DrawCell(const Cell *cell, SColor color,
    const RectangleShape<2, int> &rowRect, const RectangleShape<2, int> &clientClip)
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();

    if (cell->contentType == COLUMN_TYPE_TEXT || cell->contentType == COLUMN_TYPE_TREE) 
    {
        RectangleShape<2, int> textRect = rowRect;
        textRect.mCenter[0] = rowRect.GetVertice(RVP_UPPERLEFT)[0] + cell->xpos + cell->xmax / 2;
        textRect.mExtent[0] = cell->xmax;

        if (cell->colorDefined)
            color = cell->color;

        if (mFont) 
        {
            if (cell->contentType == COLUMN_TYPE_TEXT)
                mFont->Draw(mStrings[cell->contentIndex], textRect, color, false, true, &clientClip);
            else // tree
                mFont->Draw(cell->contentIndex ? L"+" : L"-", textRect, color, false, true, &clientClip);
        }
    }
    else if (cell->contentType == COLUMN_TYPE_IMAGE) 
    {
        if (cell->contentIndex < 0)
            return;

        std::shared_ptr<Texture2> image = mImages[cell->contentIndex];
        if (image) 
        {
            Vector2<int> destPos = rowRect.GetVertice(RVP_UPPERLEFT);
            destPos[0] += cell->xpos + 6;

            RectangleShape<2, int> sourceRect;
            sourceRect.mExtent[0] = image->GetDimension(0);
            sourceRect.mExtent[1] = image->GetDimension(1);
            sourceRect.mCenter = sourceRect.mExtent / 2;
            
            int imgh = sourceRect.GetVertice(RVP_LOWERRIGHT)[1];
            int rowh = rowRect.mExtent[1];
            if (imgh >= rowh)
            {
                sourceRect.mExtent[1] = rowh - sourceRect.GetVertice(RVP_UPPERLEFT)[1];
                sourceRect.mCenter[1] = rowh - (int)round(sourceRect.mExtent[1] / 2.f);
            }
            else destPos[1] += (rowh - imgh) * 3 / 2;

            SColor color(255, 255, 255, 255);
            const SColor colors[] = { color };

            auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
            effect->SetTexture(image);

            RectangleShape<2, int> posRect(destPos, sourceRect.mAxis, sourceRect.mExtent);
            skin->Draw2DTexture(mVisual, posRect, sourceRect, colors, &clientClip);
        }
    }
}

bool UITable::OnEvent(const Event& evt)
{
    if (!IsEnabled())
        return BaseUIElement::OnEvent(evt);

    if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
        if (evt.mKeyInput.mPressedDown && (
            evt.mKeyInput.mKey == KEY_DOWN ||
            evt.mKeyInput.mKey == KEY_UP ||
            evt.mKeyInput.mKey == KEY_HOME ||
            evt.mKeyInput.mKey == KEY_END ||
            evt.mKeyInput.mKey == KEY_NEXT ||
            evt.mKeyInput.mKey == KEY_PRIOR))
        {
            int offset = 0;
            switch (evt.mKeyInput.mKey)
            {
                case KEY_DOWN:
                    offset = 1;
                    break;
                case KEY_UP:
                    offset = -1;
                    break;
                case KEY_HOME:
                    offset = -(int)mVisibleRows.size();
                    break;
                case KEY_END:
                    offset = (int)mVisibleRows.size();
                    break;
                case KEY_NEXT:
                    offset = mAbsoluteRect.mExtent[1] / mRowHeight;
                    break;
                case KEY_PRIOR:
                    offset = -(int)(mAbsoluteRect.mExtent[1] / mRowHeight);
                    break;
                default:
                    break;
            }
            int oldSelected = mSelected;
            int rowcount = (int)mVisibleRows.size();
            if (rowcount != 0) 
            {
                mSelected = std::clamp(mSelected + offset, 0, rowcount - 1);
                AutoScroll();
            }

            if (mSelected != oldSelected)
                SendTableEvent(0, false);

            return true;
        }

        if (evt.mKeyInput.mPressedDown && (
            evt.mKeyInput.mKey == KEY_LEFT ||
            evt.mKeyInput.mKey == KEY_RIGHT))
        {
            // Open/close subtree via keyboard
            if (mSelected >= 0) 
            {
                int dir = evt.mKeyInput.mKey == KEY_LEFT ? -1 : 1;
                ToggleVisibleTree(mSelected, dir, true);
            }
            return true;
        }
        else if (!evt.mKeyInput.mPressedDown && (
            evt.mKeyInput.mKey == KEY_RETURN ||
            evt.mKeyInput.mKey == KEY_SPACE))
        {
            SendTableEvent(0, true);
            return true;
        }
        else if (evt.mKeyInput.mKey == KEY_ESCAPE ||
            evt.mKeyInput.mKey == KEY_SPACE)
        {
            // pass to parent
        }
        else if (evt.mKeyInput.mPressedDown && evt.mKeyInput.mChar)
        {
            // change selection based on text as it is typed
            unsigned int now = Timer::GetTime();
            if (now - mKeynavTime >= 500)
                mKeynavBuffer = L"";
            mKeynavTime = now;

            // add to key buffer if not a key repeat
            if (!(mKeynavBuffer.size() == 1 &&
                mKeynavBuffer[0] == evt.mKeyInput.mChar))
            {
                mKeynavBuffer.append(&evt.mKeyInput.mChar);
            }

            // find the mSelected item, starting at the current selection
            // don't change selection if the key buffer matches the current item
            int oldSelected = mSelected;
            int start = std::max(mSelected, 0);
            int rowcount = (int)mVisibleRows.size();
            for (int k = 1; k < rowcount; ++k) 
            {
                int current = start + k;
                if (current >= rowcount)
                    current -= rowcount;
                if (CheckRowStart(GetRow(current), mKeynavBuffer)) 
                {
                    mSelected = current;
                    break;
                }
            }
            AutoScroll();
            if (mSelected != oldSelected)
                SendTableEvent(0, false);

            return true;
        }
    }
    if (evt.mEventType == ET_MOUSE_INPUT_EVENT)
    {
        Vector2<int> p{ evt.mMouseInput.X, evt.mMouseInput.Y };

        if (evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL)
        {
            mScrollbar->SetPosition(mScrollbar->GetPosition() +
                (evt.mMouseInput.mWheel < 0 ? -3 : 3) * -(int)mRowHeight / 2);
            return true;
        }

        // Find hovered row and cell
        bool reallyHovering = false;
        int rowIdx = GetRowAt(p[1], reallyHovering);
        const Cell *cell = NULL;
        if (reallyHovering) 
        {
            int cellColumn = GetCellAt(p[0], rowIdx);
            if (cellColumn >= 0)
                cell = &(GetRow(rowIdx)->cells[cellColumn]);
        }

        // Update tooltip
        SetToolTipText(cell ? mStrings[cell->tooltipIndex].c_str() : L"");

        // Fix for #1567/#1806:
        // BaseUIScrollBar passes double click events to its parent,
        // which we don't want. Detect this case and discard the event
        if (evt.mMouseInput.mEvent != MIE_MOUSE_MOVED &&
            mScrollbar->IsVisible() && mScrollbar->IsPointInside(p))
            return true;

        if (evt.mMouseInput.IsLeftPressed() &&
            (IsPointInside(p) || evt.mMouseInput.mEvent == MIE_MOUSE_MOVED))
        {
            int selColumn = 0;
            bool selDoubleClick = (evt.mMouseInput.mEvent == MIE_LMOUSE_DOUBLE_CLICK);
            bool plusminusClicked = false;

            // For certain events (left click), report column
            // Also open/close subtrees when the +/- is clicked
            if (cell && (
                evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN ||
                evt.mMouseInput.mEvent == MIE_LMOUSE_DOUBLE_CLICK ||
                evt.mMouseInput.mEvent == MIE_LMOUSE_TRIPLE_CLICK))
            {
                selColumn = cell->reportedColumn;
                if (cell->contentType == COLUMN_TYPE_TREE)
                    plusminusClicked = true;
            }

            if (plusminusClicked) 
            {
                if (evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
                    ToggleVisibleTree(rowIdx, 0, false);
            }
            else 
            {
                // Normal selection
                int oldSelected = mSelected;
                mSelected = rowIdx;
                AutoScroll();

                if (mSelected != oldSelected || selColumn >= 1 || selDoubleClick) 
                    SendTableEvent(selColumn, selDoubleClick);

                // Treeview: double click opens/closes trees
                if (mHasTreeColumn && selDoubleClick)
                    ToggleVisibleTree(mSelected, 0, false);
            }
        }
        return true;
    }
    if (evt.mEventType == ET_UI_EVENT &&
        evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED &&
        evt.mUIEvent.mCaller == mScrollbar.get())
    {
        // Don't pass events from our scrollbar to the parent
        return true;
    }

    return BaseUIElement::OnEvent(evt);
}

/******************************************************************************/
/* UITable helper functions                                                  */
/******************************************************************************/

int UITable::AllocString(const std::string& text)
{
    std::map<std::string, int>::iterator it = mAllocStrings.find(text);
    if (it == mAllocStrings.end()) 
    {
        int id = (int)mStrings.size();
        std::wstring wtext = ToWideString(text);
        mStrings.emplace_back(wtext);
        mAllocStrings.insert(std::make_pair(text, id));
        return id;
    }

    return it->second;
}

int UITable::AllocImage(const std::string& imageName)
{
    std::map<std::string, int>::iterator it = mAllocImages.find(imageName);
    if (it == mAllocImages.end()) 
    {
        std::shared_ptr<ResHandle> resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(imageName)));
        std::shared_ptr<ImageResourceExtraData> resData =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        std::shared_ptr<Texture2> texture = resData->GetImage();
        if (texture)
        {
            int id = (int)mImages.size();
            mImages.push_back(texture);
            mAllocImages.insert(std::make_pair(imageName, id));
            return id;
        }
        return -1;
    }

    return it->second;
}

void UITable::AllocationComplete()
{
    // Called when done with creating rows and cells from table data,
    // i.e. when AllocString and AllocImage won't be called anymore
    mAllocStrings.clear();
    mAllocImages.clear();
}

const UITable::Row* UITable::GetRow(int i) const
{
    if (i >= 0 && i < (int)mVisibleRows.size())
        return &mRows[mVisibleRows[i]];

    return NULL;
}

bool UITable::CheckRowStart(const Row* row, const std::wstring& str) const
{
    if (row == NULL)
        return false;

    for (int j = 0; j < row->cellcount; ++j) 
    {
        Cell *cell = &row->cells[j];
        if (cell->contentType == COLUMN_TYPE_TEXT) 
        {
            const std::wstring &cellstr = mStrings[cell->contentIndex];
            if (cellstr.size() >= str.size() &&
                str.compare(ToWideString(ToLowerString(ToString(
                    cellstr.substr(0, str.size()))))) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

int UITable::GetRowAt(int y, bool& reallyHovering) const
{
    reallyHovering = false;

    int rowcount = (int)mVisibleRows.size();
    if (rowcount == 0)
        return -1;

    // Use arithmetic to find row
    int relColumn = y - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[1] - 1;
    int i = (relColumn + mScrollbar->GetPosition()) / mRowHeight;

    if (i >= 0 && i < rowcount) 
    {
        reallyHovering = true;
        return i;
    }
    if (i < 0)
        return 0;

    return rowcount - 1;
}

int UITable::GetCellAt(int x, int rowIdx) const
{
    const Row *row = GetRow(rowIdx);
    if (row == NULL)
        return -1;

    // Use binary search to find cell in row
    int relx = x - mAbsoluteRect.GetVertice(RVP_UPPERLEFT)[0] - 1;
    int columnMin = 0;
    int columnMax = row->cellcount - 1;
    while (columnMin < columnMax) 
    {
        int pivot = columnMin + (columnMax - columnMin) / 2;
        LogAssert(pivot >= 0 && pivot < row->cellcount, "Invalid row");
        const Cell *cell = &row->cells[pivot];

        if (relx >= cell->xmin && relx <= cell->xmax)
            return pivot;

        if (relx < cell->xmin)
            columnMax = pivot - 1;
        else
            columnMin = pivot + 1;
    }

    if (columnMin >= 0 && columnMin < row->cellcount &&
        relx >= row->cells[columnMin].xmin &&
        relx <= row->cells[columnMin].xmax)
        return columnMin;

    return -1;
}

void UITable::AutoScroll()
{
    if (mSelected >= 0) 
    {
        int pos = mScrollbar->GetPosition();
        int maxpos = mSelected * mRowHeight;
        int minpos = maxpos - (mAbsoluteRect.mExtent[1] - mRowHeight);
        if (pos > maxpos)
            mScrollbar->SetPosition(maxpos);
        else if (pos < minpos)
            mScrollbar->SetPosition(minpos);
    }
}

void UITable::UpdateScrollBar()
{
    int totalheight = mRowHeight * (int)mVisibleRows.size();
    int scrollmax = std::max(0, totalheight - mAbsoluteRect.mExtent[1]);
    mScrollbar->SetVisible(scrollmax > 0);
    mScrollbar->SetMax(scrollmax);
    mScrollbar->SetSmallStep(mRowHeight);
    mScrollbar->SetLargeStep(2 * mRowHeight);
    mScrollbar->SetPageSize(totalheight);
}

void UITable::SendTableEvent(int column, bool doubleclick)
{
    mSelColumn = column;
    mSelDoubleclick = doubleclick;
    if (mParent) 
    {
        Event e;
        memset(&e, 0, sizeof e);
        e.mEventType = ET_UI_EVENT;
        e.mUIEvent.mCaller = this;
        e.mUIEvent.mElement = 0;
        e.mUIEvent.mEventType = UIEVT_TABLE_CHANGED;
        mParent->OnEvent(e);
    }
}

void UITable::GetOpenedTrees(std::set<int>& openedTrees) const
{
    openedTrees.clear();
    int rowcount = (int)mRows.size();
    for (int i = 0; i < rowcount - 1; ++i) 
    {
        if (mRows[i].indent < mRows[i + 1].indent &&
            mRows[i + 1].visibleIndex != -2)
            openedTrees.insert(i);
    }
}

void UITable::SetOpenedTrees(const std::set<int>& openedTrees)
{
    int oldSelected = -1;
    if (mSelected >= 0)
        oldSelected = mVisibleRows[mSelected];

    std::vector<int> parents;
    std::vector<int> closedParents;

    mVisibleRows.clear();
    for (int i = 0; i < (int)mRows.size(); ++i)
    {
        Row *row = &mRows[i];

        // Update list of ancestors
        while (!parents.empty() && mRows[parents.back()].indent >= row->indent)
            parents.pop_back();
        while (!closedParents.empty() &&
            mRows[closedParents.back()].indent >= row->indent)
            closedParents.pop_back();

        LogAssert(closedParents.size() <= parents.size(), "Invalid size");

        if (closedParents.empty()) 
        {
            // Visible row
            row->visibleIndex = (int)mVisibleRows.size();
            mVisibleRows.push_back(i);
        }
        else if (parents.back() == closedParents.back()) 
        {
            // Invisible row, direct parent is closed
            row->visibleIndex = -2;
        }
        else 
        {
            // Invisible row, direct parent is open, some ancestor is closed
            row->visibleIndex = -1;
        }

        // If not a leaf, add to parents list
        if (i < mRows.size() - 1 && row->indent < mRows[i + 1].indent) 
        {
            parents.push_back(i);

            int contentIndex = 0; // "-", open
            if (openedTrees.count(i) == 0)
            {
                closedParents.push_back(i);
                contentIndex = 1; // "+", closed
            }

            // Update all cells of type "tree"
            for (int j = 0; j < row->cellcount; ++j)
                if (row->cells[j].contentType == COLUMN_TYPE_TREE)
                    row->cells[j].contentIndex = contentIndex;
        }
    }

    UpdateScrollBar();

    // mSelected must be updated since it is a visible row index
    if (oldSelected >= 0)
        mSelected = mRows[oldSelected].visibleIndex;
}

void UITable::OpenTree(int toOpen)
{
    std::set<int> mOpenedTrees;
    GetOpenedTrees(mOpenedTrees);
    mOpenedTrees.insert(toOpen);
    SetOpenedTrees(mOpenedTrees);
}

void UITable::CloseTree(int toClose)
{
    std::set<int> mOpenedTrees;
    GetOpenedTrees(mOpenedTrees);
    mOpenedTrees.erase(toClose);
    SetOpenedTrees(mOpenedTrees);
}

// The following function takes a visible row index (hidden rows skipped)
// dir: -1 = left (close), 0 = auto (toggle), 1 = right (open)
void UITable::ToggleVisibleTree(int rowIdx, int dir, bool moveSelection)
{
    // Check if the chosen tree is currently open
    const Row *row = GetRow(rowIdx);
    if (row == NULL)
        return;

    bool wasOpen = false;
    for (int j = 0; j < row->cellcount; ++j) 
    {
        if (row->cells[j].contentType == COLUMN_TYPE_TREE) 
        {
            wasOpen = row->cells[j].contentIndex == 0;
            break;
        }
    }

    // Check if the chosen tree should be opened
    bool doOpen = !wasOpen;
    if (dir < 0)
        doOpen = false;
    else if (dir > 0)
        doOpen = true;

    // Close or open the tree; the heavy lifting is done by SetOpenedTrees
    if (wasOpen && !doOpen)
        CloseTree(mVisibleRows[rowIdx]);
    else if (!wasOpen && doOpen)
        OpenTree(mVisibleRows[rowIdx]);

    // Change mSelected row if requested by caller,
    // this is useful for keyboard navigation
    if (moveSelection) 
    {
        int sel = rowIdx;
        if (wasOpen && doOpen) 
        {
            // Move selection to first child
            const Row *maybeChild = GetRow(sel + 1);
            if (maybeChild && maybeChild->indent > row->indent)
                sel++;
        }
        else if (!wasOpen && !doOpen) 
        {
            // Move selection to parent
            LogAssert(GetRow(sel) != NULL, "invalid row selection");
            while (sel > 0 && GetRow(sel - 1)->indent >= row->indent)
                sel--;
            sel--;
            if (sel < 0)  // was root already mSelected?
                sel = rowIdx;
        }
        if (sel != mSelected) 
        {
            mSelected = sel;
            AutoScroll();
            SendTableEvent(0, false);
        }
    }
}

void UITable::AlignContent(Cell* cell, int xmax, int contentWidth, int align)
{
    // requires that cell.xmin, cell.xmax are properly set
    // align = 0: left aligned, 1: centered, 2: right aligned, 3: inline
    if (align == 0) 
    {
        cell->xpos = cell->xmin;
        cell->xmax = xmax;
    }
    else if (align == 1) 
    {
        cell->xpos = (cell->xmin + xmax - contentWidth) / 2;
        cell->xmax = xmax;
    }
    else if (align == 2) 
    {
        cell->xpos = xmax - contentWidth;
        cell->xmax = xmax;
    }
    else 
    {
        // inline alignment: the cells of the column don't have an aligned
        // right border, the right border of each cell depends on the content
        cell->xpos = cell->xmin;
        cell->xmax = cell->xmin + contentWidth;
    }
}
