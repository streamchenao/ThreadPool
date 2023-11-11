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

	�ӹٷ���cppreference������ �̶߳���  �� �����Ĳ���ϵͳ�����Ǵ������߳� ���������й�����ϵ�� Ҳ�����ᵽ��associated
	����� �̶߳�����ù�  join��detach��������Ĭ�Ϲ����(Ҳ����ɶ����������)�������� �ƶ����������  ���������ϵ�Ͳ������ˡ�

	Ҳ����˵���̶߳������������ ��  ͨ�������������̵߳��������� �ͷ��뿪�ˡ�
	���������ϵ�����ڣ���ô�̶߳��������ˣ��߳̾ͻᱻ ����ϵͳ���� terminate()���� ֱ����ֹ�����õ������ ���ܶ���ȫ������ִ���̺߳�������������� �̺߳���ִ����һ�룬�ͱ���ֹ�ˡ��ᵼ��д���ڴ�������� ���� ���²������������ݡ�

	*/
	void start()
	{
		std::thread t(handler_,tid); // ���������̵߳�ʱ��       Tips:�������Ҫ�Ѹմ����õ��߳� �� ���̷߳��롣 ��Ȼ��������֮���̶߳�����������ˡ��̺߳���ѹ��û��ִ�еĻ����ˣ���������Ҫ�����̷߳��롣����֮���߳�ֻ����ִ�����̺߳���֮�������˳�
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


// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
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

	// ����һ��setVal��������ȡ����ִ����ķ���ֵ��
	void setVal(myAny any)  // ˭���õ��أ�����
	{
		// �洢task�ķ���ֵ
		this->any_ = std::move(any);
		sem_.post(); // �Ѿ���ȡ������ķ���ֵ�������ź�����Դ
	}
	// �������get�������û��������������ȡtask�ķ���ֵ
	myAny get() 
	{
		if (!isValid_)
		{
			return "";
		}
		sem_.wait(); // task�������û��ִ���꣬����������û����߳�
		return std::move(any_);

	}
private:
	myAny any_; // �洢����ķ���ֵ
	MySemphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<BaseTask> task_; //ָ���Ӧ��ȡ����ֵ��������� 
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч
};



const int TASK_THRESHOLD = 1024;
const int THREAD_THRESHOLD = 1000;

const int THREAD_MAX_IDLE_TIME = 10;//�߳�������ʱ��    60s
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
		//notEmpty.notify_all();  //��fixed ģʽ��
		isPoolRunning = false; //�̳߳�״̬������֪ͨ ����detach���߳��˳�
		// �ɵ����� �̣߳������̳߳�����߳�   ���û����߳�  detach�ˣ� ���Գ�����  �̼߳�ͨ���ֶ� ���̳߳��߳�  ���� reture ���� exit�����ƺ�û�������ĺð취�ˡ�

		//���е����̣߳�������� �̺߳�����ȴ�һ����������������������������ˣ�������reture����������㣬��10ms��Ȼ��ͼ����̺߳�������������ҵ��  
		//  ��������Ҫ�õ���wait_for �Լ� ����ֵ �� time_out ���� not_timeout
		//needExit.notify_all();
		
		//�����˼·�����⣬�û��߳�Ҫ�������̵߳�֪ͨ��Ҫ�ȣ�Ҫ�ȣ�Ҫ�ȡ� �������û��߳�֪ͨ������ �̳߳��̣߳���Ϊ�̳߳��߳� ������ �û��߳��������� ThreadPool����
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
		//һ���������Ӧ��CPU���������е��߳�
		isPoolRunning = true;
		for (int i = 0; i < initThreadCnt; ++i)
		{
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); //ֻ��Ҫ����unique_ptr ������Ķ���� ���캯���Ĳ����ͺ��ˣ�����Ҫ�Լ�new��
			//threadList.emplace_back(std::move(ptr));
			size_t tid = ptr->getTid();
			threadMap.emplace(tid, std::move(ptr)); //����Ҳ����ͬ���Ĵ���ע�⺯�� ʵ�ε�ѹջ˳�򣬴�������ѹ������ptr�ϵ���Դ�Ѿ�ת���ˣ��ٷ���getTid ��Ȼ����   
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

    //���������������ֵ
	void setTaskQueThreshold(size_t threshold) {
		if (isPoolRunning) {
			return;
		}
		taskQueThreshold = threshold;
	}


	//��ϣ����¶���û� �������� �� ��ֵ��������ء���Ϊ��Դ�����̫���ˣ�����ϣ���û������Ե���ģʽ��ʹ�ã���ʹ��������Ҳ��Ҫ�����е��̳߳ض����п���
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;



	// ���û�ʹ������̳߳ؿ��ʱ����Ҫ�û��Լ������ύ�Զ�������񣬺Ͷ�Ӧ�Ĵ�����
	Result commitTask(std::shared_ptr<BaseTask> task) {
		
		//1. ��ȡ��
		std::unique_lock<std::mutex> ul(taskQueMtx);
		//2. �̼߳�ͨ��    �ȴ������п��� ���� �̳߳ؿ��������ṩ����
		//uint32_t now = std::chrono::system_clock::now;
		//����ģ�����edge case����������£���������������ŵģ��û����� commitTask�ͻ�һֱ������������һ�����񽵼��������û������ȴ�2s֮�󣬾͸��û����ش�����ʾ��������æ
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
		//3. ����п���  ����������
		taskQue.emplace(task); //taskQue.push(task);
		//4. ֪ͨ�������߳� �������ѡ�����������notEmpty�Ͻ���֪ͨ
		notEmpty.notify_all();
		//taskQueMtx.unlock();

		//-------------------------------------------------CACHED---------------------------------------------------------
		//���Ҫ֧�� Cachedģʽ����Ȼ��Ҫ  �������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̷߳ŵ� ���ǵ�vector����ȥ��
		//1. �ж�������ʲô����δ������߳��أ�
		//2. �Ƿ�Ҫ��ȡ �̴߳��ڿ���״̬��ʱ���أ�   �Լ����ڿ���״̬���߳������أ�
		//���ĵ㣺a) ���񼱾�����ʱ��������С������CPU�ܼ��͵ģ� ��Ҫ�����µ��߳���֧��
		//        b) ��������ò���ˣ���������̣߳�Ҫ���յ���           ��������̣߳�������˵���ǿ���һ��ʱ����̡߳�   ������Ҫͳ���̵߳Ŀ���ʱ��

		std::cout << " idleThreadNum: " << idleThreadNum << std::endl;
		if (myMode == MODE::CACHED 
			&& curThreadNum < THREAD_THRESHOLD 
			&& taskQue.size() > idleThreadNum) 
		{
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); //ֻ��Ҫ����unique_ptr ������Ķ���� ���캯���Ĳ����ͺ��ˣ�����Ҫ�Լ�new��
			//threadList.emplace_back(std::move(ptr));
			size_t tid = ptr->getTid();
			threadMap.emplace(tid,std::move(ptr)); 
			/*ptr->start();//Bug #1:��һ���Ѿ���ptr ����Դת���ˣ�ptr�൱����һ����ָ���ˡ��������ﲻ���ٷ�����*/

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
	 �������һ�� ���⣺
	 ��� ThreadPool �������ˣ���ΪThreadPool�����Ƕ����� �û��߳���ģ����п����������ˣ�  ����ζ�� ����ThreadPool�ĳ�Ա����Ҳ���������������ǲ������������ while (taskQue.size() == 0) ��ѭ�����߼����ж� isPoolRuning�Ƿ�Ϊfalse
	 ˵���˾��ǣ������� ����һ��������ĳ�Ա�������ж���������Ƿ������ˡ�  ��һ������ֿ�ָ���쳣�����߷Ƿ������ڴ��쳣
	 ���� ThreadPool ����������������������Ҳ�������Ͼ������������ �����̶߳��˳���  ���Ա���Ҫ�� ThreadPool��������������������
	*/

	void threadHandler(int tid)
	{
		//std::chrono::system_clock::time_point idleStart = std::chrono::system_clock::now();
		//std::chrono::system_clock::time_point idleEnd = idleStart;
		std::cout << "Thread:" << tid << " real_tid: " << std::this_thread::get_id() << " start " << std::endl;
		auto last = std::chrono::high_resolution_clock().now(); //��ʼ��last��ÿ���̺߳��� һ����ֻ�ܳ�ʼ����ôһ��
		while (1)
		{
			std::shared_ptr<BaseTask> task_ptr;
			{
				std::unique_lock<std::mutex> ul(taskQueMtx); 
				// ֻҪ ������в��� �Լ�����ȥ��������Ҳ���Ǵ�����������ȡһ�����񡣻�ȡ�� �����ͷ���
				std::cout << "tid:" << std::this_thread::get_id()
					<< "���Ի�ȡ����..." << std::endl;

				//����д����Щ���ң�����ԭ���� �� ������ⲻ������        ����Ҫ���ǣ���������Ƶ�����̳߳� �� �û��ύ������  ���ߵ��������ڡ�   1. �̳߳ؽ������������Ƿ������񣬲��������Ƿ�ִ�У�ֱ���˳���   
																																			//2. �̳߳ؽ������� �����̳߳��Ƿ��Ƚ�����������ø���ִ���꣬Ҳ����ʹ�����Ƿ����÷���ֵ�� get��ͬ���ȴ��� ��Ҳ�ø���ִ���� 
				while (taskQue.size() == 0 && isPoolRunning) { //����ȥ��isPoolRunning �������2��ֻҪtaskQue.size����0���̳߳ص�û�������� �����󣬼�������ȡ����ִ�С�  �����ȥ���������̳߳�ֻҪ ״̬һ�仯���̳߳�����߳� �ͱ�ɻ��ˣ�ֱ���˳���
					//idleThreadNum++;// �����Բ�Ҫ��ԭ���ǣ�������ʼ����ʱ�� �߳̾���idle״̬�ġ�ֻ��������ȡ��task֮�󣬲ű��running״̬��
					//notEmpty.wait(ul); �²�Ӧ������������ Ҳ���ܽ�������Ӧ���� ������û���ٱ����ѵĿ��ܣ�Ȼ��һ��ʱ������ϵͳֱ�� ɱ��������̡߳�   �����������߳� ������ �������������յ� �ȴ�
					if (!isPoolRunning)
					{
						threadMap.erase(tid);
						std::cout << "thread:" << std::this_thread::get_id() << " is exit and delete from threadPool" << std::endl;
						needExit.notify_all();
						return;
					}
					if (myMode == MODE::CACHED && std::cv_status::timeout == notEmpty.wait_for(ul, std::chrono::seconds(1)))               //����ÿ��1s�����һ��  �������Ϊ����1s�����أ����ǳ�ʱ�˳����������Ϊ����������֪ͨ�����أ������������أ�˵������������Ҫ����
					{
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last);

							if (duration.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadNum > initThreadCnt)
							{
								//���յ�ǰ�߳�        Ҳ���Ǵ��߳��б��� �Ѵ�ʱ�˿����� threadHandler���̶߳���ɾ����  ��ô�ҵ���Ӧ���̶߳����б�����Ǹ������ء�      ����Ҫ��ӳ���ϵ����ѽ
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
						notEmpty.wait(ul);    //�ȴ���������֪ͨ�� ���� ��֪ͨ�ĵط�һ��Ҫע�⡣              ����㷢���߳��Ѿ�����������ģ�˵�� ���߳�����������  ��֪ͨ���̣߳�һ��Ҫ��������֮���ٵ���notify_all
						                                                                                  //  ����㷢���߳��������� ֪ͨ���߳��ϣ�˵�� ���̺߳���������  ������������û����֪ͨ�ˡ�
						                                                                                  //  ����Ҫ֪�����ǣ�֪ͨ�߳�������֮�󣬱�Ȼ�޸���ĳЩ ������    ˵�������ѱ������ˣ� ���̵߳Ĵ����߼���������ȶ�֪ͨ�߳����޸ĵ����������ж�
					}                                                                                    //   ��������˵�������  �̲߳���ȫ�� ��������ģʽ  ���� �����ϵĹ��ԡ�
																										// ���������� ���ж� ��Դ�Ƿ�Ϊ�գ�Ϊ��������������������ж�һ����Դ�Ƿ�Ϊ�գ�Ϊ�գ����������� ��Դ����
																										//����֮������  ������ ��Ҫ���ж�һ����Դ�Ƿ�Ϊ�ա��ܶ��˾ͻ�˵�����������ˣ�ֱ�Ӵ�����Դ���󲻾ͺ��ˡ� 
																										//���ǣ����̻߳����£������̣߳�����ͬʱ������˵����Խ���˵�һ���ж�������
																										//�����ʱ����һ���������ˣ���������Դ��Ȼ���ͷ���������һ����������������Ҳ����������Դ��   �ͻ�ͬʱ����������Դ��Υ����������ͼ
				}                                                                           //���������������������Դ���� ����� �����ȴ�����������֪ͨ��  isPoolRunning��һ�����������
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
				//		//���յ�ǰ�߳�        Ҳ���Ǵ��߳��б��� �Ѵ�ʱ�˿����� threadHandler���̶߳���ɾ����  ��ô�ҵ���Ӧ���̶߳����б�����Ǹ������ء�      ����Ҫ��ӳ���ϵ����ѽ
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
						<< "��ȡ����ɹ�..." << std::endl;
				}
				//// ���������Ϊ��ʱ����������ʼ�ȴ� ��������
				//else
				//{
				//	//������ʼʱ��
				//	idleStart = std::chrono::system_clock::now();
				//
				//}
			}
			idleThreadNum--;
			try
			{
				 //ֻ����������õ� �û��Զ���run�����ķ���ֵ�����Ա�ȻҪ������ �õ�����ֵ�����õ�result��
				//task_ptr->run();
				//task_ptr->get()->setVal(task_ptr->run());
				task_ptr->exec();
				last = std::chrono::high_resolution_clock().now();//��¼��һ���̴߳����������ʱ�̣��״ν����̺߳���ʱ�ᱻ��ʼ��
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
	
	std::vector<std::unique_ptr<Thread>> threadList;//��Ҫһ������̵߳����ݽṹ�� ��Ϊֻ��Ҫ �����Լ��������е��̣߳��������ֱ������ָ��

	std::unordered_map<int, std::unique_ptr<Thread>> threadMap;
	size_t idleThreadNum; //�����߳����� Ҳ���� �����̵߳�����
	size_t initThreadCnt;// �û�ָ���ĳ�ʼ�̳߳����̵߳�����

	size_t curThreadNum;//��ǰ�������е��߳�����

	std::queue<std::shared_ptr<BaseTask>> taskQue;//�������
	size_t taskQueThreshold; //������е�����������ֵ
	//��Ϊ���� �����Լ����̳߳�����߳� ����� ������У��û��߳�Ҳ������������,������Ҫ���� �̼߳�ͨ�ŵĲ�������������������
	std::mutex taskQueMtx;
	//std::unique_lock<std::mutex> taskQueMtx;
	//�����淨��������������������һ������һ��������
	std::condition_variable notFull;// ���� ������в���
	std::condition_variable notEmpty;// ���� ������в���

	std::condition_variable needExit;
	std::atomic_bool isPoolRunning;
};




#endif