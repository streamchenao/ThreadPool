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

//��������ʵ�ֶ�Ԫ�ź�����   ��ʵ�ź��������Ƕ�Ԫ�ģ���Ԫ���Ƕ�ֵ��ֻ��0��1������������һ����һ����
class MySemphore
{
public:
	MySemphore(int limit = 0) :resourse_limit(limit)
	{
	}
	~MySemphore(){}
	MySemphore(MySemphore&& rst) = default;
	MySemphore& operator=(MySemphore&& rst) = default;
	//�ź���wait����Ӧ�� �ź�����Դ-1�Ĳ�����Ҳ���� v����
	void wait()
	{
		std::unique_lock<std::mutex> ul(mtx);//���������������ܼ�������ִ��
		//if (resourse_limit > 0)
		//{
		//	resourse_limit--;
		//}
		//else
		//{
		//	cv.wait(ul);//����������
		//}
		while (resourse_limit <= 0)
		{
			std::cout << " �û��߳̿�ʼ�����ȴ������ִ�н�� " << std::endl;
			cv.wait(ul);
		}
		resourse_limit--;
		//���� ���ŵؼ�д�����������  cv.wait(ul, [&]()-> bool { return resourse_limit > 0; }); ���������� ν�ʻ���˵�������� ������������������������£�����Ͳ�������������һֱ����
	}
	
	//�ź���post ��Ӧ���ź�����Դ+1������Ҳ����p������
	void post()
	{
		std::unique_lock<std::mutex> ul(mtx);//����     ֻ����һ��MySemphore�Ķ��� ����
		resourse_limit++;
		
		cv.notify_all();// �ź�����Դ��1��֪ͨwait���̣߳����������ɻ���
		std::cout << " �û��̱߳������õ������� " << std::endl;
	}

private:
	int resourse_limit;
	std::mutex mtx;
	std::condition_variable cv;
};
