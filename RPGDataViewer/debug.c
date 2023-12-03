#ifdef _DEBUG
#include <time.h>
#include <Windows.h>
#include <strsafe.h>
#include "debug.h"
#include "rpgdata.h"

static HANDLE d_hHeap;
static size_t d_allocated_size = 0;
static unsigned int d_id = 1;
static unsigned int d_malloc_cnt = 0;
static unsigned int d_realloc_cnt = 0;

void d_clear_cnt(void)
{
	d_malloc_cnt = 0;
	d_realloc_cnt = 0;
}

void d_destroy(void)
{
	TCHAR str[100];
	if (d_allocated_size != 0)
	{
		StringCbPrintf(str, sizeof(str), TEXT("还有%ld字节内存未被释放！\nmalloc执行次数: %u\nrealloc执行次数: %u"), d_allocated_size, d_malloc_cnt, d_realloc_cnt);
		MessageBox(NULL, str, TEXT("警告"), MB_ICONWARNING);
	}
	HeapDestroy(d_hHeap);
}

/* 随机失败 */
BOOL d_fail(char rate)
{
	return rand() % 100 < rate;
}

void d_free(void *mem)
{
	size_t size;
	if (mem == NULL)
		return;
	size = HeapSize(d_hHeap, (DWORD)NULL, mem);
	HeapFree(d_hHeap, (DWORD)NULL, mem);
	d_allocated_size -= size;
}

void d_id_increase(void)
{
	d_id++;
}

void d_init(void)
{
	srand((unsigned int)time(NULL));
	d_hHeap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
}

void *d_malloc(size_t size)
{
	void *p;
	d_malloc_cnt++;
#ifdef D_MALLOC_ID
	if (d_realloc_cnt == D_MALLOC_ID)
		return NULL;
#elif defined(D_MALLOC_FAIL)
	if (d_fail(D_MALLOC_FAIL))
		return NULL;
#endif
	if (size == 0)
	{
		MessageBox(NULL, TEXT("试图分配0字节的内存空间"), TEXT("警告"), MB_ICONWARNING);
		return NULL;
	}
	p = HeapAlloc(d_hHeap, (DWORD)NULL, size);
	if (p != NULL)
		d_allocated_size += size;
	return p;
}

// 内存泄漏测试, 使用时D_MALLOC_ID或D_REALLOC_ID必须有且仅有一个为d_id, 否则程序将会陷入死循环
// 如果有内存泄漏, 则会出现警告框
void d_memory_test(LPTSTR file)
{
	int result;
	struct ruby_object obj;
	d_init();
	do
	{
		d_clear_cnt();
		result = rpgdata_load(file, &obj);
		rpgdata_free(&obj);
		d_id_increase();
		if (d_size() > 0)
			break;
#ifdef D_MALLOC_ID
	} while (result != 0);
#elif defined(D_REALLOC_ID)
	} while (d_id < 200); // 注意: realloc用于缩小内存时永远不会出错, 即使模拟出错, 文件也能正常打开, 因此不能把文件是否成功打开作为循环结束的条件
#else
	} while (0);
#endif
	d_destroy();
}

void *d_realloc(void *mem, size_t size)
{
	size_t old_size;
	void *p;
	if (mem == NULL)
		return d_malloc(size);
	d_realloc_cnt++;
	old_size = HeapSize(d_hHeap, (DWORD)NULL, mem);
#ifdef D_REALLOC_ID
	if (old_size < size && d_realloc_cnt == D_REALLOC_ID)
		return NULL;
#elif defined(D_REALLOC_FAIL)
	if (old_size < size && d_fail(D_REALLOC_FAIL))
		return NULL;
#endif
	if (size == 0)
	{
		MessageBox(NULL, TEXT("试图分配0字节的内存空间"), TEXT("警告"), MB_ICONWARNING);
		return NULL;
	}
	p = HeapReAlloc(d_hHeap, (DWORD)NULL, mem, size);
	if (p != NULL)
		d_allocated_size += size - old_size;
	return p;
}

size_t d_size(void)
{
	return d_allocated_size;
}
#endif