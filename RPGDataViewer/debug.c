#ifdef _DEBUG
#include <time.h>
#include <Windows.h>
#include <strsafe.h>
#include "debug.h"
#include "rpgdata.h"

static size_t d_allocated_size;
static unsigned int d_id = 1;
static unsigned int d_malloc_cnt;
static unsigned int d_realloc_cnt;
static HANDLE d_hHeap;

void d_clear_cnt()
{
	d_malloc_cnt = 0;
	d_realloc_cnt = 0;
}

void d_destroy()
{
	TCHAR str[100];

	if (d_allocated_size != 0)
	{
		StringCbPrintf(str, sizeof(str), TEXT("����%ld�ֽ��ڴ�δ���ͷţ�\nmallocִ�д���: %u\nreallocִ�д���: %u"), d_allocated_size, d_malloc_cnt, d_realloc_cnt);
		MessageBox(NULL, str, TEXT("����"), MB_ICONWARNING);
	}
	HeapDestroy(d_hHeap);
}

/* ���ʧ�� */
BOOL d_fail(char rate)
{
	return rand() % 100 < rate;
}

void d_free(void *mem)
{
	size_t size;

	if (mem == NULL)
		return;
	size = HeapSize(d_hHeap, 0, mem);
	HeapFree(d_hHeap, 0, mem);
	d_allocated_size -= size;
}

void d_id_increase()
{
	d_id++;
}

void d_init()
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
		MessageBox(NULL, TEXT("��ͼ����0�ֽڵ��ڴ�ռ�"), TEXT("����"), MB_ICONWARNING);
		return NULL;
	}
	p = HeapAlloc(d_hHeap, 0, size);
	if (p != NULL)
		d_allocated_size += size;
	return p;
}

// �ڴ�й©����, ʹ��ʱD_MALLOC_ID��D_REALLOC_ID�������ҽ���һ��Ϊd_id, ������򽫻�������ѭ��
// ������ڴ�й©, �����־����
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
	} while (d_id < 200); // ע��: realloc������С�ڴ�ʱ��Զ�������, ��ʹģ�����, �ļ�Ҳ��������, ��˲��ܰ��ļ��Ƿ�ɹ�����Ϊѭ������������
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
	old_size = HeapSize(d_hHeap, 0, mem);
#ifdef D_REALLOC_ID
	if (old_size < size && d_realloc_cnt == D_REALLOC_ID)
		return NULL;
#elif defined(D_REALLOC_FAIL)
	if (old_size < size && d_fail(D_REALLOC_FAIL))
		return NULL;
#endif
	if (size == 0)
	{
		MessageBox(NULL, TEXT("��ͼ����0�ֽڵ��ڴ�ռ�"), TEXT("����"), MB_ICONWARNING);
		return NULL;
	}
	p = HeapReAlloc(d_hHeap, 0, mem, size);
	if (p != NULL)
		d_allocated_size += size - old_size;
	return p;
}

size_t d_size()
{
	return d_allocated_size;
}
#endif