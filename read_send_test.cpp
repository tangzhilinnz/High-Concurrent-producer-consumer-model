#include <stdio.h>
#include <cstring>
#include <time.h>
#include <vector>
#include <pthread.h>
#include <unistd.h>
//#include <list>
#include <semaphore.h>
#include <string>
//#include <map>

#include "ngx_lockmutex.h"
#include "ngx_lockspin.h"
#include "tzmalloc.h"
#include "ngx_c_crc32.h"



//=======================================================================================

#define VER            1  /* modify VER to choose the version here */

#define TC_MALLOC      2  /* test2 */
#define MY_MALLOC      1  /* test1 */ 

const int repeatTimes = 100;
const int runTimes = 100000;
const int handleThreads = 3; //3
const int sendThreads = 1; //1
//=======================================================================================
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  read_cond = PTHREAD_COND_INITIALIZER;

struct header_s
{
	int index;
};

TzMemPool<_SPN512K, _EX>* mempool;

struct MsgTag
{
	MsgTag* next;
};

MsgTag* readQueueHead = nullptr;
MsgTag* readQueueTail = nullptr;
int readQueueSize = 0;
MsgTag* sendQueueHead = nullptr;
MsgTag* sendQueueTail = nullptr;
int sendQueueSize = 0;
static int readLOCK = 0;
static int sendLOCK = 0;
volatile int quickFetchMode = 0;
volatile int ISSTOP = 0;
volatile int EXIT = 0;
volatile sem_t speedUp[5];
unsigned char testString[] = { 'a', 'b' , '#', '$', '@', '2', '4', 'r', 'o', 'p', '?', '0' };

CCRC32* p_crc32;

//static char* out_read_queue()
//{
//	char* ptr;
//	//CLockGuard lock(&read_mutex);
//	//CLockGuardSpin lock(&read_spin);
//
//	while (__sync_lock_test_and_set(&readLOCK, 1))
//	{
//		usleep(0);
//		//printf("yes1-------%d\n", pthread_self());
//	}
//
//	if (readQueueHead == nullptr)
//	{
//		__sync_lock_release(&readLOCK);
//		return nullptr;
//	}
//
//	ptr = reinterpret_cast<char*>(readQueueHead) + sizeof(MsgTag);
//
//	readQueueHead = readQueueHead->next;
//
//	if (readQueueHead == nullptr)
//		readQueueTail = nullptr;
//
//	//--readQueueSize;
//
//	__sync_lock_release(&readLOCK);
//
//	return ptr;
//}

//static char* out_send_queue()
//{
//	char* ptr;
//	//CLockGuard lock(&read_mutex);
//	CLockGuardSpin lock(&send_spin);
//	if (sendQueueHead == nullptr)
//	{
//		return nullptr;
//	}
//
//	ptr = reinterpret_cast<char*>(sendQueueHead) + sizeof(MsgTag);
//
//	sendQueueHead = sendQueueHead->next;
//
//	if (sendQueueHead == nullptr)
//		sendQueueTail = nullptr;
//
//	--sendQueueSize;
//
//	return ptr;
//}


static void* thread_read(void* arg)
{
	printf("thread_read tid %d\n", pthread_self());
	int random;
	char* p;
	header_s* ph;
	int j = 0;

	int threadSelect = 0;

	struct timespec tmv1, tmv2;
	double tm = 0;
	double total_tm = 0;
	int i3 = 0;

	while (j++ < repeatTimes)
	{
		timespec_get(&tmv1, 1);

		ISSTOP = 0;
		srand(time(NULL));

		int crc32;
		for (int i = 0; i < runTimes; i++)
		{
			//int c = i % 12;
			//char temp = testString[c];
			//testString[c] = testString[0];
			//testString[0] = temp;
			//crc32 = p_crc32->Get_CRC(testString, 12);
			//if (crc32 == 101010) { crc32 += 11; }
			//else { crc32 += 101;}


			random = 32 + rand() % 4050;
			//random = 32 + rand() % 200;
			//random = 4050 + rand() % 32;
			//random = 32;
			random += 8;
	
#if (VER == MY_MALLOC)
			//p = (char*)mempool->TzMalloc(random);
			p = (char*)mempool->TzMalloc(random) + 8;
#endif
#if	(VER == TC_MALLOC)
			p = (char*)malloc(random) + 8;
#endif

			ph = (header_s*)p;
			ph->index = i;


			MsgTag* msg = reinterpret_cast<MsgTag*>(p - sizeof(MsgTag));
			msg->next = nullptr;

			while (readQueueSize > 400000)
			{
				usleep(16000);
			}

			while (__sync_lock_test_and_set(&readLOCK, 1))
			{
				usleep(0);
				//printf("yes1-------%d\n", pthread_self());
			}
			{
				++readQueueSize;
				if (readQueueTail == nullptr)
				{
					readQueueHead = msg;
					readQueueTail = msg;
				}
				else
				{
					readQueueTail->next = msg;
					readQueueTail = msg;
				}
			}

			//唤醒一个等待该条件的线程
			pthread_cond_signal(&read_cond); 
			//pthread_cond_broadcast(&read_cond);

			__sync_lock_release(&readLOCK);
			
		}
		
		printf("read queue size: %d\n", readQueueSize);
		//printf("ngx_get_Free_pool_size: %d\n", mempool->TzGetFreepoolSize());
		while (!ISSTOP) { /*printf("Yes\n");*/ sleep(0); }
		timespec_get(&tmv2, 1);
		tm += (tmv2.tv_sec - tmv1.tv_sec) * 1000;
		tm += (tmv2.tv_nsec * 0.000001 - tmv1.tv_nsec * 0.000001);
		total_tm += tm;
		printf("[%d] time consuming: %f ms\n", j, tm);
		if (tm <= 65) i3++;
		tm = 0;
		//printf("read queue size: %d\n", readQueueSize);
		//printf("send queue size: %d\n", sendQueueSize);
	}
	printf("read thread ===== %d finished!\n", pthread_self());
	printf("The number of timer less than 65ms: %d and total time consuming: %f ms\n", i3, total_tm);

	return (void*)0;
}

static void* thread_handler(void* arg)
{
	int random;
	char* pread, * psend;
	header_s* ph;
	int index;
	int select;
	int err;
	MsgTag* msg;

	while (true)
	{
		err = pthread_mutex_lock(&thread_mutex);
		if (err != 0)
		{
			printf("In thread_handler, func pthread_mutex_lock failed\n");
		}
		//while ((pread = out_read_queue()) == nullptr && EXIT == false)
		while (readQueueHead == nullptr && EXIT == 0)
		{
			//printf("YES1\n");
			//执行pthread_cond_wait()的时候，会卡在这里，且m_pthreadMutex会被释放掉
			//服务器程序刚初始化的时候，每个worker进程的所有线程必然是卡在这里等待的
			err = pthread_cond_wait(&read_cond, &thread_mutex); //B
			if (err != 0)
			{
				printf("In thread_handler, func pthread_cond_wait failed\n");
			}
			//printf("YES2\n");
		}

		//usleep(0);
		//pread = out_read_queue();
		while (__sync_lock_test_and_set(&readLOCK, 1))
		{
			usleep(0);
			//printf("yes2-------%d\n", pthread_self());
		}
		//============================ATOMIC LCOK FOR READ===============================
		if (readQueueHead != nullptr)
		{
			pread = reinterpret_cast<char*>(readQueueHead) + sizeof(MsgTag);
			//msg = readQueueHead;
			readQueueHead = readQueueHead->next;
			//msg->next = nullptr;
			if (readQueueHead == nullptr)
				readQueueTail = nullptr;
			--readQueueSize;
		}
		else pread = nullptr;
		//============================ATOMIC LCOK FOR READ===============================
		__sync_lock_release(&readLOCK);


		//能走下来的，必然是拿到了的消息队列中的数据或者 EXIT == true
		err = pthread_mutex_unlock(&thread_mutex); //C
		if (err != 0)
		{
			printf("In thread_handler, func pthread_mutex_unlock failed\n");
		}

		//先判断线程退出这个条件
		if (EXIT)
		{
			if (pread != NULL)
			{

#if (VER == MY_MALLOC)
				//mempool->TzFree(pread);
				mempool->TzFree(pread - 8);
#endif
#if (VER == TC_MALLOC)
				free(pread - 8);
#endif
			}
			break;
		}

		/*if (pread != nullptr)
		{*/
			//printf("yes2-------%d\n", pthread_self());
			ph = (header_s*)pread;
			index = ph->index;

			random = 32 + rand() % 4050;
			//random = 32 + rand() % 200;
			//random = 4050 + rand() % 32;
			//random = 32;
			random += 8;

#if (VER == MY_MALLOC)
			//psend = (char*)mempool->TzMalloc(random);
			psend = (char*)mempool->TzMalloc(random) + 8;
#endif
#if (VER == TC_MALLOC)
			psend = (char*)malloc(random) + 8;
#endif

			ph = (header_s*)psend;
			ph->index = index;

			MsgTag* msg = reinterpret_cast<MsgTag*>(psend - sizeof(MsgTag));
			msg->next = nullptr;

			while (__sync_lock_test_and_set(&sendLOCK, 1))
			{
				usleep(0);
				//printf("yes3-------%d\n", pthread_self());
			}
			{
				//++sendQueueSize;
				if (sendQueueTail == nullptr)
				{
					sendQueueHead = msg;
					sendQueueTail = msg;
				}
				else
				{
					sendQueueTail->next = msg;
					sendQueueTail = msg;
				}
				//++sendQueueSize;
			}
			__sync_lock_release(&sendLOCK);
			
#if (VER == MY_MALLOC)
			//mempool->TzFree(pread);
			mempool->TzFree(pread - 8);
#endif
#if (VER == TC_MALLOC)
			free(pread - 8);
#endif
		//}
	}

	return (void*)0; //线程结束
}
	
int index_order = 0;
int icount = 0;

static void* thread_send(void* arg)
{
	char* psend;
	header_s* ph;
	int err;
	int index;
	MsgTag* msg = nullptr;

	while (EXIT == 0)
	{
		while (__sync_lock_test_and_set(&sendLOCK, 1))
		{
			//printf("yes4-------%d\n", pthread_self());
			usleep(0);
		}
		msg = sendQueueHead;
		sendQueueHead = nullptr;
		sendQueueTail = nullptr;
		//sendQueueSize = 0;
		__sync_lock_release(&sendLOCK);

		//printf("YES3\n");

		while (msg)
		{
			//printf("YES3\n");
			psend = reinterpret_cast<char*>(msg) + sizeof(MsgTag);
			msg = msg->next;

			ph = (header_s*)psend;
			index = ph->index;

#if (VER == MY_MALLOC)
			//mempool->TzFree(psend);
			mempool->TzFree(psend - 8);
#endif

#if (VER == TC_MALLOC)
			free(psend - 8);
#endif
			
			//--sendQueueSize;
			//printf("index_order -- %d\n", index_order);
			if (++index_order == runTimes)
			{
				index_order = 0;
				printf("read queue size: %d\n", readQueueSize);
				//printf("send queue size: %d\n", sendQueueSize);
				//printf("ngx_get_Free_pool_size: %d\n", mempool->TzGetFreepoolSize());
				/*for (THREAD_CACHE_S* tc = mempool->__thread_heaps; tc; tc = tc->_next)
				{
					printf("deposit_size: %d === tid %d\n",
						tc->_Tdeposit_size, tc->_Tid);
				}*/
				ISSTOP = 1;
				if (++icount >= repeatTimes)
				{
					EXIT = 1;
					/*for (int i = 0; i < handleThreads; i++)
					{
						sem_post(&read_sem);
					}*/
					pthread_mutex_lock(&thread_mutex);
					pthread_cond_broadcast(&read_cond);
					pthread_mutex_unlock(&thread_mutex);
					printf("send thread ==== %d finished!\n", pthread_self());
				}
			}
		}

		usleep(100);
	}

//	while (true)
//	{
//		err = pthread_mutex_lock(&thread_mutex1);
//		if (err != 0)
//		{
//			printf("In thread_send, func pthread_mutex_lock failed\n");
//		}
//
//		while ((psend = out_send_queue()) == nullptr && EXIT == false)
//		{
//
//			//执行pthread_cond_wait()的时候，会卡在这里，且m_pthreadMutex会被释放掉
//			//服务器程序刚初始化的时候，每个worker进程的所有线程必然是卡在这里等待的
//			err = pthread_cond_wait(&send_cond, &thread_mutex1); //B
//			if (err != 0)
//			{
//				printf("In thread_send, func pthread_cond_wait failed\n");
//			}
//		}
//
//		//能走下来的，必然是拿到了的消息队列中的数据或者 EXIT == true
//		err = pthread_mutex_unlock(&thread_mutex1); //C
//		if (err != 0)
//		{
//			printf("In thread_handler, func pthread_mutex_unlock failed\n");
//		}
//
//		//先判断线程退出这个条件
//		if (EXIT)
//		{
//			if (psend != NULL)
//			{
//#if (VER == TC_MALLOC)
//				free(psend);
//#endif
//
//#if (VER == MY_MALLOC)
//				mempool->TzFree(psend);
//				//free(psend - 8);
//#endif
//			}
//			break;
//		}
//
//		//能走到这里的，就是有数据可以处理
//		if (psend != nullptr)
//		{
//			ph = (header_s*)psend;
//			index = ph->index;
//
//			mempool->TzFree(psend);
//			//free(psend - 8);
//
//			if (__sync_add_and_fetch(&index_order, 1) == runTimes)
//			{
//				index_order = 0;
//				printf("read queue size: %d\n", readQueueSize);
//				printf("send queue size: %d\n", sendQueueSize);
//				printf("ngx_get_Free_pool_size: %d\n", mempool->TzGetFreepoolSize());
//				/*for (THREAD_CACHE_S* tc = mempool->__thread_heaps; tc; tc = tc->_next)
//				{
//					printf("deposit_size: %d === tid %d\n",
//						tc->_Tdeposit_size, tc->_Tid);
//				}*/
//				ISSTOP = true;
//				if (++icount >= repeatTimes)
//				{
//					EXIT = true;
//					pthread_mutex_lock(&thread_mutex);
//					pthread_cond_broadcast(&read_cond);
//					pthread_mutex_unlock(&thread_mutex);
//
//					/*for (int i = 0; i < handleThreads; i++)
//					{
//						if (sem_post(&lockFreeQueueH[i].sem) == -1)
//						{
//							printf("func sem_post(&sem) failed\n");
//						}
//					}*/
//
//					pthread_mutex_lock(&thread_mutex1);
//					pthread_cond_broadcast(&send_cond);
//					pthread_mutex_unlock(&thread_mutex1);
//
//					printf("send thread ==== %d finished!\n", pthread_self());
//				}
//			}
//		}
//	}
	return (void*)0; //线程结束
}


int main(int argc, char* argv[])
{
	//======================================================================================

	p_crc32 = CCRC32::GetInstance();
	mempool = TzMemPool<_SPN512K, _EX>::GetInstance();
	mempool->TzCreatePool();
	//{
	//	printf("In thread_alloc, ngx_create_pool failed\n");
	//	return -1;
	//}

	std::vector<pthread_t> vecThrd;
	std::vector<pthread_t> vecThrdsend;

	pthread_t tid_read, tid_send;

	for (int i = 0; i < handleThreads; ++i)
	{
		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_handler, NULL))
		{
			printf("thread creating failed!");
			exit(-2);
		}
		vecThrd.push_back(tid);
		//sleep(0.5);
	}
	sleep(0.5);

	//for (int i = 0; i < sendThreads; ++i)
	//{
	//	pthread_t tid;
	//	if (pthread_create(&tid, NULL, thread_send, NULL))
	//	{
	//		printf("thread creating failed!");
	//		exit(-2);
	//	}
	//	vecThrdsend.push_back(tid);
	//}
	//sleep(0.5);


	if (pthread_create(&tid_send, NULL, thread_send, NULL))
	{
		printf("thread creating failed!");
		exit(-2);
	}

	//============================begin========================================
	if (pthread_create(&tid_read, NULL, thread_read, NULL))
	{
		printf("thread creating failed!");
		exit(-2);
	}

	/*if (pthread_create(&tid_send, NULL, thread_send, NULL))
	{
		printf("thread creating failed!");
		exit(-2);
	}*/

	//.......................................................................................

	if (pthread_join(tid_read, NULL))
	{
		printf("thread %d is not exit...\n", tid_read);
	}

	if (pthread_join(tid_send, NULL))
	{
		printf("thread %d is not exit...\n", tid_send);
	}

	for (int i = 0; i < handleThreads; i++)
	{
		if (pthread_join(vecThrd[i], NULL))
		{
			printf("thread[%d] %d is not exit...\n", i, vecThrd[i]);
		}
	}

	//for (int i = 0; i < sendThreads; i++)
	//{
	//	if (pthread_join(vecThrdsend[i], NULL))
	//	{
	//		printf("thread[%d] %d is not exit...\n", i, vecThrdsend[i]);
	//	}
	//}

	printf("ngx_get_Free_pool_size after finishing: %d\n", mempool->TzGetFreepoolSize());
	return 0;
}
