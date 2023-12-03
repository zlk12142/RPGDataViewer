#define RPGERR_NOTOPEN 1
#define RPGERR_FORMAT 2
#define RPGERR_MEMORY 3
#define RPGERR_DATA 4
#define RPGERR_BROKENLINK 5
#define RPGERR_BROKENSYMLINK 6
#define RPGERR_UNKNOWNTYPE 7

#define MARSHAL_MAJOR 4
#define MARSHAL_MINOR 8

/* Ruby Types (RBT) */
#define RBT_NIL '0'
#define RBT_TRUE 'T'
#define RBT_FALSE 'F'
#define RBT_FIXNUM 'i'
#define RBT_EXTENDED 'e'
#define RBT_UCLASS 'C'
#define RBT_OBJECT 'o'
#define RBT_DATA 'd'
#define RBT_USERDEF 'u'
#define RBT_USRMARSHAL 'U'
#define RBT_FLOAT 'f'
#define RBT_BIGNUM 'l'
#define RBT_STRING '"'
#define RBT_REGEXP '/'
#define RBT_ARRAY '['
#define RBT_HASH '{'
#define RBT_HASH_DEF '}'
#define RBT_STRUCT 'S'
#define RBT_MODULE_OLD 'M'
#define RBT_CLASS 'c'
#define RBT_MODULE 'm'
#define RBT_SYMBOL ':'
#define RBT_SYMLINK ';'
#define RBT_IVAR 'I'
#define RBT_LINK '@'

#define ONIG_OPTION_IGNORECASE 0x01
#define ONIG_OPTION_EXTEND 0x02
#define ONIG_OPTION_MULTILINE 0x04

#define RBLIST_STEP 2

enum ruby_float_err
{
	RUBY_FLOAT = 0,
	RUBY_FLOAT_INF = '+',
	RUBY_FLOAT_NEGINF = '-',
	RUBY_FLOAT_NAN = '?'
};

struct ruby_list
{
	long length;
	long capacity;
	struct ruby_object **items;
};

struct ruby_object
{
	unsigned char type;
	char *name;
	long length;
	struct ruby_object *extension; // ��ruby����̳е�rubyģ��

	/* ʵ������ */
	union
	{
		// �ò�ͬ���͵ı�������ʾ�����򣬱��ⷱ����ǿ������ת��
		// ���¸���Ա�������ڴ������ص���
		// 32λ����sizeof(void *)=4, 64λ����=8
		
		unsigned char *data; // ������������ (4��8�ֽ�)
		long ldata; // ������ (4�ֽ�)
		char *sdata; // �ַ����� (4��8�ֽ�)

		// ������ (16��20�ֽ�)
		struct
		{
			double ddata;
			enum ruby_float_err err; // ����������ֵ (����inf)
			char *dbl_str; // ԭʼ�������ַ���
		};
		
		struct ruby_object *adata; // ����Ԫ���б� (4��8�ֽ�)

		// �����Ա�б� (8��16�ֽ�)
		struct
		{
			struct ruby_member *members;
			struct ruby_object *symbol_ref; // ��ö�������ķ��Ŷ���
		};
	};
};

/* Ruby����ĸ�����Ա */
struct ruby_member
{
	char *name; // ����
	struct ruby_object value; // ֵ
	struct ruby_object *symbol_ref; // ��ó�Ա�����ķ��Ŷ���
};

/* ע��Ƿ��Ŷ��� */
#define rpgdata_register_object(obj) rblist_add(&rb_objlist, obj)
/* ע����Ŷ��� */
#define rpgdata_register_symbol(obj) rblist_add(&rb_symlist, obj)

BOOL rblist_add(struct ruby_list *list, struct ruby_object *item);
struct ruby_object *rblist_get(struct ruby_list *list, long id);
void rblist_reset(struct ruby_list *list);

LPTSTR rpgdata_bignum_to_string(struct ruby_object *obj);
void rpgdata_free(struct ruby_object *obj);
void rpgdata_free_member(struct ruby_member *member);
void rpgdata_free_symbol(struct ruby_object *obj);
BOOL rpgdata_isfolder(unsigned char type);
int rpgdata_load(LPCTSTR filename, struct ruby_object *obj);
int rpgdata_prepare_object(FILE *fp, struct ruby_object *obj, BOOL has_length);
int rpgdata_read(FILE *fp, struct ruby_object *obj);
int rpgdata_read_array(FILE *fp, struct ruby_object *obj);
int rpgdata_read_bignum(FILE *fp, struct ruby_object *obj);
int rpgdata_read_class(FILE *fp, struct ruby_object *obj);
int rpgdata_read_extended(FILE *fp, struct ruby_object *obj);
int rpgdata_read_fixnum(FILE *fp, struct ruby_object *obj);
int rpgdata_read_float(FILE *fp, struct ruby_object *obj);
int rpgdata_read_hash(FILE *fp, struct ruby_object *obj);
BOOL rpgdata_read_header(FILE *fp);
int rpgdata_read_ivar(FILE *fp, struct ruby_object *obj);
int rpgdata_read_link(FILE *fp, struct ruby_object *obj);
BOOL rpgdata_read_long(FILE *fp, long *val);
int rpgdata_read_object(FILE *fp, struct ruby_object *obj, BOOL only_members);
int rpgdata_read_regexp(FILE *fp, struct ruby_object *obj);
int rpgdata_read_string(FILE *fp, struct ruby_object *obj, char **ppsz);
int rpgdata_read_symbol(FILE *fp, struct ruby_object *obj);
int rpgdata_read_symbol_block(FILE *fp, struct ruby_object **ppobj);
int rpgdata_read_symlink(FILE *fp, struct ruby_object *obj);
int rpgdata_read_uclass(FILE *fp, struct ruby_object *obj);
int rpgdata_read_userdef(FILE *fp, struct ruby_object *obj);
int rpgdata_read_usermarshal(FILE *fp, struct ruby_object *obj);
void rpgdata_reset();