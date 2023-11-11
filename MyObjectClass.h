#include <iostream>

using namespace std;




class MyObject
{
public:
	virtual ~MyObject() {}
};

/*
���м̳��� MyObject��� �����࣬

*/

template<typename T>
class Inherit : public MyObject
{
public:
	Inherit(T data): data(data){}
	T& get()
	{
		return data;
	}
private:
	T data;
};



class myAny
{
public:
	myAny() {}
	~myAny() {}

	myAny(const myAny&) = delete;
	myAny& operator =(const myAny&) = delete;

	myAny(myAny&& any) = default;
	//myAny& operator =(myAny&& any) = default;
	//myAny(myAny&& any) 
	//{
	//	obj = std::move(any.obj);
	//};
	myAny& operator =(myAny&& any) 
	{
		obj = std::move(any.obj);
		return *this;
	};
	//��ע�⣬�����ҷ���һ�����ԵĴ��󣬶�����ֵ������˵���ǲ�����const�����εģ���Ϊ������� ��C/C++ ����ǿ���͵ľ�̬�������ԣ�����ʱ�ھͻ��ϸ������͡�����ֵ ������ͣ��ڱ�����������Ҫô�Ǹ�����ֵ��Ҫô�Ǹ� �������� ���� ���������Ķ���
	// �����������ר���ṩ�� �ƶ����壬�ƶ����屾������ static_cast������ת�� ���﷨�ǣ�����������ȡ��    ������ֵ һ����Ҫ������Դת�Ƶ�ʱ�� �õ��ģ��Ǽ�ȻҪ������Դת�ƣ��Ͳ��ɱ���Ļ� ��ʵ�ε�ֵ�����޸ģ��Ǳ�Ȼ������const�ˡ�
	//myAny(const myAny&& any)
	//{
	//	obj = std::move(any.obj);
	//};

	template<typename T>
	myAny(T data) : obj(new Inherit<T>(data)) {}

	// Result rst= run();     int result = rst.cast<int>();
	template<typename T>
	T& cast() 
	{
		//��ô��obj ����ָ�����ҵ���������ָ��Ķ���ȡ�������أ�       ȡ������ָ��ײ����� ��ָ�룬   ��ʹ�� ֧��RTTI ǿ������ת���� dynamic_cast������
		Inherit<T>* ptr = dynamic_cast<Inherit<T>*>(obj.get()); 
		if (ptr == nullptr)
		{
			throw "type is not compatible";
		}

		return ptr->get();
	}
private:
	//MyObject* obj;

	std::unique_ptr<MyObject> obj;
};