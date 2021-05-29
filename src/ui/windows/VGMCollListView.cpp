/**
 * VGMTrans (c) - 2002-2021
 * Licensed under the zlib license
 * See the included LICENSE for more information
 */

#include "pch.h"
#include "resource.h"
#include "VGMCollListView.h"
#include "DragDropSource.h"
#include "WinVGMRoot.h"
#include "CollDialog.h"

CVGMCollListView theVGMCollListView;

/////////////////////////////////////////////////////////////////////////////
// Message handlers

BOOL CVGMCollListView::PreTranslateMessage(MSG* pMsg) {
  return FALSE;
}

LRESULT CVGMCollListView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
  LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

  Init();
  bHandled = TRUE;

  return NULL;
}

void CVGMCollListView::OnDestroy() {
  // Destroy the state image list. The other image lists shouldn't be destroyed
  // since they are copies of the system image list. Destroying those on Win 9x
  // is a Bad Thing to do.
  if (m_imlState)
    m_imlState.Destroy();

  SetMsgHandled(false);
}

void CVGMCollListView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) {
  switch (nChar) {
    case VK_SPACE:
      winroot.Play();
      break;

    case VK_ESCAPE:
      winroot.Stop();
      break;

    /* Any other key is reflected to the parent class */
  }
}

LRESULT CVGMCollListView::OnContextMenu(HWND hwndCtrl, CPoint ptClick) {
  // if Shift-F10
  if (ptClick.x == -1 && ptClick.y == -1)
    ptClick = (CPoint)GetMessagePos();

  ScreenToClient(&ptClick);

  UINT uFlags;
  int iItem = HitTest(ptClick, &uFlags);

  if (iItem == -1)
    return 0;

  VGMColl* pcoll = (VGMColl*)GetItemData(iItem);
  ClientToScreen(&ptClick);
  ItemContextMenu(hwndCtrl, ptClick, pcoll);

  return 0;
}


void CVGMCollListView::OnDoubleClick(UINT nFlags, CPoint point) {
  winroot.Play();
}


/////////////////////////////////////////////////////////////////////////////
// Operations

void CVGMCollListView::Init() {
  InitColumns();
  InitImageLists();
  SetExtendedListViewStyle(LVS_EX_HEADERDRAGDROP, LVS_EX_HEADERDRAGDROP);

  // On XP, set some additional properties of the list ctrl.
  if (g_bXPOrLater) {
    // Turning on LVS_EX_DOUBLEBUFFER also enables the transparent
    // selection marquee.
    SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);

    // Each tile will have 2 additional lines (3 lines total).
    LVTILEVIEWINFO lvtvi = {sizeof(LVTILEVIEWINFO), LVTVIM_COLUMNS};

    lvtvi.cLines = 2;
    lvtvi.dwFlags = LVTVIF_AUTOSIZE;
    SetTileViewInfo(&lvtvi);
  }
}

void CVGMCollListView::Clear() {
  DeleteAllItems();
  theCollDialog.Clear();

  if (-1 != m_nSortedCol) {
    if (g_bXPOrLater) {
      // Remove the sort arrow indicator from the sorted column.
      HDITEM hdi = {HDI_FORMAT};
      CHeaderCtrl wndHdr = GetHeader();

      wndHdr.GetItem(m_nSortedCol, &hdi);
      hdi.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
      wndHdr.SetItem(m_nSortedCol, &hdi);

      // Remove the sorted column color from the list.
      SetSelectedColumn(-1);
    }

    m_nSortedCol = -1;
    m_bSortAscending = true;
  }
}

void CVGMCollListView::SetViewMode(int nMode) {
  ATLASSERT(nMode >= LV_VIEW_ICON && nMode <= LV_VIEW_TILE);

  if (g_bXPOrLater) {
    if (LV_VIEW_TILE == nMode)
      SetImageList(m_imlTiles, LVSIL_NORMAL);
    else
      SetImageList(m_imlLarge, LVSIL_NORMAL);

    SetView(nMode);
  } else {
    DWORD dwViewStyle;

    ATLASSERT(LV_VIEW_TILE != nMode);

    switch (nMode) {
      case LV_VIEW_ICON:
        dwViewStyle = LVS_ICON;
        break;
      case LV_VIEW_SMALLICON:
        dwViewStyle = LVS_SMALLICON;
        break;
      case LV_VIEW_LIST:
        dwViewStyle = LVS_LIST;
        break;
      case LV_VIEW_DETAILS:
        dwViewStyle = LVS_REPORT;
        break;
        DEFAULT_UNREACHABLE;
    }

    ModifyStyle(LVS_TYPEMASK, dwViewStyle);
  }
}

LRESULT CVGMCollListView::OnLvnItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) {
  // CListViewCtrl* listview = reinterpret_cast<CListViewCtrl*>(pnmh);
  NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pnmh;

  if (pNMListView->iItem != -1 && pNMListView->uNewState & LVIS_FOCUSED) {
    VGMColl* coll = (VGMColl*)pNMListView->lParam;
    winroot.SelectColl(coll);
  } else
    winroot.SelectColl(NULL);
  bHandled = false;
  return 0;
}

BOOL CVGMCollListView::SelectColl(VGMColl* coll) {
  if (coll)  // check that the item passed was not NULL
  {
    LVFINDINFO findinfo;
    findinfo.flags = LVFI_PARAM;
    findinfo.lParam = (DWORD_PTR)coll;
    return CListViewCtrl::SelectItem(FindItem(&findinfo, -1));  // items[vgmfile]);
  }
  return false;
}

void CVGMCollListView::AddColl(VGMColl* newColl) {
  LVITEMW newItem;  // = new LVITEMW();
  newItem.lParam = (DWORD_PTR)newColl;
  newItem.iItem = GetItemCount();
  newItem.iSubItem = 0;
  newItem.iImage = 0;
  // newItem.iGroupId = 0;
  newItem.pszText = (LPWSTR)newColl->GetName()->c_str();
  newItem.mask = LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE;  // | LVIF_GROUPID;
  InsertItem(&newItem);

  // newItem->iImage = NULL;//nIconIndexNormal;
}

void CVGMCollListView::RemoveColl(VGMColl* theColl) {
  LVFINDINFO findinfo;
  findinfo.flags = LVFI_PARAM;
  findinfo.lParam = (DWORD_PTR)theColl;
  DeleteItem(FindItem(&findinfo, -1));  // items[theFile]);
}

LRESULT CVGMCollListView::OnNMRClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) {
  SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Other methods

void CVGMCollListView::InitColumns() {
  InsertColumn(0, _T("Collection Name"), LVCFMT_LEFT, 130, 0);
  InsertColumn(1, _T("Type"), LVCFMT_LEFT, 40, 0);
}

void CVGMCollListView::InitImageLists() {
  // Create a state image list
  HICON hiColl = AtlLoadIconImage(IDI_COLL, LR_DEFAULTCOLOR, 16, 16);
  // HICON hiNext = AtlLoadIconImage ( IDI_NEXT_CAB, LR_DEFAULTCOLOR, 16, 16 );

  m_imlState.Create(16, 16, ILC_COLOR8 | ILC_MASK, 2, 1);

  m_imlState.AddIcon(hiColl);
  // m_imlState.AddIcon ( hiNext );

  DestroyIcon(hiColl);
  // DestroyIcon ( hiNext );

  SetImageList(m_imlState, LVSIL_SMALL);

  // On pre-XP, we're done.
  if (!g_bXPOrLater)
    return;
}
