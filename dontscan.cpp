// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "dontscan.h"
#include "actions.h"
#include "res.h"
#include "version.h"
#include <windowsx.h>
#include <vector>

class DontScanDlg
{
public:
    INT_PTR                 DoModal(HINSTANCE hinst, UINT idd, HWND hwndParent);
    INT_PTR                 DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    void                    ReadDirectories();
    bool                    WriteDirectories();
    int                     GetCaret() const;
    int                     GetSelection() const;
    void                    SetSelection(int index, bool select=true);
    bool                    GetItem(int index, std::wstring& item) const;
    void                    InsertItem(const WCHAR* item);
    void                    RemoveItem(int index);
    void                    UpdateButtons() const;
    static INT_PTR CALLBACK StaticDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE               m_hinst = 0;
    HWND                    m_hwnd = 0;
    HWND                    m_hwndListView = 0;
    std::vector<std::wstring> m_orig;
    bool                    m_dirty = false;
};

INT_PTR DontScanDlg::DoModal(HINSTANCE hinst, UINT idd, HWND hwndParent)
{
    assert(!m_hwnd);

    ThreadDpiAwarenessContext dpiContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    m_hinst = hinst;
    const INT_PTR nRet = DialogBoxParam(hinst, MAKEINTRESOURCE(idd), hwndParent, StaticDlgProc, LPARAM(this));

    return nRet;
}

INT_PTR DontScanDlg::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        {
// TODO: Use system theming when available.
            RECT rcCtrl;
            HWND hwndPlaceholder = GetDlgItem(m_hwnd, IDC_DONTSCAN_LIST);
            GetWindowRect(hwndPlaceholder, &rcCtrl);
            MapWindowRect(0, m_hwnd, &rcCtrl);

            m_hwndListView = CreateWindow(WC_LISTVIEW, TEXT(""), WS_TABSTOP|WS_BORDER|WS_VISIBLE|WS_CHILD|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER|LVS_REPORT|LVS_SORTASCENDING,
                    rcCtrl.left, rcCtrl.top, rcCtrl.right - rcCtrl.left, rcCtrl.bottom - rcCtrl.top, m_hwnd, (HMENU)IDC_DONTSCAN_LIST, m_hinst, NULL);
            if (!m_hwndListView)
                return -1;

            SetWindowPos(m_hwndListView, hwndPlaceholder, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
            DestroyWindow(hwndPlaceholder);
            ListView_SetExtendedListViewStyle(m_hwndListView, LVS_EX_FULLROWSELECT|LVS_EX_INFOTIP|LVS_EX_DOUBLEBUFFER);

            GetClientRect(m_hwndListView, &rcCtrl);

            LVCOLUMN lvc = {};
            lvc.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
            lvc.fmt = LVCFMT_LEFT;
            lvc.cx = rcCtrl.right - rcCtrl.left - GetSystemMetrics(SM_CXVSCROLL);
            lvc.pszText = TEXT("Directory");
            ListView_InsertColumn(m_hwndListView, 0, &lvc);

            ReadDirectories();
            UpdateButtons();
        }
        break;

    case WM_COMMAND:
        {
            const WORD id = GET_WM_COMMAND_ID(wParam, lParam);
            const HWND hwnd = GET_WM_COMMAND_HWND(wParam, lParam);
            const WORD code = GET_WM_COMMAND_CMD(wParam, lParam);
            switch (id)
            {
            case IDC_DONTSCAN_ADD:
                {
                    std::wstring add;
                    if (ShellBrowseForFolder(m_hwnd, TEXT("Add Folder"), add))
                    {
                        InsertItem(add.c_str());
                        UpdateButtons();
                    }
                }
                break;
            case IDC_DONTSCAN_REMOVE:
                {
                    const int index = GetSelection();
                    if (index >= 0)
                    {
                        RemoveItem(index);
                        UpdateButtons();
                    }
                }
                break;
            case IDOK:
                EndDialog(m_hwnd, WriteDirectories());
                break;
            case IDCANCEL:
                EndDialog(m_hwnd, false);
                break;
            default:
                return false;
            }
        }
        break;

    case WM_NOTIFY:
        {
            NMHDR* pnmhdr = (NMHDR*)lParam;
            NMLISTVIEW* pnml = reinterpret_cast<NMLISTVIEW*>(pnmhdr);

            if (pnmhdr->idFrom == IDC_DONTSCAN_LIST)
            {
                switch (pnmhdr->code)
                {
                case LVN_ITEMCHANGED:
                    UpdateButtons();
                    return true;
                }
            }

            return false;
        }
        break;

    default:
        return false;
    }

    return true;
}

void DontScanDlg::ReadDirectories()
{
    ReadRegStrings(TEXT("DontScanDirectories"), m_orig);

    SendMessage(m_hwndListView, WM_SETREDRAW, false, 0);

    ListView_DeleteAllItems(m_hwndListView);
    for (const auto& dir : m_orig)
        InsertItem(dir.c_str());

    SetSelection(0, false);

    SendMessage(m_hwndListView, WM_SETREDRAW, true, 0);
    InvalidateRect(m_hwndListView, nullptr, false);
}

bool DontScanDlg::WriteDirectories()
{
    std::vector<std::wstring> dirs;

    const int count = ListView_GetItemCount(m_hwndListView);
    if (count >= 0)
    {
        WCHAR dir[1024];
        for (int index = 0; index < count; ++index)
        {
            ListView_GetItemText(m_hwndListView, index, 0, dir, _countof(dir));
            dirs.emplace_back(dir);
        }
    }

    bool changed = (m_orig.size() != dirs.size());
    for (size_t index = 0; !changed && index < dirs.size(); ++index)
        changed = !!wcscmp(m_orig[index].c_str(), dirs[index].c_str());

    if (changed)
        WriteRegStrings(TEXT("DontScanDirectories"), dirs);

    return changed;
}

int DontScanDlg::GetCaret() const
{
    return ListView_GetNextItem(m_hwndListView, -1, LVIS_FOCUSED);
}

int DontScanDlg::GetSelection() const
{
    return ListView_GetNextItem(m_hwndListView, -1, LVIS_SELECTED);
}

void DontScanDlg::SetSelection(int index, bool select)
{
    // Clear the focused state from whichever item currently has it.  This
    // works around what seems to be a bug in ListView_SetItemState, where the
    // focused state is only cleared from an item that has both LVIS_SELECTED
    // and LVIS_FOCUSED.
    const int caret = ListView_GetNextItem(m_hwndListView, -1, LVNI_FOCUSED);
    if (caret >= 0 )
        ListView_SetItemState(m_hwndListView, caret, 0, LVIS_SELECTED|LVIS_FOCUSED);
    const DWORD bits = select ? LVIS_SELECTED|LVIS_FOCUSED : LVIS_FOCUSED;
    const DWORD mask = LVIS_SELECTED|LVIS_FOCUSED;
    ListView_SetItemState(m_hwndListView, index, bits, mask);
}

bool DontScanDlg::GetItem(int index, std::wstring& item) const
{
    WCHAR text[1024];
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    lvi.pszText = text;
    lvi.cchTextMax = _countof(text);
    if (!ListView_GetItem(m_hwndListView, &lvi))
        return false;
    item = text;
    return true;
}

void DontScanDlg::InsertItem(const WCHAR* item)
{
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.pszText = const_cast<WCHAR*>(item);
    const int index = ListView_InsertItem(m_hwndListView, &lvi);

    assert(index >= 0);
    if (index >= 0)
    {
#ifdef DEBUG
        std::wstring verify;
        assert(GetItem(index, verify));
        assert(!wcscmp(item, verify.c_str()));
#endif
        SetSelection(index);
    }
}

void DontScanDlg::RemoveItem(int index)
{
#ifdef DEBUG
    const bool deleted =
#endif
    ListView_DeleteItem(m_hwndListView, index);

#ifdef DEBUG
    assert(deleted);
#endif

    const int count = ListView_GetItemCount(m_hwndListView);
    if (count > 0)
        SetSelection(std::min<int>(index, count - 1));
}

static void EnableControl(HWND hwndCtrl, bool enable=true)
{
    if (!enable && GetFocus() == hwndCtrl)
        SendMessage(hwndCtrl, WM_NEXTDLGCTL, 0, 0);
    EnableWindow(hwndCtrl, enable);
}

void DontScanDlg::UpdateButtons() const
{
    EnableControl(GetDlgItem(m_hwnd, IDC_DONTSCAN_REMOVE), GetSelection() >= 0);
}

INT_PTR DontScanDlg::StaticDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DontScanDlg* pThis = nullptr;

    if (msg == WM_INITDIALOG)
    {
        // Set the "this" pointer.
        pThis = reinterpret_cast<DontScanDlg*>(lParam);
        SetWindowLongPtr(hwnd, DWLP_USER, DWORD_PTR(pThis));
        pThis->m_hwnd = hwnd;
    }
    else
    {
        pThis = reinterpret_cast<DontScanDlg*>(GetWindowLongPtr(hwnd, DWLP_USER));
    }

    if (pThis)
    {
        assert(pThis->m_hwnd == hwnd);

        if (msg == WM_DESTROY)
        {
            return true;
        }
        else if (msg == WM_NCDESTROY)
        {
            SetWindowLongPtr(hwnd, DWLP_USER, 0);
            pThis->m_hwnd = 0;
            return true;
        }

        const INT_PTR lResult = pThis->DlgProc(msg, wParam, lParam);

        // Must return actual result in order for things like
        // WM_CTLCOLORLISTBOX to work.
        if (lResult || msg == WM_INITDIALOG)
            return lResult;
    }

    return false;
}

bool ConfigureDontScanFiles(HINSTANCE hinst, HWND hwndParent)
{
    DontScanDlg dlg;
    return dlg.DoModal(hinst, IDD_CONFIG_DONTSCAN, hwndParent);
}

