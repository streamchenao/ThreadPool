#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>

#include <mutex>
#include <condition_variable>
#include <functional>
#include "MyObjectClass.h"
#include "MySemphore.h"

#include <thread>
#include <future>
enum class MODE
{
	FIXED,
	CACHED,
};

enum class TaskState
{
	PENDING,
	PROCESSING,
	DOEN,
};
class Thread
{
public: 
	using Threadhandler = std::function<void(int)>;
	Thread(Threadhandler handler):
		handler_(handler)
		,tid(idGenerator++)
	{}
	~Thread() = default;

	size_t getTid()
	{
		return this->tid;
	}

	/*
	A thread object does not have an associated thread (and is safe to destroy) after

	it was default-constructed.
	it was moved from.
	join() has been called.
	detach() has been called.

	从官方的cppreference来看， 线程对象  和 真正的操作系统给我们创建的线程 这两者是有关联关系的 也就是提到的associated
	但如果 线程对象调用过  join，detach，或者是默认构造的(也就是啥参数都不传)，或者是 移动拷贝构造的  这个关联关系就不存在了。

	也就是说，线程对象的生命周期 和  通过它所创建的线程的生命周期 就分离开了。
	如果关联关系还存在，那么线程对象析构了，线程就会被 操作系统调用 terminate()函数 直接终止掉。好的情况是 可能都完全来不及执行线程函数，坏的情况是 线程函数执行了一半，就被终止了。会导致写坏内存里的数据 或者 留下不完整的脏数据。

	*/
	void start()
	{
		std::thread t(handler_,tid); // 真正创建线程的时机       Tips:这里必须要把刚创建好的线程 和 主线程分离。 不然出作用域之后，线程对象就析构掉了。线程函数压根没有执行的机会了，所以这里要和主线程分离。分离之后，线程只会在执行完线程函数之后，主动退出
		std::cout << "Thread:" << tid  << " start "<< std::endl;
		t.detach();
	    //t.join();
	}
private:
	Threadhandler handler_;
	size_t tid;
	static int idGenerator;
};
int Thread::idGenerator = 0;

class Result;
class BaseTask
{
public:

	BaseTask()
		: result(nullptr)
	{}
	~BaseTask() = default;
	virtual myAny run() = 0;
	void exec();

	void setResult(Result* res)
	{
		result = res;
	}

private:
	Result* result;
};


// 实现接收提交到线程池的task任务执行完成后的返回值类型Result
class BaseTask;
class Result
{
public:
	Result(std::shared_ptr<BaseTask> task, bool isValid=true)
		: isValid_(isValid)
		, task_(task)
	{
		task_->setResult(this);
	}
	~Result() = default;

	// 问题一：setVal方法，获取任务执行完的返回值的
	void setVal(myAny any)  // 谁调用的呢？？？
	{
		// 存储task的返回值
		this->any_ = std::move(any);
		sem_.post(); // 已经获取的任务的返回值，增加信号量资源
	}
	// 问题二：get方法，用户调用这个方法获取task的返回值
	myAny get() 
	{
		if (!isValid_)
		{
			return "";
		}
		sem_.wait(); // task任务如果没有执行完，这里会阻塞用户的线程
		return std::move(any_);

	}
private:
	myAny any_; // 存储任务的返回值
	MySemphore sem_; // 线程通信信号量
	std::shared_ptr<BaseTask> task_; //指向对应获取返回值的任务对象 
	std::atomic_bool isValid_; // 返回值是否有效
};



const int TASK_THRESHOLD = 1024;
const int THREAD_THRESHOLD = 1000;

const int THREAD_MAX_IDLE_TIME = 10;//线程最大空闲时间    60s
class ThreadPool
{

public:

	ThreadPool():
		initThreadCnt(std::thread::hardware_concurrency())
		, taskQueThreshold(TASK_THRESHOLD)
		, myMode(MODE::FIXED)
		, isPoolRunning(false)
		, idleThreadNum(0)
		, curThreadNum(initThreadCnt)
	{}

	~ThreadPool()
	{
		//notEmpty.notify_all();  //在fixed 模式下
		isPoolRunning = false; //线程池状态，用于通知 其他detach的线程退出
		// 干掉所有 线程，但是线程池里的线程   和用户主线程  detach了， 所以除非用  线程间通信手段 让线程池线程  主动 reture 或者 exit掉，似乎没有其他的好办法了。

		//所有的子线程，具体就在 线程函数里，等待一个条件变量，如果条件变量满足了，就立马reture，如果不满足，等10ms，然后就继续线程函数的正常处理业务。  
		//  所以这里要用到，wait_for 以及 返回值 是 time_out 还是 not_timeout
		//needExit.notify_all();
		
		//上面的思路有问题，用户线程要等其他线程的通知，要等，要等，要等。 而不是用户线程通知其他的 线程池线程，因为线程池线程 会依赖 用户线程所创建的 ThreadPool对象
		std::unique_lock<std::mutex> ul(taskQueMtx);
		notEmpty.notify_all();
		needExit.wait(ul, [&]() -> bool {return threadMap.size() == 0; });
		std::cout << "ThreadPool is destruct " << std::endl;
	}

	void setMode(MODE mode) {
		if (isPoolRunning) {
			return;
		}
		myMode = mode;
	}

	void start(int initThreadCnt) {
		//一次性申请对应于CPU核数的所有的线程
		isPoolRunning = true;
		for (int i = 0; i < initThreadCnt; ++i)
		{
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); //只需要传递unique_ptr 所管理的对象的 构造函数的参数就好了，不需要自己new。
			//threadList.emplace_back(std::move(ptr));
			size_t tid = ptr->getTid();
			threadMap.emplace(tid, std::move(ptr)); //这里也犯了同样的错误：注意函数 实参的压栈顺序，从右往左压，所以ptr上的资源已经转移了，再访问getTid 必然报错。   
		}

		for (int i = 0; i < initThreadCnt; ++i)
		{
			threadMap[i]->start();
			idleThreadNum++;
		}

	}


	void setinitThreadCnt(size_t ThreadCnt) {
		if (isPoolRunning) {
			return;
		}
		initThreadCnt = ThreadCnt;
	}

    //设置任务队列上限值
	void setTaskQueThreshold(size_t threshold) {
		if (isPoolRunning) {
			return;
		}
		taskQueThreshold = threshold;
	}


	//不希望暴露给用户 拷贝构造 和 赋值运算符重载。因为资源管理的太多了，就是希望用户尽量以单例模式来使用，即使不单例，也不要从已有的线程池对象中拷贝
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;



	// 当用户使用这个线程池库的时候，需要用户自己主动提交自定义的任务，和对应的处理函数
	Result commitTask(std::shared_ptr<BaseTask> task) {
		
		//1. 获取锁
		std::unique_lock<std::mutex> ul(taskQueMtx);
		//2. 线程间通信    等待队列有空余 或者 线程池可以正常提供服务
		//uint32_t now = std::chrono::system_clock::now;
		//额外的，考虑edge case，极端情况下，任务队列总是满着的，用户调用 commitTask就会一直阻塞。这里做一个服务降级处理，当用户阻塞等待2s之后，就给用户返回错误，提示服务器正忙
		while(taskQue.size() == taskQueThreshold)
		{
			if (std::cv_status::timeout == notFull.wait_for(ul, std::chrono::seconds(2)))
			{
				std::string err = "The taskQue is full, please wait for a few minutes!!!!!";
					std::cout << err << std::endl;
					return Result(task);
					//rst.setErrMsg(err);
					//return rst;
			}
		}
		//task->set(&rst);
		//3. 如果有空余  放入新任务
		taskQue.emplace(task); //taskQue.push(task);
		//4. 通知消费者线程 进行消费。在条件变量notEmpty上进行通知
		notEmpty.notify_all();
		//taskQueMtx.unlock();

		//-------------------------------------------------CACHED---------------------------------------------------------
		//如果要支持 Cached模式，必然需要  根据任务数量和空闲线程的数量，判断是否需要创建新的线程放到 我们的vector里面去。
		//1. 判断条件是什么？如何创建新线程呢？
		//2. 是否要获取 线程处于空闲状态的时间呢？   以及处于空闲状态的线程数量呢？
		//核心点：a) 任务急剧增多时，而且是小型任务CPU密集型的， 需要创建新的线程来支持
		//        b) 当任务处理得差不多了，多出来的线程，要回收掉。           多出来的线程：具体来说就是空闲一段时间的线程。   所以需要统计线程的空闲时间

		std::cout << " idleThreadNum: " << idleThreadNum << std::endl;
		if (myMode == MODE::CACHED 
			&& curThreadNum < THREAD_THRESHOLD 
			&& taskQue.size() > idleThreadNum) 
		{
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); //只需要传递unique_ptr 所管理的对象的 构造函数的参数就好了，不需要自己new。
			//threadList.emplace_back(std::move(ptr));
			size_t tid = ptr->getTid();
			threadMap.emplace(tid,std::move(ptr)); 
			/*ptr->start();//Bug #1:上一步已经把ptr 的资源转移了，ptr相当于是一个空指针了。所以这里不能再访问了*/

			threadMap[tid]->start();
			std::cout << "create new thread success! Thread:" << tid<< std::endl;
			curThreadNum++;
			idleThreadNum++;
		}


		//std::thread* t1 = new std::thread();



		return Result(task);
	}

private:
	/*
	 这里存在一个 问题：
	 如果 ThreadPool 先析构了，因为ThreadPool对象是定义在 用户线程里的，很有可能先析构了，  这意味着 所有ThreadPool的成员变量也将析构，所以我们不能利用这里的 while (taskQue.size() == 0) 死循环的逻辑，判断 isPoolRuning是否为false
	 说白了就是，我们在 利用一个对象里的成员变量，判断这个对象是否析构了。  这一定会出现空指针异常，或者非法访问内存异常
	 所以 ThreadPool 不能先析构，出作用域了也不能马上就析构，必须等 其他线程都退出。  所以必须要在 ThreadPool的析构函数上做点文章
	*/

	void threadHandler(int tid)
	{
		//std::chrono::system_clock::time_point idleStart = std::chrono::system_clock::now();
		//std::chrono::system_clock::time_point idleEnd = idleStart;
		std::cout << "Thread:" << tid << " real_tid: " << std::this_thread::get_id() << " start " << std::endl;
		auto last = std::chrono::high_resolution_clock().now(); //初始化last。每个线程函数 一辈子只能初始化这么一次
		while (1)
		{
			std::shared_ptr<BaseTask> task_ptr;
			{
				std::unique_lock<std::mutex> ul(taskQueMtx); 
				// 只要 任务队列不空 自己主动去消费任务，也就是从任务队列里获取一个任务。获取完 马上释放锁
				std::cout << "tid:" << std::this_thread::get_id()
					<< "尝试获取任务..." << std::endl;

				//这里写的有些混乱，本质原因是 对 需求理解不清晰。        这里要考虑：我们所设计的这个线程池 和 用户提交的任务  二者的生命周期。   1. 线程池结束——不管是否有任务，不管任务是否执行，直接退出。   
																																			//2. 线程池结束—— 不管线程池是否先结束，任务你得给我执行完，也不管使用者是否会调用返回值的 get来同步等待。 你也得给我执行完 
				while (taskQue.size() == 0 && isPoolRunning) { //这里去掉isPoolRunning 就是情况2。只要taskQue.size大于0，线程池调没调用析构 管它求，继续接着取任务执行。  如果不去掉，就是线程池只要 状态一变化，线程池里的线程 就别干活了，直接退出。
					//idleThreadNum++;// 这句可以不要，原因是，本来初始化的时候 线程就是idle状态的。只有在真正取到task之后，才变成running状态。
					//notEmpty.wait(ul); 猜测应该是这里死锁 也不能叫死锁，应该是 饥饿，没有再被唤醒的可能，然后一段时间后操作系统直接 杀掉了这个线程。   导致了所有线程 都陷入 对条件变量不空的 等待
					if (!isPoolRunning)
					{
						threadMap.erase(tid);
						std::cout << "thread:" << std::this_thread::get_id() << " is exit and delete from threadPool" << std::endl;
						needExit.notify_all();
						return;
					}
					if (myMode == MODE::CACHED && std::cv_status::timeout == notEmpty.wait_for(ul, std::chrono::seconds(1)))               //这里每隔1s，检查一次  如果是因为超过1s而返回，就是超时退出。如果是因为条件变量的通知而返回，就是正常返回，说明还有任务需要处理
					{
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last);

							if (duration.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadNum > initThreadCnt)
							{
								//回收当前线程        也就是从线程列表里 把此时此刻运行 threadHandler的线程对象删除掉  怎么找到对应于线程对象列表里的那个对象呢。      必须要有映射关系才行呀
								threadMap.erase(tid);
								idleThreadNum--;
								curThreadNum--;

								std::cout << "thread:" << std::this_thread::get_id() << " is exit and delete from threadPool" << std::endl;
								return;
							}

						}
					}
					else
					{
						notEmpty.wait(ul);    //等待条件变量通知， 所以 在通知的地方一定要注意。              如果你发现线程已经阻塞在这里的，说明 该线程先抢到锁。  那通知的线程，一定要在抢到锁之后，再调用notify_all
						                                                                                  //  如果你发现线程先阻塞在 通知的线程上，说明 该线程后抢到锁。  后抢到锁，就没法被通知了。
						                                                                                  //  但是要知道的是，通知线程抢到锁之后，必然修改了某些 条件。    说明条件已被更改了， 该线程的处理逻辑，请务必先对通知线程所修改的条件进行判断
					}                                                                                    //   总体上来说，这里和  线程不安全的 懒汉单例模式  具有 本质上的共性。
																										// 懒汉单例： 先判断 资源是否为空，为空则加锁，加完锁，在判断一次资源是否为空，为空，则真正创建 资源对象。
																										//经典之处就在  加完锁 还要再判断一次资源是否为空。很多人就会说，都抢到锁了，直接创建资源对象不就好了。 
																										//但是，多线程环境下，两个线程，都在同时抢锁，说明都越过了第一个判断条件，
																										//如果这时候有一个抢到锁了，创建了资源，然后释放了锁。另一个，就能抢到锁，也继续创建资源。   就会同时存在两个资源，违背单例的意图
				}                                                                           //而针对这里的情况分析，资源就是 这里的 阻塞等待条件变量的通知。  isPoolRunning是一个共享变量。
				if (!isPoolRunning) {
					threadMap.erase(tid);
					std::cout << "thread:" << std::this_thread::get_id() << " is just exit and delete from threadPool when finished processing the task" << std::endl;
					needExit.notify_all();
					return;
				}
				//if (myMode == MODE::CACHED
				//	&& taskQue.size() == 0)
				//{
				//	auto now = std::chrono::high_resolution_clock().now();
				//	auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last);
				//
				//	if (duration.count() >= THREAD_MAX_IDLE_TIME
				//		&& curThreadNum > initThreadCnt)
				//	{
				//
				//		//回收当前线程        也就是从线程列表里 把此时此刻运行 threadHandler的线程对象删除掉  怎么找到对应于线程对象列表里的那个对象呢。      必须要有映射关系才行呀
				//		threadMap.erase(tid);
				//		idleThreadNum--;
				//		curThreadNum--;
				//
				//		std::cout << "thread:" << std::this_thread::get_id() << " is exit and delete from threadPool";
				//		return;
				//	}
				//
				//}

				if (taskQue.size() > 0)
				{
					task_ptr = taskQue.front();
					taskQue.pop();
					//while (!taskQue.empty() && task_ptr->getTaskState() != TaskState::PENDING) task_ptr = nextone;

					//task_ptr->setTaskState(TaskState::PROCESSING);
					if (taskQue.size() > 0)
					{
						notEmpty.notify_all();
					}
					std::cout << "tid:" << std::this_thread::get_id()
						<< "获取任务成功..." << std::endl;
				}
				//// 当任务队列为空时，再主动开始等待 条件变量
				//else
				//{
				//	//空闲起始时刻
				//	idleStart = std::chrono::system_clock::now();
				//
				//}
			}
			idleThreadNum--;
			try
			{
				 //只有这里可以拿到 用户自定的run方法的返回值，所以必然要在这里 拿到返回值，设置到result里
				//task_ptr->run();
				//task_ptr->get()->setVal(task_ptr->run());
				task_ptr->exec();
				last = std::chrono::high_resolution_clock().now();//记录上一次线程处理完任务的时刻，首次进入线程函数时会被初始化
			}
			catch (const std::exception&)
			{
				
			}

			std::cout << "threadHandler end " << std::this_thread::get_id() << std::endl;

			notFull.notify_all();
			idleThreadNum++;


			if (!isPoolRunning)
			{
				threadMap.erase(tid);
				std::cout << "thread:" << std::this_thread::get_id() << " is just exit and delete from threadPool when finished processing the task" << std::endl;
				needExit.notify_all();
				return;
			}
		}
		//taskQueMtx.unlock();
	}

private:
	MODE myMode;
	
	std::vector<std::unique_ptr<Thread>> threadList;//需要一个存放线程的数据结构， 因为只需要 我们自己管理所有的线程，这里可以直接用裸指针

	std::unordered_map<int, std::unique_ptr<Thread>> threadMap;
	size_t idleThreadNum; //可用线程数量 也就是 空闲线程的数量
	size_t initThreadCnt;// 用户指定的初始线程池中线程的数量

	size_t curThreadNum;//当前正在运行的线程数量

	std::queue<std::shared_ptr<BaseTask>> taskQue;//任务队列
	size_t taskQueThreshold; //任务队列的任务数量阈值
	//因为除了 我们自己的线程池里的线程 会访问 任务队列，用户线程也会访问任务队列,所以需要定义 线程间通信的操作，这里用条件变量
	std::mutex taskQueMtx;
	//std::unique_lock<std::mutex> taskQueMtx;
	//常规玩法：定义两个条件变量，一个不空一个不满。
	std::condition_variable notFull;// 代表 任务队列不满
	std::condition_variable notEmpty;// 代表 任务队列不空

	std::condition_variable needExit;
	std::atomic_bool isPoolRunning;
};




#endif