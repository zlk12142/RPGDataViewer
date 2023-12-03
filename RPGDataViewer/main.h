#define APP_TITLE TEXT("RPG Data Viewer")

#define VIEW_MARGIN 4
#define TREEVIEW_WIDTH 200 // 树控件窗格初始宽度

LRESULT CALLBACK wnd_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK about_dlg_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void add_tree_children(HTREEITEM hParent, struct ruby_object *obj);
void display_folder(struct ruby_object *folder);
struct ruby_object *get_current_folder();
void get_hash_member_name(struct ruby_object *obj, long i, LPTSTR lpStr, int cbSize);
long get_index(struct ruby_object *parent, struct ruby_object *obj);
HTREEITEM get_parent_folder(struct ruby_object *obj, HTREEITEM hItem);
LPTSTR get_item_path(HTREEITEM hFolder, struct ruby_object *obj, BOOL bParentItem);
LPTSTR get_type_string(unsigned char type);
BOOL in_folder(struct ruby_object *obj, struct ruby_object *folder);
void init_image_list();
void init_list_view();
void open_file_dlg();
void open_item(int iItem);
void update_status_text(int iItem);
void update_title();
void update_tree();
LPTSTR utf8_to_tstr(char *ustr);