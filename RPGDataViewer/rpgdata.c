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
	memset(list, 0, sizeof(struct ruby_list)); // ��������ṹ��
}

/* ����������ת��Ϊʮ�������ַ��� */
LPTSTR rpgdata_bignum_to_string(struct ruby_object *obj)
{
	BOOL zero_flag = TRUE; // ȥ������ǰ��0�ı��
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

/* �ͷŴ洢��ruby_object�ṹ�е�������ռ�õ��ڴ� */
/* ���Ҫ�ͷŽṹ�屾��, ��ֱ�ӵ���free���� */
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

/* �ͷŷ��Ŷ����������Ӷ��� */
// ��objһ���Ƿ��Ŷ���ʱ, ���ߵ�obj�����Ƿ��Ŷ�����������ʱ������ô˺������ͷ��ڴ�
// ����Ѿ�ȷ��objһ���Ƿ�������, ��ôֻ��ֱ�ӵ���free����
void rpgdata_free_symbol(struct ruby_object *obj)
{
	if (obj == NULL)
		return;
	rpgdata_free(obj); // �ͷŴ洢�ڷ��Ŷ����е�����
	free(obj); // �ͷŶ�������ռ�õ��ڴ�
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
��ȡ�ļ��н������ķ��Ŷ�������������ָ��ķ��Ŷ���
�Ը÷��ŵ�������Ϊ��������������ע����󣬷����������Ķ���
*/
int rpgdata_prepare_object(FILE *fp, struct ruby_object *obj, BOOL has_length)
{
	// ��Ruby�У������ͳ�Ա�����Ƿ��ţ���ͬ������ֻ�����ڴ��д���һ��
	int result;
	struct ruby_object *sym;
	result = rpgdata_read_symbol_block(fp, &sym); // ����
	if (result != 0)
		return result;

	obj->name = sym->name; // ֻҪ���Ŷ������, ����ַ����Ͳ���ʧЧ
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
	unsigned char sign = fgetc(fp); // ����λ
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
ע�⣺
	ʹ��rpgdata_read_fixnum��ȡһ��Fixnum����
	ʹ��rpgdata_read_long��ȡ�������ԭʼ�����������еĳ�����ֵ������String�����б�ʾ�ַ������ȵĳ�������
*/
int rpgdata_read_fixnum(FILE *fp, struct ruby_object *obj)
{
	// ��Ruby Marshal�У�Fixnum���͵���������ע��
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
			return 0; // ����ϣ��Ϊ��, ����������ڴ�
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
		return rpgdata_read(fp, &obj->adata[0]); // Ĭ��ֵ�浽[0]��
	return 0;
}

/* ��ȡMarshal����ͷ */
BOOL rpgdata_read_header(FILE *fp)
{
	long oldpos = ftell(fp);
	unsigned char data[2];
	fread(data, 2, 1, fp);
	if (!feof(fp) && data[0] == MARSHAL_MAJOR && data[1] == MARSHAL_MINOR)
		return TRUE; // �����ȡ�ɹ��򷵻�true����ָ���ƶ�������ͷ�ĺ���
	else
	{
		fseek(fp, oldpos, SEEK_SET); // �����ȡʧ�ܣ����˻ص���ȡǰ��λ�ã�������false
		return FALSE;
	}
}

int rpgdata_read_ivar(FILE *fp, struct ruby_object *obj)
{
	int result;
	struct ruby_member *ucls_member;
	unsigned char type = obj->type;
	void *p;
	
	result = rpgdata_read(fp, obj); // ������������һ��RBT_UCLASS����
	if (result != 0)
		return result;

	ucls_member = obj->members; // �ݴ�uclass��ĳ�Ա
	obj->type = type;
	if (!rpgdata_read_long(fp, &obj->length) || obj->length < 0) // ��Ա����
		return RPGERR_DATA; // ��ʱ�ڴ�ռ�ucls_member�ܱ�rpgdata_free�ͷŵ�
	result = rpgdata_read_object(fp, obj, TRUE); // ��ȡ������ĸ���Ա, �������һ����ı�obj->members��ֵ, ʹ��ucls_member��Ϊ�����ڴ�
	if (result != 0)
	{
		rpgdata_free_member(ucls_member); // �ͷŴ洢�ڹ����ڴ��е�����
		free(ucls_member); // �ͷŹ����ڴ�
		return result;
	}
	// ��ִ�гɹ�, ��obj->membersһ����ı�

	// ����Ա�����Ƶ���Ա�б�ͷ
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
	free(ucls_member); // ucls_memberδ��ע��, ��˿���ֱ���ͷ�, ���Ǵ洢�����е����ݲ����ͷ�, ��Ϊobj->members[0]����������Щ����
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

/* ��ȡһ������Ruby Marshal���⴦����з��ų��������������ͱ�ǣ� */
BOOL rpgdata_read_long(FILE *fp, long *val)
{
	char n = fgetc(fp);
	long i;
	if (feof(fp))
		return FALSE; // �ļ��������, ��ȡʧ��

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
	return TRUE; // ��ȡ�ɹ�
}

int rpgdata_read_object(FILE *fp, struct ruby_object *obj, BOOL only_members)
{
	int result;
	long i;
	struct ruby_object *memsym;

	obj->members = NULL; // ȷ��ָ��ֵһ�������ı�, ʹ֮ǰָ����ڴ������Ϊ�����ڴ�, ��ֹ����
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
				obj->members[i].name++; // ɾ��ǰ����@
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
�������objΪNULL����ô�ö��󽫲��ᱻע�ᣬ����ֱ��ͨ������ppsz�����ַ���ָ�룬������������ͷ��ڴ�
����ppsz����Ϊ��
*/
int rpgdata_read_string(FILE *fp, struct ruby_object *obj, char **ppsz)
{
	char *str;
	long len;
	if (!rpgdata_read_long(fp, &len) || len < 0)
		return RPGERR_DATA; // �ַ������ȶ�ȡʧ�ܻ򲻺Ϸ�

	str = (char *)malloc(len + 1);
	if (str == NULL)
		return RPGERR_MEMORY; // �ڴ����ʧ��
	str[len] = '\0';

	// �Ƚ�������ڴ��ַ�󶨵�obj��ppsz��, ���ⷢ���ڴ�й©
	if (ppsz != NULL)
		*ppsz = str;
	if (obj != NULL)
	{
		obj->length = len; // �ַ�������
		obj->sdata = str;
	}

	// ��ȡ�ַ�������
	// ��Ϊ���ַ���, ��str��ֻ��һ��\0
	if (len > 0)
	{
		fread(str, len, 1, fp);
		if (feof(fp))
			return RPGERR_DATA; // ����ȡ�����������ļ�������, ������ַ���������
	}

	// ע���ַ�������
	if (obj != NULL && !rpgdata_register_object(obj))
		return RPGERR_MEMORY; // ע�����ʧ��
	return 0;
}

int rpgdata_read_symbol(FILE *fp, struct ruby_object *obj)
{
	if (!rpgdata_register_symbol(obj)) // ע��÷��ţ��Ա���֮�����ӻ�������Ϊ���ڴ��У�ͬ�����Ŷ���ֻ�������һ��
		return RPGERR_MEMORY;
	return rpgdata_read_string(fp, NULL, &obj->name);
}

/* ��ȡ���Ż�������� */
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
		result = rpgdata_read_symbol(fp, *ppobj); // ��ȡ����Զ�ע��÷��ţ����symӦ�����ñ�����ֱ�������������ͷ�����ruby����
	else if ((*ppobj)->type == RBT_SYMLINK)
		result = rpgdata_read_symlink(fp, *ppobj); // ʹ����Ϻ�����ͷ�sym
	else
		result = RPGERR_DATA; // �������ʹ���

	if (result != 0)
	{
		// �����ȡʧ��, ����Ҫ�ͷŷ�����ڴ�
		// ��Ϊ��������ʱ���ܻ����ǽ�*ppobjָ����ڴ汣�浽symbol_ref��, ����ڴ�й©
		rpgdata_free(*ppobj);
		free(*ppobj);
		*ppobj = NULL; // ���ؿ�ָ��
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

	result = rpgdata_read_symbol_block(fp, &sym); // ��ȡ����, ���ݴ�
	if (result != 0)
	{
		obj->symbol_ref = sym; // �˳�ʱ�Զ��ͷ�sym����
		return result;
	}
	result = rpgdata_read(fp, obj); // ��ȡ������, ��ע���������
	if (result != 0)
	{
		// ��ʱobj->symbol_ref�����ѱ�ռ��, ��˲���������sym����, ��Ҫ�����ͷ�
		rpgdata_free_symbol(sym);
		return result;
	}
	
	// ��֮ǰ���������ݷŵ�member->value��, ͬʱ���obj
	// ������ע��Ķ������obj, ����member->value, ������������ȴ�Ƿŵ�member->value���
	member = (struct ruby_member *)malloc(sizeof(struct ruby_member)); // �¿��ٵ��ڴ���, ������Ա��Ϊ��������
	if (member == NULL)
	{
		rpgdata_free_symbol(sym);
		return RPGERR_MEMORY;
	}
	memcpy(&member->value, obj, sizeof(struct ruby_object));
	memset(obj, 0, sizeof(struct ruby_object));
	// ֻ��֮ǰ����ȡ�Ķ���objԭ�ⲻ���ĸ��Ƶ���member->value��, �������obj, ���޸�obj������

	member->name = "."; // �������õ������ʾ
	member->symbol_ref = NULL; // ���������������
	obj->name = sym->name; // UCLASS������
	obj->type = type;
	obj->length = 1; // ĿǰUCLASS������ֻ��һ��"."��Ա
	obj->members = member;
	if (sym->type == RBT_SYMBOL)
		obj->symbol_ref = sym; // ���Ŷ��������ͷ�
	else
		free(sym); // �������ӱ����ͷ�
	return 0;
}

// RBT_USERDEF���ͱ�ʾһ����֪���ȵĶ���������
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

// RBT_USRMARSHAL�����൱��һ�����Ⱥ�Ϊ1������
int rpgdata_read_usermarshal(FILE *fp, struct ruby_object *obj)
{
	int result = rpgdata_prepare_object(fp, obj, FALSE);
	if (result != 0)
		return result;
	obj->length = 1;
	obj->adata = (struct ruby_object *)malloc(sizeof(struct ruby_object));
	if (obj->adata == NULL)
		return RPGERR_MEMORY;
	return rpgdata_read(fp, obj->adata); // ����Ϊ����Ruby����
}

/* �����һ���ֽ��������������б� */
void rpgdata_reset(void)
{
	rblist_reset(&rb_objlist);
	rblist_reset(&rb_symlist);
}
