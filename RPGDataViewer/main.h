#define APP_TITLE TEXT("RPG Data Viewer")

#define IDM_OPEN 101
#define IDM_EXIT 102
#define IDM_ABOUT 103

#define VIEW_MARGIN 4
#define TREEVIEW_WIDTH 200 // 树控件窗格初始宽度

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AddTreeChildren(HTREEITEM hParent, struct ruby_object *obj);
HMENU CreateMainMenu(void);
void DisplayFolder(struct ruby_object *folder);
struct ruby_object *GetCurrentFolder(void);
void GetHashMemberName(struct ruby_object *obj, long i, LPTSTR lpStr, int cbSize);
long GetIndex(struct ruby_object *parent, struct ruby_object *obj);
HTREEITEM GetParentFolder(struct ruby_object *obj, HTREEITEM hItem);
LPTSTR GetItemPath(HTREEITEM hFolder, struct ruby_object *obj, BOOL bParentItem);
LPTSTR GetTypeString(unsigned char type);
BOOL InFolder(struct ruby_object *obj, struct ruby_object *folder);
void InitImageList(void);
void InitListView(void);
void OpenFileDlg(void);
void OpenItem(int iItem);
void UpdateStatusText(int iItem);
void UpdateTitle(void);
void UpdateTree(void);
LPTSTR UTF8ToTStr(char *ustr);