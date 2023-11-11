#include "ThreadPool.h"
#include <chrono>


void BaseTask::exec()
{
	if (result != nullptr)
	{
		result->setVal(run());
	}
}



class taskA : public BaseTask
{
public:
	taskA(int x,int y, TaskState state = TaskState::PENDING):
		state(state)
	,x(x)
	,y(y)
	{}
	myAny run() {
		std::cout << "Tid:" << std::this_thread::get_id() << " start do task A " << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		int total = 0;
		for (int i = x; i < y; i++)
		{
			total += i;
		}
		std::cout << "Tid:" << std::this_thread::get_id() << " task A done " << std::endl;
		return total;
	}
	void setTaskState(TaskState state) { state = state; }

	TaskState getTaskState() { return state; }
private:
	TaskState state;
	int x;
	int y;
};

//如果是带返回值的 任务处理函数呢？
// 如何定义出 可以返回万能类型的返回值的函数呢？  
// 模板吗？ 确实是一种思路 但这里我们提供
//



class A
{
public:
	A(int x,int *p=nullptr):x(x),ptr(p){}
	~A() {}
	A(A&& rst) {
		x = rst.x;
		ptr = rst.ptr;
		if (rst.ptr != nullptr) delete rst.ptr;
		rst.ptr = nullptr;
		std::cout << "A(A&& rst) " << std::endl;
	}
	A& operator=(A&& rst) {
		x = rst.x;
		ptr = rst.ptr;
		if(rst.ptr != nullptr) delete rst.ptr;
		rst.ptr = nullptr;
		std::cout << "A& operator=(A&& rst) " << std::endl;
		return *this;
	}
	static A get()
	{
		return A(110);
	}

private:
	int x;
	int* ptr;
};








// 目前指定是要做一点 使用上的优化的。                   

/*
当前v1.0版本，需要用户 整一个自己的任务类，并且继承自 我们的虚基类，重写run方法，进而在run方法中描述自己的想做的任务。

对于用户使用来说麻烦的地方: 不是直接 明了地    new 一个自己写的任务类 的裸指针。而是要用  shared_ptr 再包一层。 包一层的原因也很简单：线程池的逻辑里要依赖 这个用户的任务类对象，但是 用户自定义的任务类对象，生命周期 完全不可控。
所以用一个强智能指针，来延长 或者说 托管用户任务类对象 的生命周期。

这里我有一个疑问： 用shared_ptr 能直接 接收裸指针 作为 构造函数的参数 或者 operator= 的参数吗？
也就是说能不能这样使用：shared_ptr<BaseClass> ptr = new MyClass();

但实际通常：shared_ptr<BaseClass> ptr = std::make_shared<MyClass>();


优化思路：
1. 模版元编程 + 模版可变参 + 右值引用 + 类型完美转发 + 引用折叠 
2. 依赖标准库中的  package_task 和  future               和async 以及 promise 应该有点关系

submitTask(func,param1,param2,parm3)
*/
int main()
{
	{
		ThreadPool pool;   //线程池的资源回收 如何处理？   怎么写线程池对象的析构函数
			//pool.setMode(MODE::CACHED);

		pool.start(4);


		Result rst = pool.commitTask(std::make_shared<taskA>(1, 100));
		//Result rst2 = pool.commitTask(std::make_shared<taskA>(101, 200));
		//
		//Result rst3 = pool.commitTask(std::make_shared<taskA>(1, 100));
		//Result rst4 = pool.commitTask(std::make_shared<taskA>(101, 200));
		//
		//Result rst5 = pool.commitTask(std::make_shared<taskA>(1, 100));
		//Result rst6 = pool.commitTask(std::make_shared<taskA>(101, 200));

		int total1 = rst.get().cast<int>();

		cout << total1 << endl;
	}
	cout << " main " << endl;

	return 0;
}