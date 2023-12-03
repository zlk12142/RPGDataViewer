#ifdef _DEBUG

/* 内存泄漏检查 */
#define malloc d_malloc
#define realloc d_realloc
#define free d_free

/* 定点内存分配失败 */
//#define D_MALLOC_ID 102
//#define D_MALLOC_ID d_id
//#define D_REALLOC_ID 69
//#define D_REALLOC_ID d_id

/* 随机内存分配失败 */
//#define D_MALLOC_FAIL 10
//#define D_REALLOC_FAIL 10

void d_clear_cnt(void);
void d_destroy(void);
BOOL d_fail(char rate);
void d_free(void *mem);
void d_id_increase(void);
void d_init(void);
void *d_malloc(size_t size);
void d_memory_test(LPTSTR file);
void *d_realloc(void *mem, size_t size);
size_t d_size(void);

#endif