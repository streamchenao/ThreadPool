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

//����Ǵ�����ֵ�� ���������أ�
// ��ζ���� ���Է����������͵ķ���ֵ�ĺ����أ�  
// ģ���� ȷʵ��һ��˼· �����������ṩ
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








// Ŀǰָ����Ҫ��һ�� ʹ���ϵ��Ż��ġ�                   

/*
��ǰv1.0�汾����Ҫ�û� ��һ���Լ��������࣬���Ҽ̳��� ���ǵ�����࣬��дrun������������run�����������Լ�������������

�����û�ʹ����˵�鷳�ĵط�: ����ֱ�� ���˵�    new һ���Լ�д�������� ����ָ�롣����Ҫ��  shared_ptr �ٰ�һ�㡣 ��һ���ԭ��Ҳ�ܼ򵥣��̳߳ص��߼���Ҫ���� ����û�����������󣬵��� �û��Զ��������������������� ��ȫ���ɿء�
������һ��ǿ����ָ�룬���ӳ� ����˵ �й��û���������� ���������ڡ�

��������һ�����ʣ� ��shared_ptr ��ֱ�� ������ָ�� ��Ϊ ���캯���Ĳ��� ���� operator= �Ĳ�����
Ҳ����˵�ܲ�������ʹ�ã�shared_ptr<BaseClass> ptr = new MyClass();

��ʵ��ͨ����shared_ptr<BaseClass> ptr = std::make_shared<MyClass>();


�Ż�˼·��
1. ģ��Ԫ��� + ģ��ɱ�� + ��ֵ���� + ��������ת�� + �����۵� 
2. ������׼���е�  package_task ��  future               ��async �Լ� promise Ӧ���е��ϵ

submitTask(func,param1,param2,parm3)
*/
int main()
{
	{
		ThreadPool pool;   //�̳߳ص���Դ���� ��δ���   ��ôд�̳߳ض������������
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