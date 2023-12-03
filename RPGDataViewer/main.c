#include <tchar.h>
#include <Windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include "resource.h"
#include "debug.h"
#include "rpgdata.h"
#include "main.h"

// ����XP���ؼ�
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' language='*' processorArchitecture='*' publicKeyToken='6595b64144ccf1df'\"")

BOOL bResizingView = FALSE;
HIMAGELIST himl; // ͼ���б�
HWND hwndMain; // ������
HWND hwndTree, hwndList;
HWND hwndStatus;
SHFILEINFO sfi = {0}; // ��ǰ�ļ����ļ���Ϣ
TCHAR szFile[MAX_PATH]; // �򿪵��ļ�
struct ruby_object filecontent;

/* ������ */
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HMENU hMenu;
	HWND hWnd;
	MSG msg;
	WNDCLASSEX wcex;
	InitCommonControls(); // ��ʼ���ؼ�

	/* ע�ᴰ���� */
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_SIZEWE);
	wcex.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), (UINT)NULL);
	wcex.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), (UINT)NULL);
	wcex.hInstance = hInstance;
	wcex.lpszClassName = TEXT("MainWindow");
	wcex.lpfnWndProc = WndProc;
	wcex.style = CS_DBLCLKS; // ����˫����Ϣ
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, TEXT("ע�ᴰ����ʧ�ܣ�"), TEXT("����"), MB_ICONERROR);
		return 0;
	}

	/* ��������ʾ���� */
	hMenu = CreateMainMenu();
	hWnd = CreateWindow(wcex.lpszClassName, APP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, hMenu, hInstance, NULL);
	if (!hWnd)
	{
		MessageBox(NULL, TEXT("����������ʧ�ܣ�"), TEXT("����"), MB_ICONERROR);
		return 0;
	}
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	/* ��Ϣѭ�� */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

/* ��������Ϣ������ */
// WPARAM = Word(16λ) Parameter, LPARAM = Long(32λ) Parameter
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HINSTANCE hInstance;
	int wmId;
	LPNMHDR pnmh;
	LPNMLISTVIEW pnmlv;
	RECT rect;
	SIZE size;

	switch (uMsg)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_OPEN:
			OpenFileDlg();
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_ABOUT:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutDlgProc);
			break;
		}
		break;
	case WM_CREATE:
		/* �����ڴ���ʱ */
		hwndMain = hWnd;
		hInstance = GetModuleHandle(NULL);
		ZeroMemory(&filecontent, sizeof(filecontent));
#if _DEBUG
		d_init(); // ��ʼ�ڴ�й©���
#endif
		
		// �������ؼ�����
		hwndTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL, WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, 0, 0, TREEVIEW_WIDTH, 0, hWnd, NULL, hInstance, NULL);
		himl = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 0, 1);
		TreeView_SetImageList(hwndTree, himl, TVSIL_NORMAL);
		
		// �����б�ؼ�����
		hwndList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
		ListView_SetImageList(hwndList, himl, LVSIL_SMALL);
		ListView_SetExtendedListViewStyle(hwndList, LVS_EX_DOUBLEBUFFER); // ���ð�͸��ѡ���
		
		InitImageList();
		InitListView();

		// ����״̬��
		hwndStatus = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
		SendMessage(hwndStatus, SB_SIMPLE, TRUE, (LPARAM)NULL);
		break;
	case WM_DESTROY:
		/* �������Ѿ��ر�ʱ */
		rpgdata_free(&filecontent);
		if (sfi.hIcon != NULL)
			DestroyIcon(sfi.hIcon);
#if _DEBUG
		d_destroy(); // ֹͣ�ڴ�й©���
#endif
		PostQuitMessage(0); // �˳���Ϣѭ��
		break;
	case WM_LBUTTONDBLCLK:
		/* ˫��ʱ�ָ�Ĭ�Ͽ�� */
		GetWindowRect(hwndTree, &rect);
		size.cy = rect.bottom - rect.top;
		GetClientRect(hWnd, &rect);
		MoveWindow(hwndTree, 0, 0, TREEVIEW_WIDTH, size.cy, TRUE);
		MoveWindow(hwndList, TREEVIEW_WIDTH + VIEW_MARGIN, 0, rect.right - rect.left - TREEVIEW_WIDTH - VIEW_MARGIN, size.cy, TRUE);
		break;
	case WM_LBUTTONDOWN:
		bResizingView = TRUE;
		SetCapture(hWnd);
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		bResizingView = FALSE;
		break;
	case WM_MOUSEMOVE:
		/* ����϶��ı䴰���� */
		if (bResizingView)
		{
			size.cx = GET_X_LPARAM(lParam); // ���ؼ������� = �������ڿͻ�����λ��
			if (size.cx < 0)
				break;
			GetWindowRect(hwndTree, &rect);
			size.cy = rect.bottom - rect.top; // ���ؼ�����߶�

			// ��ȡ�ͻ������
			GetClientRect(hWnd, &rect);
			rect.right -= rect.left;
			if (size.cx > rect.right)
				break;

			MoveWindow(hwndTree, 0, 0, size.cx, size.cy, TRUE);
			MoveWindow(hwndList, size.cx + VIEW_MARGIN, 0, rect.right - size.cx - VIEW_MARGIN, size.cy, TRUE);
		}
		break;
	case WM_NOTIFY:
		pnmh = (LPNMHDR)lParam;
		if (pnmh->hwndFrom == hwndTree && pnmh->code == TVN_SELCHANGED)
		{
			DisplayFolder((struct ruby_object *)((LPNMTREEVIEW)lParam)->itemNew.lParam); // ѡ�����ؼ���ʱ���ļ���
			UpdateStatusText(-1);
		}
		else if (pnmh->hwndFrom == hwndList)
		{
			if (pnmh->code == LVN_ITEMACTIVATE)
				OpenItem(((LPNMITEMACTIVATE)lParam)->iItem); // ˫�����»س���ʱ�򿪶��� (ע: ����ʹ��NMITEMACTIVATE�ṹ���е�lParam��Ա)
			else if (pnmh->code == LVN_ITEMCHANGED)
			{
				// ѡ�л�ȡ��ѡ����Ŀʱ����״̬������
				pnmlv = (LPNMLISTVIEW)lParam;
				if (pnmlv->uNewState & LVIS_SELECTED)
					UpdateStatusText(pnmlv->iItem);
				else
					UpdateStatusText(-1);
			}
		}
		break;
	case WM_SIZE:
		/* �������ڴ�Сʱ */
		if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
		{
			GetWindowRect(hwndTree, &rect);
			size.cx = rect.right - rect.left; // ���ؼ�������
			GetWindowRect(hwndStatus, &rect);
			size.cy = HIWORD(lParam) - (rect.bottom - rect.top); // ���ؼ�����߶�

			MoveWindow(hwndStatus, 0, 0, 0, 0, TRUE);
			MoveWindow(hwndTree, 0, 0, size.cx, size.cy, TRUE);
			MoveWindow(hwndList, size.cx + VIEW_MARGIN, 0, LOWORD(lParam) - size.cx - VIEW_MARGIN, size.cy, TRUE);
		}
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return FALSE;
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int wmId;
	switch (uMsg)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, wmId);
			break;
		}
		break;
	case WM_INITDIALOG:
		return TRUE; // ���س����ɹرմ���
	}
	return FALSE;
}

void AddTreeChildren(HTREEITEM hParent, struct ruby_object *obj)
{
	HTREEITEM hNode;
	long i, len;
	LPTSTR pMemStr = NULL;
	TCHAR szText[20];
	TVINSERTSTRUCT tvis;
	struct ruby_object *value;
	ZeroMemory(&tvis, sizeof(tvis));

	tvis.item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_TEXT;
	tvis.item.iImage = tvis.item.iSelectedImage = 1;
	tvis.hParent = hParent;
	len = obj->length;
	if (obj->type == RBT_HASH)
		len *= 2;
	else if (obj->type == RBT_HASH_DEF)
		len = 2 * len + 1;
	for (i = 0; i < len; i++)
	{
		if (obj->type == RBT_IVAR || obj->type == RBT_OBJECT || obj->type == RBT_STRUCT || obj->type == RBT_UCLASS)
			value = &obj->members[i].value;
		else
			value = &obj->adata[i];
		if (!rpgdata_isfolder(value->type))
			continue;
		if (obj->type == RBT_IVAR || obj->type == RBT_OBJECT || obj->type == RBT_STRUCT || obj->type == RBT_UCLASS)
		{
			pMemStr = UTF8ToTStr(obj->members[i].name);
			tvis.item.pszText = pMemStr;
		}
		else if (obj->type == RBT_HASH || obj->type == RBT_HASH_DEF)
		{
			GetHashMemberName(obj, i, szText, sizeof(szText));
			tvis.item.pszText = szText;
		}
		else if (obj->type == RBT_USRMARSHAL)
			tvis.item.pszText = TEXT("marshal_dump");
		else
		{
			StringCbPrintf(szText, sizeof(szText), TEXT("%ld"), i);
			tvis.item.pszText = szText;
		}
		tvis.item.lParam = (LPARAM)value;
		hNode = TreeView_InsertItem(hwndTree, &tvis);
		if (pMemStr != NULL)
			free(pMemStr);
		AddTreeChildren(hNode, value);
	}
}

/* ���������ڲ˵� */
HMENU CreateMainMenu(void)
{
	HMENU hMenu = CreateMenu();
	HMENU hmenuFile = CreatePopupMenu();
	HMENU hmenuHelp = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP | MF_STRING, (UINT_PTR)hmenuFile, TEXT("�ļ�(&F)"));
	AppendMenu(hmenuFile, MF_STRING, IDM_OPEN, TEXT("��(&O)..."));
	AppendMenu(hmenuFile, MF_SEPARATOR, (UINT_PTR)NULL, NULL);
	AppendMenu(hmenuFile, MF_STRING, IDM_EXIT, TEXT("�˳�(&E)"));

	AppendMenu(hMenu, MF_POPUP | MF_STRING, (UINT_PTR)hmenuHelp, TEXT("����(&H)"));
	AppendMenu(hmenuHelp, MF_STRING, IDM_ABOUT, TEXT("����(&A)..."));
	return hMenu;
}

/* ��ʾָ���ļ��е����� */
void DisplayFolder(struct ruby_object *folder)
{
	long i, j, len;
	LVITEM lvi;
	TCHAR szText[20];
	LPTSTR pMemStr = NULL;
	struct ruby_object *value;
	unsigned char real_type;

	if (!rpgdata_isfolder(folder->type))
		return;
	ListView_DeleteAllItems(hwndList);

	// ��ʾ�����ļ��� (�涨lParamΪNULL)
	lvi.iItem = 0;
	if (folder != &filecontent)
	{
		lvi.mask = LVIF_IMAGE | LVIF_TEXT;
		lvi.iSubItem = 0;
		lvi.iImage = 1;
		lvi.pszText = TEXT("..");
		ListView_InsertItem(hwndList, &lvi);
		lvi.iItem++;
	}

	// ָ����ݷ�ʽͼ���overlay���
	// ��lvi.mask����LVIF_STATE����ʱ��ʾ
	lvi.state = INDEXTOOVERLAYMASK(1);
	lvi.stateMask = LVIS_OVERLAYMASK;
	
	// ������ǰ�ļ���
	for (i = 0; i < 2; i++)
	{
		len = folder->length;
		if (folder->type == RBT_HASH)
			len *= 2;
		else if (folder->type == RBT_HASH_DEF)
			len = 2 * len + 1;

		for (j = 0; j < len; j++)
		{
			if (folder->type == RBT_IVAR || folder->type == RBT_OBJECT || folder->type == RBT_STRUCT || folder->type == RBT_UCLASS)
				value = &folder->members[j].value; // ��ǰ��Ա
			else
				value = &folder->adata[j]; // ��ǰ����Ԫ��

			// �����ʵ���������� (���Զ�������)
			if (value->type == RBT_LINK)
				real_type = value->adata->type;
			else
				real_type = value->type;
			
			// ����ʾ�ļ���, ����ʾ������Ŀ
			if (i == 0)
			{
				if (!rpgdata_isfolder(value->type))
					continue;
			}
			else if (i == 1)
			{
				if (rpgdata_isfolder(value->type))
					continue;
			}

			// ����
			lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT;
			if (value->type == RBT_LINK || value->type == RBT_SYMLINK)
				lvi.mask |= LVIF_STATE; // ��ʾ��ݷ�ʽͼ��
			lvi.iSubItem = 0;
			lvi.lParam = (LPARAM)value; // ���б�����Ruby�����
			if (rpgdata_isfolder(real_type))
				lvi.iImage = 1; // �ļ���ͼ��
			else if (real_type == RBT_BIGNUM || real_type == RBT_FIXNUM || real_type == RBT_FLOAT || real_type == RBT_USERDEF || real_type == RBT_USRMARSHAL)
				lvi.iImage = 5; // ����������ͼ��
			else
				lvi.iImage = 4; // �ַ���ͼ��
			if (folder->type == RBT_IVAR || folder->type == RBT_OBJECT || folder->type == RBT_STRUCT || folder->type == RBT_UCLASS)
			{
				// �����������
				pMemStr = UTF8ToTStr(folder->members[j].name);
				lvi.pszText = pMemStr;
			}
			else if (folder->type == RBT_HASH || folder->type == RBT_HASH_DEF)
			{
				// ��ϣ���Ա��
				GetHashMemberName(folder, j, szText, sizeof(szText));
				lvi.pszText = szText;
			}
			else if (folder->type == RBT_USRMARSHAL)
				lvi.pszText = TEXT("marshal_dump");
			else
			{
				// ������������
				StringCbPrintf(szText, sizeof(szText), TEXT("%ld"), j);
				lvi.pszText = szText;
			}
			ListView_InsertItem(hwndList, &lvi);
			if (pMemStr != NULL)
			{
				free(pMemStr);
				pMemStr = NULL;
			}

			// ����
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 1;
			lvi.pszText = GetTypeString(value->type);
			ListView_SetItem(hwndList, &lvi);

			// ֵ
			lvi.iSubItem = 2;
			lvi.pszText = szText;
			switch (value->type)
			{
			case RBT_BIGNUM:
				pMemStr = rpgdata_bignum_to_string(value);
				break;
			case RBT_CLASS:
			case RBT_IVAR:
			case RBT_MODULE:
			case RBT_OBJECT:
			case RBT_STRUCT:
			case RBT_SYMBOL:
			case RBT_SYMLINK:
			case RBT_UCLASS:
			case RBT_USERDEF:
			case RBT_USRMARSHAL:
				pMemStr = UTF8ToTStr(value->name);
				break;
			case RBT_FALSE:
				lvi.pszText = TEXT("false");
				break;
			case RBT_FIXNUM:
				StringCbPrintf(szText, sizeof(szText), TEXT("%ld"), value->ldata);
				break;
			case RBT_FLOAT:
				switch (value->err)
				{
				case RUBY_FLOAT_INF:
					lvi.pszText = TEXT("+��");
					break;
				case RUBY_FLOAT_NEGINF:
					lvi.pszText = TEXT("-��");
					break;
				case RUBY_FLOAT_NAN:
					lvi.pszText = TEXT("NaN");
					break;
				default:
					pMemStr = UTF8ToTStr(value->dbl_str); // ֱ����ʾԭʼ�ַ���
				}
				break;
			case RBT_LINK:
				pMemStr = GetItemPath(GetParentFolder(value->adata, NULL), value->adata, FALSE);
				break;
			case RBT_NIL:
				lvi.pszText = TEXT("nil");
				break;
			case RBT_REGEXP:
			case RBT_STRING:
				pMemStr = UTF8ToTStr(value->sdata);
				break;
			case RBT_TRUE:
				lvi.pszText = TEXT("true");
				break;
			default:
				lvi.pszText = NULL;
			}
			if (pMemStr != NULL)
				lvi.pszText = pMemStr;
			if (lvi.pszText != NULL)
			{
				ListView_SetItem(hwndList, &lvi);
				if (pMemStr != NULL)
				{
					free(pMemStr);
					pMemStr = NULL;
				}
			}

			lvi.iItem++;
		}
	}
}

// ��ȡ��ǰ��ʾ���ļ���
struct ruby_object *GetCurrentFolder(void)
{
	TVITEM tvi;
	tvi.mask = TVIF_PARAM;
	tvi.hItem = TreeView_GetSelection(hwndTree); // ���ؼ��еĵ�ǰѡ����
	TreeView_GetItem(hwndTree, &tvi);
	return (struct ruby_object *)tvi.lParam;
}

void GetHashMemberName(struct ruby_object *obj, long i, LPTSTR lpStr, int cbSize)
{
	if (obj->type == RBT_HASH_DEF && i == 0)
		StringCbCopy(lpStr, cbSize, TEXT("Ĭ��ֵ"));
	else if ((obj->type == RBT_HASH && i % 2 == 0) || (obj->type == RBT_HASH_DEF && i % 2 == 1))
		StringCbPrintf(lpStr, cbSize, TEXT("��%ld"), i / 2);
	else
		StringCbPrintf(lpStr, cbSize, TEXT("ֵ%ld"), (i - 1) / 2);
}

// ��������Ԫ��(������Ա)��ֵ�ĵ�ַȷ������(�����)���±�
// �õ�ַ�������±�����������
long GetIndex(struct ruby_object *parent, struct ruby_object *obj)
{
	if (parent == NULL)
		parent = GetCurrentFolder();
	if (parent->type == RBT_ARRAY || parent->type == RBT_HASH || parent->type == RBT_HASH_DEF)
		return obj - parent->adata;
	else if (parent->type == RBT_IVAR || parent->type == RBT_OBJECT || parent->type == RBT_STRUCT || parent->type == RBT_UCLASS)
		return (struct ruby_member *)obj - (struct ruby_member *)&parent->members[0].value;
	else
		return -1; // �޷�ȷ��
}

/* ����һ��Ruby�����·���ַ��� */
// hFolder: ���ڵ��ļ���
// obj: ����, ���ΪNULL������hFolder��·��
// bParentItem: �����Ƿ�Ϊ..�ļ���
LPTSTR GetItemPath(HTREEITEM hFolder, struct ruby_object *obj, BOOL bParentItem)
{
	BOOL bAddDot = FALSE;
	int cchSize = 150;
	int i, j;
	int nLen;
	long iIndex;
	LPTSTR pszText = (LPTSTR)malloc(cchSize * sizeof(TCHAR));
	LPTSTR pszName, pTemp;
	TCHAR szText[40];
	TCHAR temp;
	TVITEM tvi;
	struct ruby_object *folder;

	if (pszText == NULL)
		return NULL;
	tvi.mask = TVIF_PARAM;
	tvi.hItem = hFolder;
	if (obj == NULL)
	{
		// ��ȡ�ϲ��ļ���
		for (i = 0; i <= bParentItem; i++) // ����: ��ѡ����ʱһ��, ѡ����".."�ļ���ʱ����
		{
			TreeView_GetItem(hwndTree, &tvi);
			obj = (struct ruby_object *)tvi.lParam;
			tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem);
		}
	}

	j = 0; // ���ַ����±�
	while (TreeView_GetItem(hwndTree, &tvi))
	{
		folder = (struct ruby_object *)tvi.lParam;
		iIndex = GetIndex(folder, obj); // �������iIndex=-1, ������˺�������������
		pszName = szText;
		if (folder->type == RBT_ARRAY)
		{
			if (folder == &filecontent)
				StringCbPrintf(szText, sizeof(szText), TEXT("data%ld"), iIndex); // Marshal.dump�����
			else
				StringCbPrintf(szText, sizeof(szText), TEXT("[%ld]"), iIndex); // ������±�
		}
		else if (folder->type == RBT_HASH || folder->type == RBT_HASH_DEF)
		{
			// ��ϣ��
			if (folder->type == RBT_HASH_DEF && iIndex == 0)
				StringCbCopy(szText, sizeof(szText), TEXT(".default"));
			else if ((folder->type == RBT_HASH && iIndex % 2 == 0) || (folder->type == RBT_HASH_DEF && iIndex % 2 == 1))
				StringCbPrintf(szText, sizeof(szText), TEXT(".keys[%ld]"), iIndex / 2);
			else
				StringCbPrintf(szText, sizeof(szText), TEXT(".values[%ld]"), (iIndex - 1) / 2);
		}
		else if (folder->type == RBT_USRMARSHAL)
			StringCbCopy(szText, sizeof(szText), TEXT(".marshal_dump"));
		else
		{
			// ����ĳ�Ա����
			if (strcmp(folder->members[iIndex].name, ".") != 0)
			{
				pszName = UTF8ToTStr(folder->members[iIndex].name);
				bAddDot = TRUE;
			}
			else
				szText[0] = '\0';
		}

		// �������ַ���
		nLen = lstrlen(pszName);
		if (cchSize - j < nLen + 1)
		{
			cchSize += nLen + 1;
			cchSize *= 2; // ��������ʱ����
			pTemp = (LPTSTR)realloc(pszText, cchSize * sizeof(TCHAR));
			if (pTemp == NULL)
			{
				free(pszText);
				pszText = NULL;
			}
			else
				pszText = pTemp;
		}
		if (pszText != NULL)
		{
			for (i = nLen - 1; i >= 0; i--)
				pszText[j++] = pszName[i];
		}
		if (pszName != szText)
			free(pszName);
		if (pszText == NULL)
			return NULL;

		if (bAddDot)
		{
			pszText[j++] = '.';
			bAddDot = FALSE;
		}

		// ������ȡ��һ���ļ��е���Ϣ, ֱ�����
		obj = (struct ruby_object *)tvi.lParam;
		tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem);
	}

	// ��ת�ַ���
	for (i = 0; i < j / 2; i++)
	{
		temp = pszText[i];
		pszText[i] = pszText[j - i - 1];
		pszText[j - i - 1] = temp;
	}
	pszText[j] = '\0';
	return pszText;
}

// ��ȡRuby�������ڵ��ļ���
// hFolder: ����ָ���ļ����еĸ����ļ���, ���ΪNULL, ���������ļ���
HTREEITEM GetParentFolder(struct ruby_object *obj, HTREEITEM hFolder)
{
	TVITEM tvi;
	if (hFolder == NULL)
	{
		hFolder = TreeView_GetRoot(hwndTree);
		if (InFolder(obj, &filecontent))
			return hFolder; // ����ڸ��ļ�������ֱ�ӷ���
	}
	
	// ����hItem�ļ����µ������ӽ��ļ���
	tvi.mask = TVIF_PARAM;
	tvi.hItem = TreeView_GetChild(hwndTree, hFolder);
	while (tvi.hItem != NULL)
	{
		TreeView_GetItem(hwndTree, &tvi);
		if (InFolder(obj, (struct ruby_object *)tvi.lParam))
			return tvi.hItem;
		else
		{
			hFolder = GetParentFolder(obj, tvi.hItem); // �ݹ���������ļ���
			if (hFolder != NULL)
				return hFolder;
		}
		tvi.hItem = TreeView_GetNextSibling(hwndTree, tvi.hItem);
	}
	return NULL;
}

LPTSTR GetTypeString(unsigned char type)
{
	switch (type)
	{
	case RBT_ARRAY:
		return TEXT("����");
	case RBT_BIGNUM:
		return TEXT("������");
	case RBT_CLASS:
		return TEXT("��");
	case RBT_FALSE:
	case RBT_TRUE:
		return TEXT("����");
	case RBT_FIXNUM:
		return TEXT("����");
	case RBT_FLOAT:
		return TEXT("������");
	case RBT_HASH:
	case RBT_HASH_DEF:
		return TEXT("��ϣ��");
	case RBT_IVAR:
	case RBT_UCLASS:
		return TEXT("��չ����");
	case RBT_LINK:
		return TEXT("����");
	case RBT_MODULE:
		return TEXT("ģ��");
	case RBT_NIL:
		return TEXT("��");
	case RBT_OBJECT:
		return TEXT("����");
	case RBT_REGEXP:
		return TEXT("������ʽ");
	case RBT_STRUCT:
		return TEXT("�ṹ��");
	case RBT_STRING:
		return TEXT("�ַ���");
	case RBT_SYMBOL:
		return TEXT("����");
	case RBT_SYMLINK:
		return TEXT("��������");
	case RBT_USERDEF:
		return TEXT("����������");
	case RBT_USRMARSHAL:
		return TEXT("�Զ��嵼������");
	default:
		return TEXT("δ֪����");
	}
}

// �ж�һ��Ruby�����Ƿ���һ���ļ����� (���������ļ���)
// ע��: ���Ŷ�����ܲ�λ������һ���ļ���, ������ĳ�������symbol_ref��
BOOL InFolder(struct ruby_object *obj, struct ruby_object *folder)
{
	int size; // ������Ĵ�С
	if (obj < folder->adata)
		return FALSE; // ��obj�ĵ�ַλ��folder������ĵ�ַ֮ǰ, �򲻿���������ļ�����
	size = folder->length;
	if (folder->type == RBT_ARRAY)
		size *= sizeof(struct ruby_object);
	else if (folder->type == RBT_HASH || folder->type == RBT_HASH_DEF)
	{
		size *= 2;
		if (folder->type == RBT_HASH_DEF)
			size++;
		size *= sizeof(struct ruby_object);
	}
	else if (folder->type == RBT_OBJECT || folder->type == RBT_STRUCT)
		size *= sizeof(struct ruby_member);
	else
		return FALSE;

	return ((unsigned char *)obj - folder->data < size); // ֻҪobj�ĵ�ַλ��folder�������ַ֮��, ����Ϊobj��folder��
}

/* ��ʼ��ͼ���б� */
void InitImageList(void)
{
	HICON hIcon;
	HMODULE hmdl[] = {LoadLibrary(TEXT("regedit.exe")), LoadLibrary(TEXT("shell32.dll"))};
	int iIcons[] = {1, 4, 5, 16769, 205, 206, 30};
	int iIconsSrc[] = {1, 1, 1, 1, 0, 0, 1};
	int cx, cy;
	int i;

	ImageList_GetIconSize(himl, &cx, &cy);
	for (i = 0; i < _countof(iIcons); i++)
	{
		hIcon = (HICON)LoadImage(hmdl[iIconsSrc[i]], MAKEINTRESOURCE(iIcons[i]), IMAGE_ICON, cx, cy, (UINT)NULL);
		if (hIcon == NULL)
			hIcon = LoadIcon(NULL, IDI_EXCLAMATION);
		ImageList_AddIcon(himl, hIcon);
		DestroyIcon(hIcon);
	}
	
	ImageList_SetOverlayImage(himl, 6, 1); // ��ݷ�ʽͼ��
	for (i = 0; i < _countof(hmdl); i++)
		FreeLibrary(hmdl[i]);
}

void InitListView(void)
{
	LPTSTR strarr[] = {TEXT("����"), TEXT("����"), TEXT("ֵ")};
	int iWidth[] = {300, 100, 200};
	int i;
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT | LVCF_WIDTH;
	for (i = 0; i < _countof(strarr); i++)
	{
		lvc.pszText = strarr[i];
		lvc.cx = iWidth[i];
		ListView_InsertColumn(hwndList, i, &lvc);
	}
}

/* ��ʾ���ļ��ĶԻ��� */
void OpenFileDlg(void)
{
	OPENFILENAME ofn;
	UINT uRet;

	szFile[0] = '\0';
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndMain;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = TEXT("RPG Maker XP �����ļ�(*.rxdata)\0*.rxdata\0�����ļ�\0*\0");
	ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == IDOK)
	{
		rpgdata_free(&filecontent);
		uRet = rpgdata_load(szFile, &filecontent);
		if (uRet == 0)
		{
			// �ɹ����ļ�
			UpdateTitle();
			UpdateTree();
		}
		else
		{
			rpgdata_free(&filecontent);
			switch (uRet)
			{
			case RPGERR_NOTOPEN:
				MessageBox(hwndMain, TEXT("���ļ�ʧ��"), TEXT("����"), MB_ICONWARNING);
				break;
			case RPGERR_FORMAT:
				MessageBox(hwndMain, TEXT("�޷�ʶ����ļ��ĸ�ʽ"), TEXT("����"), MB_ICONWARNING);
				break;
			case RPGERR_MEMORY:
				MessageBox(hwndMain, TEXT("ϵͳ�ڴ治��"), TEXT("����"), MB_ICONERROR);
				break;
			case RPGERR_DATA:
				MessageBox(hwndMain, TEXT("�ļ��������������"), TEXT("����"), MB_ICONWARNING);
				break;
			case RPGERR_BROKENLINK:
				MessageBox(hwndMain, TEXT("�ļ�����δ��ȷ���ӵĶ���"), TEXT("����"), MB_ICONWARNING);
				break;
			case RPGERR_BROKENSYMLINK:
				MessageBox(hwndMain, TEXT("�ļ�����δ��ȷ���ӵķ���"), TEXT("����"), MB_ICONWARNING);
				break;
			case RPGERR_UNKNOWNTYPE:
				MessageBox(hwndMain, TEXT("�ļ�����δ֪���͵�����"), TEXT("����"), MB_ICONWARNING);
				break;
			default:
				MessageBox(hwndMain, TEXT("���ļ�ʱ����δ֪����"), TEXT("����"), MB_ICONERROR);
			}

#ifdef _DEBUG
			if (d_size() != 0)
				MessageBox(hwndMain, TEXT("�ڴ�δ���ͷ����"), TEXT("����"), MB_ICONWARNING);
			else
				d_clear_cnt(); // ������һ���ҵ��ڴ�й©��, Ȼ����ж������
#endif
		}
	}
}

/* ���б���е���Ŀ */
void OpenItem(int iItem)
{
	int nCount = ListView_GetSelectedCount(hwndList); // �б��ѡ����ĸ���
	LVFINDINFO lvfi;
	LVITEM lvi;
	TVITEM tvi;
	struct ruby_object *obj;
	if (nCount != 1)
		return; // ֻ�ܴ�һ����Ŀ

	// ��ȡѡ����Ŀ��Ӧ��Ruby����
	if (iItem == -1)
		lvi.iItem = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); // �Զ���ȡѡ����
	else
		lvi.iItem = iItem;
	lvi.iSubItem = 0;
	lvi.mask = LVIF_PARAM;
	ListView_GetItem(hwndList, &lvi);

	obj = (struct ruby_object *)lvi.lParam; // obj=NULLʱһ����..�ļ���
	tvi.mask = TVIF_PARAM;
	if (obj == NULL || rpgdata_isfolder(obj->type) || obj->type == RBT_LINK)
	{
		/* �����ӻ��ļ��� */
		if (obj == NULL || obj->type != RBT_LINK)
		{
			/* ���ļ��� (����..) */
			tvi.hItem = TreeView_GetSelection(hwndTree); // ���ؼ��еĵ�ǰѡ����
			if (obj != NULL)
			{
				/* ����ͨ�ļ��� */
				tvi.hItem = TreeView_GetChild(hwndTree, tvi.hItem); // ��ȡ��һ�����ļ���
				while (tvi.hItem != NULL)
				{
					TreeView_GetItem(hwndTree, &tvi);
					if (tvi.lParam == lvi.lParam)
						break;
					tvi.hItem = TreeView_GetNextSibling(hwndTree, tvi.hItem);
				}
			}
			else
			{
				/* ��..�ļ��� */
				TreeView_GetItem(hwndTree, &tvi); // ��ǰѡ�������Ϣ
				lvfi.lParam = tvi.lParam; // ��ǰѡ��������Ӧ��Ruby����
				tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem); // �ϲ��ļ���
			}
		}
		else
		{
			/* �����Ӷ���(obj->adata)���ڵ��ļ��� */
			tvi.hItem = GetParentFolder(obj->adata, NULL);
			if (rpgdata_isfolder(obj->adata->type))
			{
				// ������Ӷ�����һ���ļ���, ��ֱ�ӽ�������ļ���
				// �����Զ�ѡ��..�ļ���
				lvfi.lParam = (LPARAM)NULL;
				tvi.hItem = TreeView_GetChild(hwndTree, tvi.hItem);
				while (tvi.hItem != NULL)
				{
					TreeView_GetItem(hwndTree, &tvi);
					if (tvi.lParam == (LPARAM)obj->adata)
						break;
					tvi.hItem = TreeView_GetNextSibling(hwndTree, tvi.hItem);
				}
			}
			else
			{
				// ������Ӷ������ļ���, �����ö������ڵ��ļ���
				// ���Զ��ڸ��ļ�����ѡ���������
				lvfi.lParam = (LPARAM)obj->adata;
			}
		}

		// ѡ�����ؼ��ж�Ӧ���ļ���
		// ѡ�к���Զ�����TVN_SELCHANGED�¼�
		if (tvi.hItem != NULL)
		{
			if (tvi.hItem != TreeView_GetSelection(hwndTree))
			{
				// ���Ŀ���ļ��к͵�ǰ�ļ��в�ͬ
				TreeView_SelectItem(hwndTree, tvi.hItem); // ѡ��Ŀ���ļ���, �����б������ʾ���ļ��е�����
				TreeView_Expand(hwndTree, tvi.hItem, TVE_EXPAND); // չ��Ŀ���ļ���
			}
			else
			{
				// �����ͬ, ����Ҫȡ��ѡ���б���еĵ�ǰ��
				if (iItem != -1)
					ListView_SetItemState(hwndList, iItem, (UINT)NULL, LVIS_SELECTED);
			}

			if (obj == NULL || obj->type == RBT_LINK)
			{
				// �Զ�ѡ���б���е��ϲ��ļ��л����ӵĶ���
				lvfi.flags = LVFI_PARAM;
				lvi.iItem = ListView_FindItem(hwndList, -1, &lvfi); // ��ʱ�б���е������Ѿ�����
				if (lvi.iItem != -1)
					ListView_SetItemState(hwndList, lvi.iItem, LVIS_SELECTED, LVIS_SELECTED);
			}
		}
	}
}

/* ����״̬������ */
void UpdateStatusText(int iItem)
{
	BOOL bParentItem = FALSE;
	HTREEITEM hFolder;
	LPTSTR pszText, pTemp;
	LPCTSTR pszTitle = TEXT("·��: ");
	LVITEM lvi;
	TCHAR szText[20];
	int cchText, cchTitle;
	int nCount = ListView_GetSelectedCount(hwndList);
	if (nCount <= 1)
	{
		lvi.lParam = (LPARAM)NULL;
		if (iItem >= 0)
		{
			// ��ȡ�б��ǰѡ����
			lvi.iItem = iItem;
			lvi.iSubItem = 0;
			lvi.mask = LVIF_PARAM;
			ListView_GetItem(hwndList, &lvi);
			if (lvi.lParam == (LPARAM)NULL)
				bParentItem = TRUE; // ѡ�����..�ļ���
		}
		hFolder = TreeView_GetSelection(hwndTree);
		pszText = GetItemPath(hFolder, (struct ruby_object *)lvi.lParam, bParentItem);
		if (pszText != NULL && pszText[0] != '\0')
		{
			cchTitle = lstrlen(pszTitle);
			cchText = lstrlen(pszText);
			pTemp = (LPTSTR)realloc(pszText, (cchTitle + cchText + 1) * sizeof(TCHAR));
			if (pTemp != NULL)
			{
				pszText = pTemp;
				memmove(pszText + cchTitle, pszText, (cchText + 1) * sizeof(TCHAR)); // �ƶ�����
				memcpy(pszText, pszTitle, cchTitle * sizeof(TCHAR)); // ��ӱ���
			}
		}
		SendMessage(hwndStatus, SB_SETTEXT, SB_SIMPLEID, (LPARAM)pszText);
		free(pszText);
	}
	else
	{
		StringCchPrintf(szText, sizeof(szText), TEXT("ѡ����%d����Ŀ��"), nCount);
		SendMessage(hwndStatus, SB_SETTEXT, SB_SIMPLEID, (LPARAM)szText);
	}
}

/* ���������ڱ��������� */
void UpdateTitle(void)
{
	TCHAR szTitle[MAX_PATH * 2];
	if (sfi.hIcon != NULL)
		DestroyIcon(sfi.hIcon);
	SHGetFileInfo(szFile, (DWORD)NULL, &sfi, sizeof(SHFILEINFO), SHGFI_DISPLAYNAME | SHGFI_ICON | SHGFI_SMALLICON);
	StringCbPrintf(szTitle, sizeof(szTitle), TEXT("%s - %s"), sfi.szDisplayName, APP_TITLE);
	SetWindowText(hwndMain, szTitle);
}

/* �������ؼ��е����� */
void UpdateTree(void)
{
	HTREEITEM hRoot;
	TVINSERTSTRUCT tvis;
	ZeroMemory(&tvis, sizeof(tvis));
	
	/* ���ڵ� */
	ImageList_ReplaceIcon(himl, 0, sfi.hIcon); // ���ļ�ͼ�긴�Ƶ�ͼ���б���
	TreeView_DeleteAllItems(hwndTree); // ������ؼ��е���������
	tvis.item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_TEXT;
	tvis.item.iImage = tvis.item.iSelectedImage = 0; // ʹ�øղŸ��Ƶ��ļ�ͼ��
	tvis.item.pszText = sfi.szDisplayName; // �ļ���
	tvis.item.lParam = (LPARAM)&filecontent;
	hRoot = TreeView_InsertItem(hwndTree, &tvis);
	AddTreeChildren(hRoot, &filecontent);
	
	// Ĭ��Ϊչ����ѡ��״̬
	TreeView_Expand(hwndTree, hRoot, TVE_EXPAND);
	TreeView_SelectItem(hwndTree, hRoot);
}

/* 
��UTF-8�ַ���ת��ΪUTF-16��ANSI����
ʹ��������ͷ��ڴ�
*/
LPTSTR UTF8ToTStr(char *ustr)
{
	// ��UTF-8ת��ΪUTF-16
	int n = MultiByteToWideChar(CP_UTF8, (DWORD)NULL, ustr, -1, NULL, (int)NULL);
	wchar_t *wstr = (wchar_t *)malloc(n * sizeof(wchar_t));
#ifndef _UNICODE
	char *str;
#endif
	MultiByteToWideChar(CP_UTF8, (DWORD)NULL, ustr, -1, wstr, n);

#ifdef _UNICODE
	/* ��TCHAR = wchar_tʱ */
	return wstr;
#else
	/* ��TCHAR = charʱ */
	// ��UTF-16ת��ΪANSI
	// �ڼ������İ�ϵͳ�£�ANSI = GB2312
	n = WideCharToMultiByte(CP_ACP, (DWORD)NULL, wstr, -1, NULL, (int)NULL, NULL, NULL);
	str = (char *)malloc(n * sizeof(char));
	WideCharToMultiByte(CP_ACP, (DWORD)NULL, wstr, -1, str, n, NULL, NULL);
	free(wstr);
	return str;
#endif
}
