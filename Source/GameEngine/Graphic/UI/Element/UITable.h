// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

// 07.10.2005 - Multicolor-Listbox addet by A. Buschhueter (Acki)
//                                          A_Buschhueter@gmx.de

// Copyright (C) 2003-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UITABLE_H
#define UITABLE_H

#include "UIElement.h"

#include "Graphic/Resource/Color.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIScrollBar;
class BaseUIFont;

//! modes for ordering used when a column header is clicked
enum UIColumnOrdering
{
    //! Do not use ordering
    CO_NONE,

    //! Send a EGET_TABLE_HEADER_CHANGED message when a column header is clicked.
    CO_CUSTOM,

    //! Sort it ascending by it's ascii value like: a,b,c,...
    CO_ASCENDING,

    //! Sort it descending by it's ascii value like: z,x,y,...
    CO_DESCENDING,

    //! Sort it ascending on first click, descending on next, etc
    CO_FLIP_ASCENDING_DESCENDING,

    //! Not used as mode, only to get maximum value for this enum
    CO_COUNT
};

//! Names for EGUI_COLUMN_ORDERING types
const char* const UIColumnOrderingNames[] =
{
    "none",
    "custom",
    "ascend",
    "descend",
    "ascend_descend",
    0,
};

enum UIOrderingMode
{
    //! No element ordering
    OM_NONE,

    //! Elements are ordered from the smallest to the largest.
    OM_ASCENDING,

    //! Elements are ordered from the largest to the smallest.
    OM_DESCENDING,

    //! this value is not used, it only specifies the amount of default ordering types
    //! available.
    OM_COUNT
};

const char* const UIOrderingModeNames[] =
{
    "none",
    "ascending",
    "descending",
    0
};

enum UITableDrawFlags
{
    TDF_ROWS = 1,
    TDF_COLUMNS = 2,
    TDF_ACTIVE_ROW = 4,
    TDF_COUNT
};

//! Default table GUI element.
/** \par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_TABLE_CHANGED
\li EGET_TABLE_mSelected_AGAIN
\li EGET_TABLE_HEADER_CHANGED
*/
class BaseUITable : public BaseUIElement
{
public:
    //! constructor
    BaseUITable(int id, RectangleShape<2, int> rectangle)
        : BaseUIElement(UIET_TABLE, id, rectangle) {}

    //! Initialize table
    virtual void OnInit() = 0;

    //! clears the table, deletes all items in the table
    virtual void Clear() = 0;

    //! Sets another skin independent font. If this is set to zero, the table uses the font of the skin.
    virtual void SetOverrideFont(const std::shared_ptr<BaseUIFont>& font = 0) = 0;

    //! Gets the override font (if any)
    virtual const std::shared_ptr<BaseUIFont>& GetOverrideFont() const = 0;

    /* Get index of currently mSelected row (first=1; 0 if none mSelected) */
    virtual int GetSelected() const = 0;

    /* Set currently mSelected row (first=1; 0 if none mSelected) */
    // If given index is not visible at the moment, select its parent
    // AutoScroll to make the mSelected row fully visible
    virtual void SetSelected(int index) = 0;
};


class UITable : public BaseUITable
{
public:
    /*
        Stores dynamic data that should be preserved
        when updating a form
    */
    struct DynamicData
    {
        int mSelected = 0;
        int mScrollPos = 0;
        int mKeynavTime = 0;
        std::wstring mKeynavBuffer;
        std::set<int> mOpenedTrees;
    };

    /*
        An option of the form <name>=<value>
    */
    struct Option
    {
        std::string mName;
        std::string mValue;

        Option(const std::string& name, const std::string& value) :
            mName(name),
            mValue(value)
        {}
    };

    /*
        A list of options that concern the entire table
    */
    typedef std::vector<Option> TableOptions;

    /*
        A column with options
    */
    struct TableColumn
    {
        std::string mType;
        std::vector<Option> mSettings;
    };
    typedef std::vector<TableColumn> TableColumns;


    UITable(BaseUI* ui, int id, RectangleShape<2, int> rectangle);

    //! Initialize table
    virtual void OnInit();

    virtual ~UITable();

    /* Split a string of the form "name=value" into name and value */
    static Option SplitOption(const std::string& str);

    /* Set textlist-like options, columns and data */
    void SetTextList(const std::vector<std::string>& content, bool transparent);

    /* Set generic table options, columns and content */
    // Adds empty strings to end of content if there is an incomplete row
    void SetTable(const TableOptions& options,
        const TableColumns& columns, std::vector<std::string>& content);

    //! clears the table, deletes all items in the table
    virtual void Clear();

    //! Sets another skin independent font. If this is set to zero, the table uses the font of the skin.
    virtual void SetOverrideFont(const std::shared_ptr<BaseUIFont>& font = 0);

    //! Gets the override font (if any)
    virtual const std::shared_ptr<BaseUIFont>& GetOverrideFont() const;

    /* Get index of currently mSelected row (first=1; 0 if none mSelected) */
    virtual int GetSelected() const;

    /* Set currently mSelected row (first=1; 0 if none mSelected) */
    // If given index is not visible at the moment, select its parent
    // AutoScroll to make the mSelected row fully visible
    virtual void SetSelected(int index);

    /* Get info about last event (string such as "CHG:1:2") */
    // Call this after UIEVT_TABLE_CHANGED
    std::string CheckEvent();

    /* Get selection, scroll position and opened (sub)trees */
    DynamicData GetDynamicData() const;

    /* Set selection, scroll position and opened (sub)trees */
    void SetDynamicData(const DynamicData &dyndata);

    /* Must be called when position or size changes */
    virtual void UpdateAbsolutePosition();

    /* draw method */
    virtual void Draw();

    /* event handler */
    virtual bool OnEvent(const Event& evt);

protected:

    enum ColumnType 
    {
        COLUMN_TYPE_TEXT,
        COLUMN_TYPE_IMAGE,
        COLUMN_TYPE_COLOR,
        COLUMN_TYPE_INDENT,
        COLUMN_TYPE_TREE,
    };

    struct Cell 
    {
        int xmin;
        int xmax;
        int xpos;
        ColumnType contentType;
        int contentIndex;
        int tooltipIndex;
        SColor color;
        bool colorDefined;
        int reportedColumn;
    };

    struct Row 
    {
        Cell* cells;
        int cellcount;
        int indent;
        // visibleIndex >= 0: is index of row in mVisibleRows
        // visibleIndex == -1: parent open but other ancestor closed
        // visibleIndex == -2: parent closed
        int visibleIndex;
    };

    int AllocString(const std::string& text);
    int AllocImage(const std::string& imageName);
    void AllocationComplete();

    // Helper for draw() that draws a single cell
    void DrawCell(const Cell* cell, SColor color,
        const RectangleShape<2, int>& rowRect,
        const RectangleShape<2, int>& clientClip);

    // Returns the i-th visible row (NULL if i is invalid)
    const Row* GetRow(int i) const;

    // Key navigation helper
    bool CheckRowStart(const Row* row, const std::wstring& str) const;

    // Returns the row at a given screen Y coordinate
    // Returns index i such that mRows[i] is valid (or -1 on error)
    int GetRowAt(int y, bool& reallyHovering) const;

    // Returns the cell at a given screen X coordinate within mRows[rowIdx]
    // Returns index j such that mRows[rowIdx].cells[j] is valid
    // (or -1 on error)
    int GetCellAt(int x, int rowIdx) const;

    // Make the mSelected row fully visible
    void AutoScroll();

    // Should be called when m_rowcount or mRowHeight changes
    void UpdateScrollBar();

    // Sends ET_UI_EVENT / UIEVT_TABLE_CHANGED to parent
    void SendTableEvent(int column, bool doubleclick);

    // Functions that help deal with hidden rows
    // The following functions take raw row indices (hidden rows not skipped)
    void GetOpenedTrees(std::set<int>& openedTrees) const;
    void SetOpenedTrees(const std::set<int>& openedTrees);
    void OpenTree(int toOpen);
    void CloseTree(int toClose);
    // The following function takes a visible row index (hidden rows skipped)
    // dir: -1 = left (close), 0 = auto (toggle), 1 = right (open)
    void ToggleVisibleTree(int rowIdx, int dir, bool moveSelection);

    // Aligns cell content in column according to alignment specification
    // align = 0: left aligned, 1: centered, 2: right aligned, 3: inline
    static void AlignContent(Cell* cell, int xmax, int contentWidth, int align);

    BaseUI* mUI;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<Visual> mVisualBackground;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

    // Table content (including hidden rows)
    std::vector<Row> mRows;
    // Table content (only visible; indices into rows)
    std::vector<int> mVisibleRows;
    bool mIsTextlist = false;
    bool mHasTreeColumn = false;

    // Selection status
    int mSelected = -1; // index of row (1...n), or 0 if none selected
    int mSelColumn = 0;
    bool mSelDoubleclick = false;

    // Keyboard navigation stuff
    unsigned int mKeynavTime = 0;
    std::wstring mKeynavBuffer = L"";

    // Drawing and geometry information
    bool mBorder = true;
    SColor mColor = SColor(255, 255, 255, 255);
    SColor mBackground = SColor(255, 0, 0, 0);
    SColor mHighlight = SColor(255, 70, 100, 50);
    SColor mHighlightText = SColor(255, 255, 255, 255);
    int mRowHeight = 1;
    std::shared_ptr<BaseUIFont> mFont = nullptr;
    std::shared_ptr<BaseUIScrollBar> mScrollbar = nullptr;

    // Allocated strings and images
    std::vector<std::wstring> mStrings;
    std::vector<std::shared_ptr<Texture2>> mImages;
    std::map<std::string, int> mAllocStrings;
    std::map<std::string, int> mAllocImages;
};


#endif