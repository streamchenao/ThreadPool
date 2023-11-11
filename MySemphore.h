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

//我们这里实现二元信号量。   其实信号量本身是多元的，二元就是二值，只有0和1，看起来就像一把锁一样。
class MySemphore
{
public:
	MySemphore(int limit = 0) :resourse_limit(limit)
	{
	}
	~MySemphore(){}
	MySemphore(MySemphore&& rst) = default;
	MySemphore& operator=(MySemphore&& rst) = default;
	//信号量wait，对应于 信号量资源-1的操作，也就是 v操作
	void wait()
	{
		std::unique_lock<std::mutex> ul(mtx);//抢锁，抢到锁才能继续往下执行
		//if (resourse_limit > 0)
		//{
		//	resourse_limit--;
		//}
		//else
		//{
		//	cv.wait(ul);//不带条件的
		//}
		while (resourse_limit <= 0)
		{
			std::cout << " 用户线程开始阻塞等待任务的执行结果 " << std::endl;
			cv.wait(ul);
		}
		resourse_limit--;
		//可以 优雅地简写成下面的样子  cv.wait(ul, [&]()-> bool { return resourse_limit > 0; }); 满足后面这个 谓词或者说函数对象 的条件，且抢到了锁的情况下，这里就不会阻塞，否则一直阻塞
	}
	
	//信号量post 对应于信号量资源+1操作，也就是p操作。
	void post()
	{
		std::unique_lock<std::mutex> ul(mtx);//抢锁     只会有一个MySemphore的对象 存在
		resourse_limit++;
		
		cv.notify_all();// 信号量资源加1，通知wait的线程，可以起来干活了
		std::cout << " 用户线程被唤醒拿到计算结果 " << std::endl;
	}

private:
	int resourse_limit;
	std::mutex mtx;
	std::condition_variable cv;
};
