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

//清理函数(回调函数)的类型
typedef void(*ngx_pool_cleanup_pt)(void *data);

typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;
struct ngx_pool_cleanup_s 
{
	ngx_pool_cleanup_pt _handler; //函数指针，保存清理操作的回调函数
	void*               _data;    //传递给回调函数的参数
	ngx_pool_cleanup_t* _next;    //所有的cleanup清理操作都被串在一条链表上
};

//分配小块内存的内存池的头部数据信息
typedef struct ngx_pool_data_s  ngx_pool_data_t;
struct ngx_pool_data_s
{
	/*u_char*  _work;
	int32_t  _ptimes_thread;*/

	u_char*  _begin;           //小块内存池的起始地址
	u_char*  _last;            //小块内存池可用内存的起始地址
	u_char*  _end;             //小块内存池可用内存的末尾地址后一位
	u_char*  _reset;		   //小块内存池的回收标记位
	int16_t  _ptimes;          //小块内存已分配数量
	ngx_pool_data_t*  _next;   //所有capacity内的内存块都被串在一起
	ngx_pool_data_t*  _next_chunck; //所有capacity内存块都被串在一起

};

//线程内存分配管理类型
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

//小块内存分配考虑字节对齐时的单位(platform word)
const size_t NGX_ALIGNMENT = sizeof(unsigned long);
const size_t PAGE_SIZE = 4096;
//ngx小块内存池可分配的最大空间
const size_t NGX_MAX_ALLOC_FROM_POOL = PAGE_SIZE - 1;
const size_t NGX_DEFAULT_POOL_SIZE = 64 * 1024; 
//内存池大小按照16字节进行对齐
const size_t NGX_POOL_ALIGNMENT = 16;
//ngx小块内存池最小的size调整成NGX_POOL_ALIGNMENT的临近的倍数
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
	//考虑内存字节对齐，从内存池申请size大小的内存
	void* ngx_palloc(/*size_t capacity,*/ size_t size);
	//和上面的函数一样，不考虑内存字节对齐
	void* ngx_pnalloc(/*size_t capacity,*/ size_t size);
	//调用的是ngx_palloc实现内存分配，但是会初始化0
	void* ngx_pcalloc(/*size_t capacity,*/ size_t size);
	//内存池的销毁函数
	void ngx_destroy_pool();
	//添加回调清理操作函数
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
	static pthread_mutex_t  __pool_mutex; //小块内存分配释放锁
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
