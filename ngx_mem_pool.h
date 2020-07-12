#ifndef __NGX_MEN_POOL_H__
#define __NGX_MEN_POOL_H__


#include <stdlib.h>
#include <memory.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <map>
#include <cmath>

enum T_CAPACITY
{
	_CAP1  =  1,
	_CAP2  =  2,
	_CAP3  =  3,
	_CAP4  =  4,
	_CAP5  =  5
};

// Select one of T_SPANSIZE value as each span size
// _SPN64K    <====>   span size: 64 * 1024(64KB)
// _SPN128K   <====>   span size: 128 * 1024(128KB)
// _SPN256K   <====>   span size: 256 * 1024(256KB)
// _SPN512K   <====>   span size: 512 * 1024(512KB)
// _SPN1M     <====>   span size: 1024 * 1024(1MB)
enum T_SPANSIZE
{
	_SPN16K  =  16 * 1024,
	_SPN32K  =  32 * 1024,
	_SPN64K  =  64 * 1024,
	_SPN128K =  128 * 1024,
	_SPN256K =  256 * 1024,
	_SPN512K =  512 * 1024,
	_SPN1M   =  1024 * 1024
};

// Select one of T_POOLSIZE value as the pool size,
// the default value is medium
// _SMALL    <====>   pool size: 120
// _MEDIUM   <====>   pool size: 240
// _LARGE    <====>   pool size: 360
enum T_POOLSIZE
{
	_SMALL  =  200,
	_MEDIUM =  400,
	_LARGE  =  600
};

extern const int handleThreads;
static const int _TOTAL_THREADS_ = handleThreads + 2;

//static std::map<T_SPANSIZE, uint16_t> 
//_MOVE_BITS_ = {
//	{_SPN16K, 14}, 
//	{_SPN32K, 15}, 
//	{_SPN64K, 16}, 
//	{_SPN128K, 17}, 
//	{_SPN256K, 18}, 
//	{_SPN512K, 19}, 
//	{_SPN1M, 20} };

using u_char = unsigned char;

//������(�ص�����)������
typedef void(*ngx_pool_cleanup_pt)(void *data);

typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;
struct ngx_pool_cleanup_s 
{
	ngx_pool_cleanup_pt _handler; //����ָ�룬������������Ļص�����
	void*               _data;    //���ݸ��ص������Ĳ���
	ngx_pool_cleanup_t* _next;    //���е�cleanup���������������һ��������
};

//����С���ڴ���ڴ�ص�ͷ��������Ϣ
typedef struct ngx_pool_data_s  ngx_pool_data_t;
struct ngx_pool_data_s
{
	/*u_char*  _work;
	int32_t  _ptimes_thread;*/

	u_char*  _begin;           //С���ڴ�ص���ʼ��ַ
	u_char*  _last;            //С���ڴ�ؿ����ڴ����ʼ��ַ
	u_char*  _end;             //С���ڴ�ؿ����ڴ��ĩβ��ַ��һλ
	u_char*  _reset;		   //С���ڴ�صĻ��ձ��λ
	int16_t  _ptimes;          //С���ڴ��ѷ�������
	ngx_pool_data_t*  _next;   //����capacity�ڵ��ڴ�鶼������һ��
	ngx_pool_data_t*  _next_chunck; //����capacity�ڴ�鶼������һ��

};

//�߳��ڴ�����������
typedef struct thread_memmanage_s  thread_memmanage_t;
struct thread_memmanage_s
{
	pthread_t  _tid;
	//size_t  _poolcapacity;
	ngx_pool_data_t* _poolcurrent;
	ngx_pool_data_t* _pooldeposit;
	ngx_pool_data_t* _pooldeposit_tail;
	int _pooldeposit_size;

	thread_memmanage_t* _next;
	thread_memmanage_t* _prev;
};

//extern __thread  thread_memmanage_t*  __threadlocal_data  
//	   __attribute__((tls_model("initial-exec")));
//
//extern pthread_key_t __threadlocal_key;

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, mi, ma) MAX(MIN(v, ma), mi)
#define ngx_align(d, a)    (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
    (/*char**/u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

//С���ڴ���俼���ֽڶ���ʱ�ĵ�λ(platform word)
const size_t NGX_ALIGNMENT = sizeof(unsigned long);
const size_t PAGE_SIZE = 4096;
//ngxС���ڴ�ؿɷ�������ռ�
const size_t NGX_MAX_ALLOC_FROM_POOL = PAGE_SIZE - 1;
const size_t NGX_DEFAULT_POOL_SIZE = 64 * 1024; 
//�ڴ�ش�С����16�ֽڽ��ж���
const size_t NGX_POOL_ALIGNMENT = 16;
//ngxС���ڴ����С��size������NGX_POOL_ALIGNMENT���ٽ��ı���
const size_t NGX_MIN_POOL_SIZE = ngx_align(64 * 1024, NGX_POOL_ALIGNMENT);

const size_t NGX_POINTER_SIZE = sizeof(ngx_pool_data_t*);
const size_t NGX_POOL_DATA_T_SIZE = sizeof(ngx_pool_data_t);


class ngx_mem_pool
{
public:
	ngx_mem_pool()
	{
		__cleanup = nullptr;
		__Head = nullptr;
		__Tail = nullptr;
		//__init = false;
	}

	virtual ~ngx_mem_pool()
	{
		ngx_destroy_pool();
		/*ngx_log_stderr(0, "~ngx_destroy_pool() executed, "
			"global class ngx_mem_pool object was detroyed!");*/
		return;
	}

public:
	size_t ngx_get_Freepool_size();
	//ngx_pool_data_t* ngx_get_cur_pool();

    bool ngx_create_pool();
	//�����ڴ��ֽڶ��룬���ڴ������size��С���ڴ�
	void* ngx_palloc(/*size_t capacity,*/ size_t size);
	//������ĺ���һ�����������ڴ��ֽڶ���
	void* ngx_pnalloc(/*size_t capacity,*/ size_t size);
	//���õ���ngx_pallocʵ���ڴ���䣬���ǻ��ʼ��0
	void* ngx_pcalloc(/*size_t capacity,*/ size_t size);
	//�ڴ�ص����ٺ���
	void ngx_destroy_pool();
	//��ӻص������������
	ngx_pool_cleanup_t* 
		ngx_pool_cleanup_add(ngx_pool_cleanup_pt handler, void* data);

	void FREE(void* p);

private:
	enum { MAX_STOCK = 200 };
	enum { SPAN_SIZE = 1024 * 1024 }; // 2 ^ 16 = 64 * 1024 B
	enum { POOL_SIZE = 1024 * 1024 };
	enum { MAX_ALLOC_SIZE = NGX_MAX_ALLOC_FROM_POOL };

	//enum { MOVEBITS = log(_SPN512K)/log(2) };
	//enum { CAPACITY = 5 };

private:
	/*static*/ u_char*  __Head;
	/*static*/ u_char*  __Tail;
	ngx_pool_cleanup_t*  __cleanup;
	static pthread_mutex_t  __pool_mutex; //С���ڴ�����ͷ���
	static pthread_spinlock_t  __pool_spin;

	static ngx_pool_data_t   __Pool_list[_LARGE];
	static ngx_pool_data_t*  __Free_pool;
	static size_t  __Free_pool_size;

	static bool __init;
public:
	static int  __capacity;
	
	static int  __movebits;

//public:
	//static std::map<pthread_t, thread_memmanage_t*> __thread_cache;

private:
	void* ngx_palloc_small(size_t size, unsigned int align);
	void* ngx_palloc_large(size_t size);

	static void ngx_delete_cache(void* ptr);
	static thread_memmanage_t* ngx_palloc_cache();
	static void ngx_set_cache();
	static void ngx_set_capacity();

	static __thread  thread_memmanage_t* __threadlocal_data
		__attribute__((tls_model("initial-exec")));

	static pthread_key_t __threadlocal_key;

	static u_char __lk;

public:
	static thread_memmanage_t* __thread_heaps;
	static int __thread_heap_count;

};

#endif
