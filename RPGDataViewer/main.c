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

// 启用XP风格控件
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' language='*' processorArchitecture='*' publicKeyToken='6595b64144ccf1df'\"")

BOOL bResizingView = FALSE;
HIMAGELIST himl; // 图标列表
HWND hwndMain; // 主窗口
HWND hwndTree, hwndList;
HWND hwndStatus;
SHFILEINFO sfi = {0}; // 当前文件的文件信息
TCHAR szFile[MAX_PATH]; // 打开的文件
struct ruby_object filecontent;

/* 主函数 */
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HMENU hMenu;
	HWND hWnd;
	MSG msg;
	WNDCLASSEX wcex;
	InitCommonControls(); // 初始化控件

	/* 注册窗口类 */
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_SIZEWE);
	wcex.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), (UINT)NULL);
	wcex.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), (UINT)NULL);
	wcex.hInstance = hInstance;
	wcex.lpszClassName = TEXT("MainWindow");
	wcex.lpfnWndProc = WndProc;
	wcex.style = CS_DBLCLKS; // 启用双击消息
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, TEXT("注册窗口类失败！"), TEXT("错误"), MB_ICONERROR);
		return 0;
	}

	/* 创建并显示窗口 */
	hMenu = CreateMainMenu();
	hWnd = CreateWindow(wcex.lpszClassName, APP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, hMenu, hInstance, NULL);
	if (!hWnd)
	{
		MessageBox(NULL, TEXT("创建主窗口失败！"), TEXT("错误"), MB_ICONERROR);
		return 0;
	}
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	/* 消息循环 */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

/* 主窗口消息处理函数 */
// WPARAM = Word(16位) Parameter, LPARAM = Long(32位) Parameter
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
		/* 当窗口创建时 */
		hwndMain = hWnd;
		hInstance = GetModuleHandle(NULL);
		ZeroMemory(&filecontent, sizeof(filecontent));
#if _DEBUG
		d_init(); // 开始内存泄漏检查
#endif
		
		// 创建树控件窗格
		hwndTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL, WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, 0, 0, TREEVIEW_WIDTH, 0, hWnd, NULL, hInstance, NULL);
		himl = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 0, 1);
		TreeView_SetImageList(hwndTree, himl, TVSIL_NORMAL);
		
		// 创建列表控件窗格
		hwndList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
		ListView_SetImageList(hwndList, himl, LVSIL_SMALL);
		ListView_SetExtendedListViewStyle(hwndList, LVS_EX_DOUBLEBUFFER); // 启用半透明选择框
		
		InitImageList();
		InitListView();

		// 创建状态栏
		hwndStatus = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
		SendMessage(hwndStatus, SB_SIMPLE, TRUE, (LPARAM)NULL);
		break;
	case WM_DESTROY:
		/* 当窗口已经关闭时 */
		rpgdata_free(&filecontent);
		if (sfi.hIcon != NULL)
			DestroyIcon(sfi.hIcon);
#if _DEBUG
		d_destroy(); // 停止内存泄漏检查
#endif
		PostQuitMessage(0); // 退出消息循环
		break;
	case WM_LBUTTONDBLCLK:
		/* 双击时恢复默认宽度 */
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
		/* 鼠标拖动改变窗格宽度 */
		if (bResizingView)
		{
			size.cx = GET_X_LPARAM(lParam); // 树控件窗格宽度 = 鼠标相对于客户区的位置
			if (size.cx < 0)
				break;
			GetWindowRect(hwndTree, &rect);
			size.cy = rect.bottom - rect.top; // 树控件窗格高度

			// 获取客户区宽度
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
			DisplayFolder((struct ruby_object *)((LPNMTREEVIEW)lParam)->itemNew.lParam); // 选中树控件项时打开文件夹
			UpdateStatusText(-1);
		}
		else if (pnmh->hwndFrom == hwndList)
		{
			if (pnmh->code == LVN_ITEMACTIVATE)
				OpenItem(((LPNMITEMACTIVATE)lParam)->iItem); // 双击或按下回车键时打开对象 (注: 不能使用NMITEMACTIVATE结构体中的lParam成员)
			else if (pnmh->code == LVN_ITEMCHANGED)
			{
				// 选中或取消选中项目时更新状态栏文字
				pnmlv = (LPNMLISTVIEW)lParam;
				if (pnmlv->uNewState & LVIS_SELECTED)
					UpdateStatusText(pnmlv->iItem);
				else
					UpdateStatusText(-1);
			}
		}
		break;
	case WM_SIZE:
		/* 调整窗口大小时 */
		if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
		{
			GetWindowRect(hwndTree, &rect);
			size.cx = rect.right - rect.left; // 树控件窗格宽度
			GetWindowRect(hwndStatus, &rect);
			size.cy = HIWORD(lParam) - (rect.bottom - rect.top); // 树控件窗格高度

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
		return TRUE; // 按回车键可关闭窗口
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

/* 创建主窗口菜单 */
HMENU CreateMainMenu(void)
{
	HMENU hMenu = CreateMenu();
	HMENU hmenuFile = CreatePopupMenu();
	HMENU hmenuHelp = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP | MF_STRING, (UINT_PTR)hmenuFile, TEXT("文件(&F)"));
	AppendMenu(hmenuFile, MF_STRING, IDM_OPEN, TEXT("打开(&O)..."));
	AppendMenu(hmenuFile, MF_SEPARATOR, (UINT_PTR)NULL, NULL);
	AppendMenu(hmenuFile, MF_STRING, IDM_EXIT, TEXT("退出(&E)"));

	AppendMenu(hMenu, MF_POPUP | MF_STRING, (UINT_PTR)hmenuHelp, TEXT("帮助(&H)"));
	AppendMenu(hmenuHelp, MF_STRING, IDM_ABOUT, TEXT("关于(&A)..."));
	return hMenu;
}

/* 显示指定文件夹的内容 */
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

	// 显示顶层文件夹 (规定lParam为NULL)
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

	// 指定快捷方式图标的overlay序号
	// 当lvi.mask带有LVIF_STATE属性时显示
	lvi.state = INDEXTOOVERLAYMASK(1);
	lvi.stateMask = LVIS_OVERLAYMASK;
	
	// 遍历当前文件夹
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
				value = &folder->members[j].value; // 当前成员
			else
				value = &folder->adata[j]; // 当前数组元素

			// 对象的实际数据类型 (忽略对象链接)
			if (value->type == RBT_LINK)
				real_type = value->adata->type;
			else
				real_type = value->type;
			
			// 先显示文件夹, 再显示其他项目
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

			// 名称
			lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT;
			if (value->type == RBT_LINK || value->type == RBT_SYMLINK)
				lvi.mask |= LVIF_STATE; // 显示快捷方式图标
			lvi.iSubItem = 0;
			lvi.lParam = (LPARAM)value; // 将列表项与Ruby对象绑定
			if (rpgdata_isfolder(real_type))
				lvi.iImage = 1; // 文件夹图标
			else if (real_type == RBT_BIGNUM || real_type == RBT_FIXNUM || real_type == RBT_FLOAT || real_type == RBT_USERDEF || real_type == RBT_USRMARSHAL)
				lvi.iImage = 5; // 二进制数据图标
			else
				lvi.iImage = 4; // 字符串图标
			if (folder->type == RBT_IVAR || folder->type == RBT_OBJECT || folder->type == RBT_STRUCT || folder->type == RBT_UCLASS)
			{
				// 对象的属性名
				pMemStr = UTF8ToTStr(folder->members[j].name);
				lvi.pszText = pMemStr;
			}
			else if (folder->type == RBT_HASH || folder->type == RBT_HASH_DEF)
			{
				// 哈希表成员名
				GetHashMemberName(folder, j, szText, sizeof(szText));
				lvi.pszText = szText;
			}
			else if (folder->type == RBT_USRMARSHAL)
				lvi.pszText = TEXT("marshal_dump");
			else
			{
				// 数组的索引序号
				StringCbPrintf(szText, sizeof(szText), TEXT("%ld"), j);
				lvi.pszText = szText;
			}
			ListView_InsertItem(hwndList, &lvi);
			if (pMemStr != NULL)
			{
				free(pMemStr);
				pMemStr = NULL;
			}

			// 类型
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 1;
			lvi.pszText = GetTypeString(value->type);
			ListView_SetItem(hwndList, &lvi);

			// 值
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
					lvi.pszText = TEXT("+∞");
					break;
				case RUBY_FLOAT_NEGINF:
					lvi.pszText = TEXT("-∞");
					break;
				case RUBY_FLOAT_NAN:
					lvi.pszText = TEXT("NaN");
					break;
				default:
					pMemStr = UTF8ToTStr(value->dbl_str); // 直接显示原始字符串
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

// 获取当前显示的文件夹
struct ruby_object *GetCurrentFolder(void)
{
	TVITEM tvi;
	tvi.mask = TVIF_PARAM;
	tvi.hItem = TreeView_GetSelection(hwndTree); // 树控件中的当前选中项
	TreeView_GetItem(hwndTree, &tvi);
	return (struct ruby_object *)tvi.lParam;
}

void GetHashMemberName(struct ruby_object *obj, long i, LPTSTR lpStr, int cbSize)
{
	if (obj->type == RBT_HASH_DEF && i == 0)
		StringCbCopy(lpStr, cbSize, TEXT("默认值"));
	else if ((obj->type == RBT_HASH && i % 2 == 0) || (obj->type == RBT_HASH_DEF && i % 2 == 1))
		StringCbPrintf(lpStr, cbSize, TEXT("键%ld"), i / 2);
	else
		StringCbPrintf(lpStr, cbSize, TEXT("值%ld"), (i - 1) / 2);
}

// 根据数组元素(或对象成员)的值的地址确定数组(或对象)的下标
// 该地址是随着下标线性增长的
long GetIndex(struct ruby_object *parent, struct ruby_object *obj)
{
	if (parent == NULL)
		parent = GetCurrentFolder();
	if (parent->type == RBT_ARRAY || parent->type == RBT_HASH || parent->type == RBT_HASH_DEF)
		return obj - parent->adata;
	else if (parent->type == RBT_IVAR || parent->type == RBT_OBJECT || parent->type == RBT_STRUCT || parent->type == RBT_UCLASS)
		return (struct ruby_member *)obj - (struct ruby_member *)&parent->members[0].value;
	else
		return -1; // 无法确定
}

/* 生成一个Ruby对象的路径字符串 */
// hFolder: 所在的文件夹
// obj: 对象, 如果为NULL则生成hFolder的路径
// bParentItem: 对象是否为..文件夹
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
		// 获取上层文件夹
		for (i = 0; i <= bParentItem; i++) // 次数: 无选择项时一次, 选择了".."文件夹时两次
		{
			TreeView_GetItem(hwndTree, &tvi);
			obj = (struct ruby_object *)tvi.lParam;
			tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem);
		}
	}

	j = 0; // 主字符串下标
	while (TreeView_GetItem(hwndTree, &tvi))
	{
		folder = (struct ruby_object *)tvi.lParam;
		iIndex = GetIndex(folder, obj); // 如果发现iIndex=-1, 则表明此函数工作不正常
		pszName = szText;
		if (folder->type == RBT_ARRAY)
		{
			if (folder == &filecontent)
				StringCbPrintf(szText, sizeof(szText), TEXT("data%ld"), iIndex); // Marshal.dump的序号
			else
				StringCbPrintf(szText, sizeof(szText), TEXT("[%ld]"), iIndex); // 数组的下标
		}
		else if (folder->type == RBT_HASH || folder->type == RBT_HASH_DEF)
		{
			// 哈希表
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
			// 对象的成员名称
			if (strcmp(folder->members[iIndex].name, ".") != 0)
			{
				pszName = UTF8ToTStr(folder->members[iIndex].name);
				bAddDot = TRUE;
			}
			else
				szText[0] = '\0';
		}

		// 逆向复制字符串
		nLen = lstrlen(pszName);
		if (cchSize - j < nLen + 1)
		{
			cchSize += nLen + 1;
			cchSize *= 2; // 容量不够时扩容
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

		// 继续获取上一层文件夹的信息, 直到最顶层
		obj = (struct ruby_object *)tvi.lParam;
		tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem);
	}

	// 倒转字符串
	for (i = 0; i < j / 2; i++)
	{
		temp = pszText[i];
		pszText[i] = pszText[j - i - 1];
		pszText[j - i - 1] = temp;
	}
	pszText[j] = '\0';
	return pszText;
}

// 获取Ruby对象所在的文件夹
// hFolder: 搜索指定文件夹中的各子文件夹, 如果为NULL, 则搜索根文件夹
HTREEITEM GetParentFolder(struct ruby_object *obj, HTREEITEM hFolder)
{
	TVITEM tvi;
	if (hFolder == NULL)
	{
		hFolder = TreeView_GetRoot(hwndTree);
		if (InFolder(obj, &filecontent))
			return hFolder; // 如果在根文件夹下则直接返回
	}
	
	// 遍历hItem文件夹下的所有子节文件夹
	tvi.mask = TVIF_PARAM;
	tvi.hItem = TreeView_GetChild(hwndTree, hFolder);
	while (tvi.hItem != NULL)
	{
		TreeView_GetItem(hwndTree, &tvi);
		if (InFolder(obj, (struct ruby_object *)tvi.lParam))
			return tvi.hItem;
		else
		{
			hFolder = GetParentFolder(obj, tvi.hItem); // 递归遍历子孙文件夹
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
		return TEXT("数组");
	case RBT_BIGNUM:
		return TEXT("长整数");
	case RBT_CLASS:
		return TEXT("类");
	case RBT_FALSE:
	case RBT_TRUE:
		return TEXT("布尔");
	case RBT_FIXNUM:
		return TEXT("整数");
	case RBT_FLOAT:
		return TEXT("浮点数");
	case RBT_HASH:
	case RBT_HASH_DEF:
		return TEXT("哈希表");
	case RBT_IVAR:
	case RBT_UCLASS:
		return TEXT("扩展对象");
	case RBT_LINK:
		return TEXT("链接");
	case RBT_MODULE:
		return TEXT("模块");
	case RBT_NIL:
		return TEXT("空");
	case RBT_OBJECT:
		return TEXT("对象");
	case RBT_REGEXP:
		return TEXT("正则表达式");
	case RBT_STRUCT:
		return TEXT("结构体");
	case RBT_STRING:
		return TEXT("字符串");
	case RBT_SYMBOL:
		return TEXT("符号");
	case RBT_SYMLINK:
		return TEXT("符号链接");
	case RBT_USERDEF:
		return TEXT("二进制数据");
	case RBT_USRMARSHAL:
		return TEXT("自定义导出数据");
	default:
		return TEXT("未知类型");
	}
}

// 判断一个Ruby对象是否在一个文件夹中 (不考虑子文件夹)
// 注意: 符号对象可能不位于任意一个文件夹, 而是在某个对象的symbol_ref中
BOOL InFolder(struct ruby_object *obj, struct ruby_object *folder)
{
	int size; // 数据域的大小
	if (obj < folder->adata)
		return FALSE; // 若obj的地址位于folder数据域的地址之前, 则不可能在这个文件夹中
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

	return ((unsigned char *)obj - folder->data < size); // 只要obj的地址位于folder数据域地址之间, 就认为obj在folder中
}

/* 初始化图像列表 */
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
	
	ImageList_SetOverlayImage(himl, 6, 1); // 快捷方式图标
	for (i = 0; i < _countof(hmdl); i++)
		FreeLibrary(hmdl[i]);
}

void InitListView(void)
{
	LPTSTR strarr[] = {TEXT("名称"), TEXT("类型"), TEXT("值")};
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

/* 显示打开文件的对话框 */
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
	ofn.lpstrFilter = TEXT("RPG Maker XP 数据文件(*.rxdata)\0*.rxdata\0所有文件\0*\0");
	ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == IDOK)
	{
		rpgdata_free(&filecontent);
		uRet = rpgdata_load(szFile, &filecontent);
		if (uRet == 0)
		{
			// 成功打开文件
			UpdateTitle();
			UpdateTree();
		}
		else
		{
			rpgdata_free(&filecontent);
			switch (uRet)
			{
			case RPGERR_NOTOPEN:
				MessageBox(hwndMain, TEXT("打开文件失败"), TEXT("错误"), MB_ICONWARNING);
				break;
			case RPGERR_FORMAT:
				MessageBox(hwndMain, TEXT("无法识别该文件的格式"), TEXT("错误"), MB_ICONWARNING);
				break;
			case RPGERR_MEMORY:
				MessageBox(hwndMain, TEXT("系统内存不足"), TEXT("错误"), MB_ICONERROR);
				break;
			case RPGERR_DATA:
				MessageBox(hwndMain, TEXT("文件数据有误或不完整"), TEXT("错误"), MB_ICONWARNING);
				break;
			case RPGERR_BROKENLINK:
				MessageBox(hwndMain, TEXT("文件中有未正确链接的对象"), TEXT("错误"), MB_ICONWARNING);
				break;
			case RPGERR_BROKENSYMLINK:
				MessageBox(hwndMain, TEXT("文件中有未正确链接的符号"), TEXT("错误"), MB_ICONWARNING);
				break;
			case RPGERR_UNKNOWNTYPE:
				MessageBox(hwndMain, TEXT("文件中有未知类型的数据"), TEXT("错误"), MB_ICONWARNING);
				break;
			default:
				MessageBox(hwndMain, TEXT("打开文件时发生未知错误"), TEXT("错误"), MB_ICONERROR);
			}

#ifdef _DEBUG
			if (d_size() != 0)
				MessageBox(hwndMain, TEXT("内存未被释放完毕"), TEXT("警告"), MB_ICONWARNING);
			else
				d_clear_cnt(); // 便于下一次找到内存泄漏点, 然后进行定点测试
#endif
		}
	}
}

/* 打开列表框中的项目 */
void OpenItem(int iItem)
{
	int nCount = ListView_GetSelectedCount(hwndList); // 列表框选中项的个数
	LVFINDINFO lvfi;
	LVITEM lvi;
	TVITEM tvi;
	struct ruby_object *obj;
	if (nCount != 1)
		return; // 只能打开一个项目

	// 获取选中项目对应的Ruby对象
	if (iItem == -1)
		lvi.iItem = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); // 自动获取选中项
	else
		lvi.iItem = iItem;
	lvi.iSubItem = 0;
	lvi.mask = LVIF_PARAM;
	ListView_GetItem(hwndList, &lvi);

	obj = (struct ruby_object *)lvi.lParam; // obj=NULL时一定是..文件夹
	tvi.mask = TVIF_PARAM;
	if (obj == NULL || rpgdata_isfolder(obj->type) || obj->type == RBT_LINK)
	{
		/* 打开链接或文件夹 */
		if (obj == NULL || obj->type != RBT_LINK)
		{
			/* 打开文件夹 (包括..) */
			tvi.hItem = TreeView_GetSelection(hwndTree); // 树控件中的当前选中项
			if (obj != NULL)
			{
				/* 打开普通文件夹 */
				tvi.hItem = TreeView_GetChild(hwndTree, tvi.hItem); // 获取第一个子文件夹
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
				/* 打开..文件夹 */
				TreeView_GetItem(hwndTree, &tvi); // 当前选中项的信息
				lvfi.lParam = tvi.lParam; // 当前选中项所对应的Ruby对象
				tvi.hItem = TreeView_GetParent(hwndTree, tvi.hItem); // 上层文件夹
			}
		}
		else
		{
			/* 打开链接对象(obj->adata)所在的文件夹 */
			tvi.hItem = GetParentFolder(obj->adata, NULL);
			if (rpgdata_isfolder(obj->adata->type))
			{
				// 如果链接对象是一个文件夹, 则直接进入这个文件夹
				// 并且自动选中..文件夹
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
				// 如果链接对象不是文件夹, 则进入该对象所在的文件夹
				// 并自动在该文件夹中选中这个对象
				lvfi.lParam = (LPARAM)obj->adata;
			}
		}

		// 选中树控件中对应的文件夹
		// 选中后会自动触发TVN_SELCHANGED事件
		if (tvi.hItem != NULL)
		{
			if (tvi.hItem != TreeView_GetSelection(hwndTree))
			{
				// 如果目标文件夹和当前文件夹不同
				TreeView_SelectItem(hwndTree, tvi.hItem); // 选中目标文件夹, 并在列表框中显示该文件夹的内容
				TreeView_Expand(hwndTree, tvi.hItem, TVE_EXPAND); // 展开目标文件夹
			}
			else
			{
				// 如果相同, 则需要取消选中列表框中的当前项
				if (iItem != -1)
					ListView_SetItemState(hwndList, iItem, (UINT)NULL, LVIS_SELECTED);
			}

			if (obj == NULL || obj->type == RBT_LINK)
			{
				// 自动选中列表框中的上层文件夹或被链接的对象
				lvfi.flags = LVFI_PARAM;
				lvi.iItem = ListView_FindItem(hwndList, -1, &lvfi); // 此时列表框中的内容已经更新
				if (lvi.iItem != -1)
					ListView_SetItemState(hwndList, lvi.iItem, LVIS_SELECTED, LVIS_SELECTED);
			}
		}
	}
}

/* 更新状态栏文字 */
void UpdateStatusText(int iItem)
{
	BOOL bParentItem = FALSE;
	HTREEITEM hFolder;
	LPTSTR pszText, pTemp;
	LPCTSTR pszTitle = TEXT("路径: ");
	LVITEM lvi;
	TCHAR szText[20];
	int cchText, cchTitle;
	int nCount = ListView_GetSelectedCount(hwndList);
	if (nCount <= 1)
	{
		lvi.lParam = (LPARAM)NULL;
		if (iItem >= 0)
		{
			// 获取列表框当前选中项
			lvi.iItem = iItem;
			lvi.iSubItem = 0;
			lvi.mask = LVIF_PARAM;
			ListView_GetItem(hwndList, &lvi);
			if (lvi.lParam == (LPARAM)NULL)
				bParentItem = TRUE; // 选择的是..文件夹
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
				memmove(pszText + cchTitle, pszText, (cchText + 1) * sizeof(TCHAR)); // 移动内容
				memcpy(pszText, pszTitle, cchTitle * sizeof(TCHAR)); // 添加标题
			}
		}
		SendMessage(hwndStatus, SB_SETTEXT, SB_SIMPLEID, (LPARAM)pszText);
		free(pszText);
	}
	else
	{
		StringCchPrintf(szText, sizeof(szText), TEXT("选中了%d个项目。"), nCount);
		SendMessage(hwndStatus, SB_SETTEXT, SB_SIMPLEID, (LPARAM)szText);
	}
}

/* 更新主窗口标题栏文字 */
void UpdateTitle(void)
{
	TCHAR szTitle[MAX_PATH * 2];
	if (sfi.hIcon != NULL)
		DestroyIcon(sfi.hIcon);
	SHGetFileInfo(szFile, (DWORD)NULL, &sfi, sizeof(SHFILEINFO), SHGFI_DISPLAYNAME | SHGFI_ICON | SHGFI_SMALLICON);
	StringCbPrintf(szTitle, sizeof(szTitle), TEXT("%s - %s"), sfi.szDisplayName, APP_TITLE);
	SetWindowText(hwndMain, szTitle);
}

/* 更新树控件中的内容 */
void UpdateTree(void)
{
	HTREEITEM hRoot;
	TVINSERTSTRUCT tvis;
	ZeroMemory(&tvis, sizeof(tvis));
	
	/* 根节点 */
	ImageList_ReplaceIcon(himl, 0, sfi.hIcon); // 将文件图标复制到图像列表中
	TreeView_DeleteAllItems(hwndTree); // 清除树控件中的现有内容
	tvis.item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_TEXT;
	tvis.item.iImage = tvis.item.iSelectedImage = 0; // 使用刚才复制的文件图标
	tvis.item.pszText = sfi.szDisplayName; // 文件名
	tvis.item.lParam = (LPARAM)&filecontent;
	hRoot = TreeView_InsertItem(hwndTree, &tvis);
	AddTreeChildren(hRoot, &filecontent);
	
	// 默认为展开和选中状态
	TreeView_Expand(hwndTree, hRoot, TVE_EXPAND);
	TreeView_SelectItem(hwndTree, hRoot);
}

/* 
将UTF-8字符串转换为UTF-16或ANSI编码
使用完必须释放内存
*/
LPTSTR UTF8ToTStr(char *ustr)
{
	// 将UTF-8转换为UTF-16
	int n = MultiByteToWideChar(CP_UTF8, (DWORD)NULL, ustr, -1, NULL, (int)NULL);
	wchar_t *wstr = (wchar_t *)malloc(n * sizeof(wchar_t));
#ifndef _UNICODE
	char *str;
#endif
	MultiByteToWideChar(CP_UTF8, (DWORD)NULL, ustr, -1, wstr, n);

#ifdef _UNICODE
	/* 当TCHAR = wchar_t时 */
	return wstr;
#else
	/* 当TCHAR = char时 */
	// 将UTF-16转换为ANSI
	// 在简体中文版系统下，ANSI = GB2312
	n = WideCharToMultiByte(CP_ACP, (DWORD)NULL, wstr, -1, NULL, (int)NULL, NULL, NULL);
	str = (char *)malloc(n * sizeof(char));
	WideCharToMultiByte(CP_ACP, (DWORD)NULL, wstr, -1, str, n, NULL, NULL);
	free(wstr);
	return str;
#endif
}
