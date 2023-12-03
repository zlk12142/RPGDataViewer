#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include "debug.h"
#include "rpgdata.h"

static struct ruby_list rb_objlist = {0}, rb_symlist = {0};

BOOL rblist_add(struct ruby_list *list, struct ruby_object *item)
{
	void *p;
	if (list->length == list->capacity)
	{
		list->capacity += RBLIST_STEP;
		p = realloc(list->items, list->capacity * sizeof(struct ruby_object *));
		if (p == NULL)
			return FALSE;
		list->items = (struct ruby_object **)p;
	}
	list->items[list->length] = item;
	list->length++;
	return TRUE;
}

struct ruby_object *rblist_get(struct ruby_list *list, long id)
{
	if (id >= list->length)
		return NULL;
	return list->items[id];
}

void rblist_reset(struct ruby_list *list)
{
	if (list->items != NULL)
		free(list->items);
	memset(list, 0, sizeof(struct ruby_list)); // 清空整个结构体
}

/* 长整数对象转换为十六进制字符串 */
LPTSTR rpgdata_bignum_to_string(struct ruby_object *obj)
{
	BOOL zero_flag = TRUE; // 去掉多余前导0的标记
	const char *list = "0123456789abcdef";
	LPTSTR str;
	unsigned long size;
	unsigned long i, j = 0;
	if (obj->type != RBT_BIGNUM)
		return NULL;

	size = 2 * obj->length + 1;
	if (obj->data[0] == '-')
		size++;
	str = (LPTSTR)malloc(size * sizeof(TCHAR));
	if (str == NULL)
		return NULL;
	if (obj->data[0] == '-')
		str[j++] = '-'; 
	str[j++] = '0';
	str[j++] = 'x';

	for (i = obj->length - 1; i > 0; i--)
	{
		if (zero_flag && obj->data[i] == 0)
			continue;
		if (!zero_flag || (obj->data[i] & 0xf0) != 0)
			str[j++] = list[obj->data[i] >> 4];
		str[j++] = list[obj->data[i] & 0x0f];
		if (zero_flag)
			zero_flag = FALSE;
	}

	str[j] = '\0';
	return str;
}

/* 释放存储在ruby_object结构中的数据所占用的内存 */
/* 如果要释放结构体本身, 请直接调用free函数 */
void rpgdata_free(struct ruby_object *obj)
{
	long i, j;
	if (obj == NULL)
		return;
	if (obj->extension != NULL)
	{
		rpgdata_free(obj->extension);
		free(obj->extension);
		obj->extension = NULL;
	}
	switch (obj->type)
	{
	case RBT_ARRAY:
	case RBT_HASH:
	case RBT_HASH_DEF:
		j = obj->length;
		if (obj->type == RBT_HASH || obj->type == RBT_HASH_DEF)
			j *= 2;
		if (obj->type == RBT_HASH_DEF)
			j++;
		if (obj->adata != NULL)
		{
			for (i = 0; i < j; i++)
				rpgdata_free(&obj->adata[i]);
			free(obj->adata);
			obj->adata = NULL;
		}
		break;
	case RBT_BIGNUM:
	case RBT_REGEXP:
	case RBT_STRING:
		free(obj->data);
		obj->data = NULL;
		break;
	case RBT_CLASS:
	case RBT_MODULE:
	case RBT_SYMBOL:
		free(obj->name);
		obj->name = NULL;
		break;
	case RBT_FLOAT:
		free(obj->dbl_str);
		obj->dbl_str = NULL;
		break;
	case RBT_IVAR:
	case RBT_OBJECT:
	case RBT_STRUCT:
	case RBT_UCLASS:
		if (obj->members != NULL)
		{
			for (i = 0; i < obj->length; i++)
				rpgdata_free_member(&obj->members[i]);
			free(obj->members);
			obj->members = NULL;
		}
		rpgdata_free_symbol(obj->symbol_ref);
		obj->symbol_ref = NULL;
		break;
	case RBT_USERDEF:
	case RBT_USRMARSHAL:
		if (obj->type == RBT_USRMARSHAL)
			rpgdata_free(obj->adata);
		free(obj->data);
		obj->data = NULL;
		rpgdata_free_symbol(obj->symbol_ref);
		obj->symbol_ref = NULL;
		break;
	}
}

void rpgdata_free_member(struct ruby_member *member)
{
	rpgdata_free(&member->value);
	rpgdata_free_symbol(member->symbol_ref);
}

/* 释放符号对象或符号链接对象 */
// 当obj一定是符号对象时, 或者当obj可能是符号对象或符号链接时，请调用此函数来释放内存
// 如果已经确定obj一定是符号链接, 那么只需直接调用free函数
void rpgdata_free_symbol(struct ruby_object *obj)
{
	if (obj == NULL)
		return;
	rpgdata_free(obj); // 释放存储在符号对象中的数据
	free(obj); // 释放对象本身所占用的内存
}

BOOL rpgdata_isfolder(unsigned char type)
{
	return (type == RBT_ARRAY || type == RBT_HASH || type == RBT_HASH_DEF || type == RBT_IVAR || type == RBT_OBJECT || type == RBT_STRUCT || type == RBT_UCLASS || type == RBT_USRMARSHAL);
}

int rpgdata_load(LPCTSTR filename, struct ruby_object *obj)
{
	FILE *fp;
	int ret = 0;
	void *p;

	_tfopen_s(&fp, filename, TEXT("rb"));
	if (fp == NULL)
		return RPGERR_NOTOPEN;

	memset(obj, 0, sizeof(struct ruby_object));
	obj->type = RBT_ARRAY;
	obj->length = 0;
	obj->adata = NULL;
	while (ret == 0 && rpgdata_read_header(fp))
	{
		p = realloc(obj->adata, (obj->length + 1) * sizeof(struct ruby_object));
		if (p == NULL)
		{
			ret = RPGERR_MEMORY;
			break;
		}
		obj->adata = (struct ruby_object *)p;

		ret = rpgdata_read(fp, &obj->adata[obj->length]);
		obj->length++;
		rpgdata_reset();
	}
	if (obj->length == 0)
		ret = RPGERR_FORMAT;
	fclose(fp);
	return ret;
}

/* 
读取文件中接下来的符号对象或符号链接所指向的符号对象
以该符号的名称作为对象名，创建并注册对象，返回所创建的对象
*/
int rpgdata_prepare_object(FILE *fp, struct ruby_object *obj, BOOL has_length)
{
	// 在Ruby中，类名和成员名都是符号，且同名符号只能在内存中存在一次
	int result;
	struct ruby_object *sym;
	result = rpgdata_read_symbol_block(fp, &sym); // 类名
	if (result != 0)
		return result;

	obj->name = sym->name; // 只要符号对象存在, 这个字符串就不会失效
	if (sym->type == RBT_SYMBOL)
		obj->symbol_ref = sym;
	else
		free(sym);
	if (has_length)
	{
		if (!rpgdata_read_long(fp, &obj->length) || obj->length < 0)
			return RPGERR_DATA;
	}
	else
		obj->length = 0;

	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	return 0;
}

int rpgdata_read(FILE *fp, struct ruby_object *obj)
{
	memset(obj, 0, sizeof(struct ruby_object));
	obj->type = fgetc(fp);
	if (feof(fp))
		return RPGERR_DATA;
	switch (obj->type)
	{
	case RBT_ARRAY:
		return rpgdata_read_array(fp, obj);
	case RBT_BIGNUM:
		return rpgdata_read_bignum(fp, obj);
	case RBT_CLASS:
	case RBT_MODULE:
		return rpgdata_read_class(fp, obj);
	case RBT_EXTENDED:
		return rpgdata_read_extended(fp, obj);
	case RBT_FALSE:
	case RBT_TRUE:
	case RBT_NIL:
		return 0;
	case RBT_FIXNUM:
		return rpgdata_read_fixnum(fp, obj);
	case RBT_FLOAT:
		return rpgdata_read_float(fp, obj);
	case RBT_HASH:
	case RBT_HASH_DEF:
		return rpgdata_read_hash(fp, obj);
	case RBT_IVAR:
		return rpgdata_read_ivar(fp, obj);
	case RBT_LINK:
		return rpgdata_read_link(fp, obj);
	case RBT_OBJECT:
	case RBT_STRUCT:
		return rpgdata_read_object(fp, obj, FALSE);
	case RBT_REGEXP:
		return rpgdata_read_regexp(fp, obj);
	case RBT_STRING:
		return rpgdata_read_string(fp, obj, NULL);
	case RBT_SYMBOL:
		return rpgdata_read_symbol(fp, obj);
	case RBT_SYMLINK:
		return rpgdata_read_symlink(fp, obj);
	case RBT_UCLASS:
		return rpgdata_read_uclass(fp, obj);
	case RBT_USERDEF:
		return rpgdata_read_userdef(fp, obj);
	case RBT_USRMARSHAL:
		return rpgdata_read_usermarshal(fp, obj);
	default:
		return RPGERR_UNKNOWNTYPE;
	}
}

int rpgdata_read_array(FILE *fp, struct ruby_object *obj)
{
	int result;
	long i;
	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	if (!rpgdata_read_long(fp, &obj->length) || obj->length < 0)
		return RPGERR_DATA;
	if (obj->length > 0)
	{
		obj->adata = (struct ruby_object *)malloc(obj->length * sizeof(struct ruby_object));
		if (obj->adata == NULL)
			return RPGERR_MEMORY;
		memset(obj->adata, 0, obj->length * sizeof(struct ruby_object));
		for (i = 0; i < obj->length; i++)
		{
			result = rpgdata_read(fp, &obj->adata[i]);
			if (result != 0)
				return result;
		}
	}
	return 0;
}

int rpgdata_read_bignum(FILE *fp, struct ruby_object *obj)
{
	unsigned char sign = fgetc(fp); // 符号位
	if (feof(fp))
		return RPGERR_DATA;
	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	if (!rpgdata_read_long(fp, &obj->length))
		return RPGERR_DATA;
	obj->length = 2 * obj->length + 1;
	obj->data = (unsigned char *)malloc(obj->length);
	if (obj->data == NULL)
		return RPGERR_MEMORY;
	obj->data[0] = sign;
	fread(obj->data + 1, obj->length - 1, 1, fp);
	if (feof(fp))
		return RPGERR_DATA;
	return 0;
}

int rpgdata_read_class(FILE *fp, struct ruby_object *obj)
{
	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	return rpgdata_read_string(fp, NULL, &obj->name);
}

int rpgdata_read_extended(FILE *fp, struct ruby_object *obj)
{
	int result;
	struct ruby_object *sym;
	result = rpgdata_read_symbol_block(fp, &sym);
	if (result != 0)
		return result;
	result = rpgdata_read(fp, obj);
	obj->extension = sym;
	return result;
}

/*
注意：
	使用rpgdata_read_fixnum读取一个Fixnum对象
	使用rpgdata_read_long读取任意对象原始二进制数据中的长整数值（例如String对象中表示字符串长度的长整数）
*/
int rpgdata_read_fixnum(FILE *fp, struct ruby_object *obj)
{
	// 在Ruby Marshal中，Fixnum类型的数据无需注册
	if (!rpgdata_read_long(fp, &obj->ldata))
		return RPGERR_DATA;
	return 0;
}

int rpgdata_read_float(FILE *fp, struct ruby_object *obj)
{
	int err;
	err = rpgdata_read_string(fp, NULL, &obj->dbl_str);
	if (err != 0)
		return err;
	obj->ddata = 0.0;
	obj->err = RUBY_FLOAT;
	if (strcmp(obj->dbl_str, "nan") == 0)
		obj->err = RUBY_FLOAT_NAN;
	else if (strcmp(obj->dbl_str, "inf") == 0)
		obj->err = RUBY_FLOAT_INF;
	else if (strcmp(obj->dbl_str, "-inf") == 0)
		obj->err = RUBY_FLOAT_NEGINF;
	else
		sscanf_s(obj->dbl_str, "%lg", &obj->ddata);
	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	return 0;
}

int rpgdata_read_hash(FILE *fp, struct ruby_object *obj)
{
	int result;
	long i, n;
	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	if (!rpgdata_read_long(fp, &obj->length) || obj->length < 0)
		return RPGERR_DATA;
	n = 2 * obj->length;
	if (obj->type == RBT_HASH_DEF)
	{
		n++;
		i = 1;
	}
	else
	{
		i = 0;
		if (n == 0)
			return 0; // 若哈希表为空, 则无需分配内存
	}

	obj->adata = (struct ruby_object *)malloc(n * sizeof(struct ruby_object));
	if (obj->adata == NULL)
		return RPGERR_MEMORY;
	memset(obj->adata, 0, n * sizeof(struct ruby_object));
	for (; i < n; i++)
	{
		result = rpgdata_read(fp, &obj->adata[i]);
		if (result != 0)
			return result;
	}
	if (obj->type == RBT_HASH_DEF)
		return rpgdata_read(fp, &obj->adata[0]); // 默认值存到[0]处
	return 0;
}

/* 读取Marshal数据头 */
BOOL rpgdata_read_header(FILE *fp)
{
	long oldpos = ftell(fp);
	unsigned char data[2];
	fread(data, 2, 1, fp);
	if (!feof(fp) && data[0] == MARSHAL_MAJOR && data[1] == MARSHAL_MINOR)
		return TRUE; // 如果读取成功则返回true，且指针移动到数据头的后面
	else
	{
		fseek(fp, oldpos, SEEK_SET); // 如果读取失败，则退回到读取前的位置，并返回false
		return FALSE;
	}
}

int rpgdata_read_ivar(FILE *fp, struct ruby_object *obj)
{
	int result;
	struct ruby_member *ucls_member;
	unsigned char type = obj->type;
	void *p;
	
	result = rpgdata_read(fp, obj); // 读出来的总是一个RBT_UCLASS对象
	if (result != 0)
		return result;

	ucls_member = obj->members; // 暂存uclass里的成员
	obj->type = type;
	if (!rpgdata_read_long(fp, &obj->length) || obj->length < 0) // 成员个数
		return RPGERR_DATA; // 此时内存空间ucls_member能被rpgdata_free释放掉
	result = rpgdata_read_object(fp, obj, TRUE); // 读取对象里的各成员, 这个操作一定会改变obj->members的值, 使得ucls_member变为孤立内存
	if (result != 0)
	{
		rpgdata_free_member(ucls_member); // 释放存储在孤立内存中的数据
		free(ucls_member); // 释放孤立内存
		return result;
	}
	// 若执行成功, 则obj->members一定会改变

	// 将成员本身复制到成员列表开头
	p = realloc(obj->members, (obj->length + 1) * sizeof(struct ruby_member));
	if (p == NULL)
	{
		rpgdata_free_member(ucls_member);
		free(ucls_member);
		return RPGERR_MEMORY;
	}
	obj->members = (struct ruby_member *)p;
	if (obj->length > 0)
		memmove(obj->members + 1, obj->members, obj->length * sizeof(struct ruby_member));
	memcpy(obj->members, ucls_member, sizeof(struct ruby_member));
	free(ucls_member); // ucls_member未被注册, 因此可以直接释放, 但是存储在其中的数据不能释放, 因为obj->members[0]里引用了这些数据
	obj->length++;
	return 0;
}

int rpgdata_read_link(FILE *fp, struct ruby_object *obj)
{
	long id;
	if (!rpgdata_read_long(fp, &id))
		return RPGERR_DATA;
	obj->adata = rblist_get(&rb_objlist, id);
	if (obj->adata == NULL)
		return RPGERR_BROKENLINK;
	return 0;
}

/* 读取一个经过Ruby Marshal特殊处理的有符号长整数（不含类型标记） */
BOOL rpgdata_read_long(FILE *fp, long *val)
{
	char n = fgetc(fp);
	long i;
	if (feof(fp))
		return FALSE; // 文件意外结束, 读取失败

	if (n == 0)
		*val = 0;
	else if (n > 0)
	{
		if (n > 4 && n < 128)
			*val = n - 5;
		else
		{
			*val = 0;
			for (i = 0; i < n; i++)
			{
				*val |= (long)fgetc(fp) << (8 * i);
				if (feof(fp))
					return FALSE;
			}
		}
	}
	else
	{
		if (n > -129 && n < -4)
			*val = n + 5;
		else
		{
			n = -n;
			*val = -1;
			for (i = 0; i < n; i++)
			{
				*val &= ~(0xff << (8 * i));
				*val |= (long)fgetc(fp) << (8 * i);
				if (feof(fp))
					return FALSE;
			}
		}
	}
	return TRUE; // 读取成功
}

int rpgdata_read_object(FILE *fp, struct ruby_object *obj, BOOL only_members)
{
	int result;
	long i;
	struct ruby_object *memsym;

	obj->members = NULL; // 确保指针值一定发生改变, 使之前指向的内存区域成为孤立内存, 防止出错
	if (!only_members)
	{
		result = rpgdata_prepare_object(fp, obj, TRUE);
		if (result != 0)
			return result;
	}
	if (obj->length > 0)
	{
		obj->members = (struct ruby_member *)malloc(obj->length * sizeof(struct ruby_member));
		if (obj->members == NULL)
			return RPGERR_MEMORY;
		memset(obj->members, 0, obj->length * sizeof(struct ruby_member));
		for (i = 0; i < obj->length; i++)
		{
			result = rpgdata_read_symbol_block(fp, &memsym);
			if (result != 0)
				return result;
			obj->members[i].name = memsym->name;
			if (obj->members[i].name != NULL && obj->members[i].name[0] == '@')
				obj->members[i].name++; // 删除前导的@
			if (memsym->type == RBT_SYMBOL)
				obj->members[i].symbol_ref = memsym;
			else
			{
				obj->members[i].symbol_ref = NULL;
				free(memsym);
			}
			result = rpgdata_read(fp, &obj->members[i].value);
			if (result != 0)
				return result;
		}
	}
	return 0;
}

int rpgdata_read_regexp(FILE *fp, struct ruby_object *obj)
{
	char options;
	long i, len;
	if (!rpgdata_read_long(fp, &len) || len < 0)
		return RPGERR_DATA;

	if (!rpgdata_register_object(obj))
		return RPGERR_MEMORY;
	obj->sdata = (char *)malloc(len + 6);
	if (obj->sdata == NULL)
		return RPGERR_MEMORY;
	obj->sdata[0] = '/';
	fread(obj->sdata + 1, len, 1, fp);
	if (feof(fp))
		return RPGERR_DATA;
	obj->sdata[len + 1] = '/';
	
	options = fgetc(fp);
	if (feof(fp))
		return RPGERR_DATA;
	i = len + 2;
	if (options & ONIG_OPTION_IGNORECASE)
		obj->sdata[i++] = 'i';
	if (options & ONIG_OPTION_MULTILINE)
		obj->sdata[i++] = 'm';
	if (options & ONIG_OPTION_EXTEND)
		obj->sdata[i++] = 'x';
	obj->sdata[i] = '\0';
	obj->sdata = (char *)realloc(obj->sdata, i + 1);
	return 0;
}

/* 
如果参数obj为NULL，那么该对象将不会被注册，而是直接通过参数ppsz返回字符串指针，而且用完必须释放内存
参数ppsz可以为空
*/
int rpgdata_read_string(FILE *fp, struct ruby_object *obj, char **ppsz)
{
	char *str;
	long len;
	if (!rpgdata_read_long(fp, &len) || len < 0)
		return RPGERR_DATA; // 字符串长度读取失败或不合法

	str = (char *)malloc(len + 1);
	if (str == NULL)
		return RPGERR_MEMORY; // 内存分配失败
	str[len] = '\0';

	// 先将分配的内存地址绑定到obj或ppsz上, 避免发生内存泄漏
	if (ppsz != NULL)
		*ppsz = str;
	if (obj != NULL)
	{
		obj->length = len; // 字符串长度
		obj->sdata = str;
	}

	// 读取字符串内容
	// 若为空字符串, 则str中只有一个\0
	if (len > 0)
	{
		fread(str, len, 1, fp);
		if (feof(fp))
			return RPGERR_DATA; // 若读取的内容中有文件结束符, 则表明字符串不完整
	}

	// 注册字符串对象
	if (obj != NULL && !rpgdata_register_object(obj))
		return RPGERR_MEMORY; // 注册对象失败
	return 0;
}

int rpgdata_read_symbol(FILE *fp, struct ruby_object *obj)
{
	if (!rpgdata_register_symbol(obj)) // 注册该符号，以便于之后链接回来。因为在内存中，同名符号对象只允许存在一次
		return RPGERR_MEMORY;
	return rpgdata_read_string(fp, NULL, &obj->name);
}

/* 读取符号或符号链接 */
int rpgdata_read_symbol_block(FILE *fp, struct ruby_object **ppobj)
{
	int result;
	*ppobj = (struct ruby_object *)malloc(sizeof(struct ruby_object));
	if (*ppobj == NULL)
		return RPGERR_MEMORY;
	memset(*ppobj, 0, sizeof(struct ruby_object));

	(*ppobj)->type = fgetc(fp);
	if (feof(fp))
		return RPGERR_DATA;

	if ((*ppobj)->type == RBT_SYMBOL)
		result = rpgdata_read_symbol(fp, *ppobj); // 读取后会自动注册该符号，因此sym应该永久保留，直到解析结束并释放整个ruby对象
	else if ((*ppobj)->type == RBT_SYMLINK)
		result = rpgdata_read_symlink(fp, *ppobj); // 使用完毕后必须释放sym
	else
		result = RPGERR_DATA; // 数据类型错误

	if (result != 0)
	{
		// 如果读取失败, 则需要释放分配的内存
		// 因为函数返回时可能会忘记将*ppobj指向的内存保存到symbol_ref中, 造成内存泄漏
		rpgdata_free(*ppobj);
		free(*ppobj);
		*ppobj = NULL; // 返回空指针
	}
	return result;
}

int rpgdata_read_symlink(FILE *fp, struct ruby_object *obj)
{
	long id;
	if (!rpgdata_read_long(fp, &id))
		return RPGERR_DATA;
	obj->adata = rblist_get(&rb_symlist, id);
	if (obj->adata == NULL)
		return RPGERR_BROKENSYMLINK;
	obj->name = obj->adata->name;
	return 0;
}

int rpgdata_read_uclass(FILE *fp, struct ruby_object *obj)
{
	int result;
	struct ruby_member *member;
	struct ruby_object *sym;
	unsigned char type = obj->type;

	result = rpgdata_read_symbol_block(fp, &sym); // 读取类名, 并暂存
	if (result != 0)
	{
		obj->symbol_ref = sym; // 退出时自动释放sym对象
		return result;
	}
	result = rpgdata_read(fp, obj); // 读取对象本身, 并注册这个对象
	if (result != 0)
	{
		// 此时obj->symbol_ref可能已被占用, 因此不能用来绑定sym对象, 需要立即释放
		rpgdata_free_symbol(sym);
		return result;
	}
	
	// 将之前读到的内容放到member->value中, 同时清空obj
	// 这样被注册的对象就是obj, 而非member->value, 但读到的内容却是放到member->value里的
	member = (struct ruby_member *)malloc(sizeof(struct ruby_member)); // 新开辟的内存中, 三个成员均为垃圾数据
	if (member == NULL)
	{
		rpgdata_free_symbol(sym);
		return RPGERR_MEMORY;
	}
	memcpy(&member->value, obj, sizeof(struct ruby_object));
	memset(obj, 0, sizeof(struct ruby_object));
	// 只有之前所读取的对象obj原封不动的复制到了member->value中, 才能清空obj, 并修改obj的内容

	member->name = "."; // 对象本身用单个点表示
	member->symbol_ref = NULL; // 必须清除垃圾数据
	obj->name = sym->name; // UCLASS对象名
	obj->type = type;
	obj->length = 1; // 目前UCLASS对象暂只有一个"."成员
	obj->members = member;
	if (sym->type == RBT_SYMBOL)
		obj->symbol_ref = sym; // 符号对象无需释放
	else
		free(sym); // 符号链接必须释放
	return 0;
}

// RBT_USERDEF类型表示一堆已知长度的二进制数据
int rpgdata_read_userdef(FILE *fp, struct ruby_object *obj)
{
	int result = rpgdata_prepare_object(fp, obj, TRUE);
	if (result != 0)
		return result;
	obj->data = (unsigned char *)malloc(obj->length);
	if (obj->data == NULL)
		return RPGERR_MEMORY;
	fread(obj->data, obj->length, 1, fp);
	if (feof(fp))
		return RPGERR_DATA;
	return 0;
}

// RBT_USRMARSHAL类型相当于一个长度恒为1的数组
int rpgdata_read_usermarshal(FILE *fp, struct ruby_object *obj)
{
	int result = rpgdata_prepare_object(fp, obj, FALSE);
	if (result != 0)
		return result;
	obj->length = 1;
	obj->adata = (struct ruby_object *)malloc(sizeof(struct ruby_object));
	if (obj->adata == NULL)
		return RPGERR_MEMORY;
	return rpgdata_read(fp, obj->adata); // 可以为任意Ruby对象
}

/* 清空上一个字节流的所有链接列表 */
void rpgdata_reset(void)
{
	rblist_reset(&rb_objlist);
	rblist_reset(&rb_symlist);
}
