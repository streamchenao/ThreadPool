#include <iostream>

using namespace std;




class MyObject
{
public:
	virtual ~MyObject() {}
};

/*
所有继承至 MyObject类的 派生类，

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
	//请注意，这里我犯了一个明显的错误，对于右值引用来说，是不能用const来修饰的，因为本身对于 像C/C++ 这类强类型的静态类型语言，编译时期就会严格检查类型。而右值 这个类型，在编译器看来，要么是个字面值，要么是个 匿名对象 或者 即将析构的对象。
	// 对于这类对象，专门提供了 移动语义，移动语义本质上是 static_cast的类型转换 的语法糖，用了类型萃取。    所以右值 一定是要进行资源转移的时候 用到的，那既然要进行资源转移，就不可避免的会 对实参的值进行修改，那必然不能是const了。
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
		//怎么从obj 智能指针里找到他所真正指向的对象，取出数据呢？       取出智能指针底层管理的 裸指针，   再使用 支持RTTI 强制类型转换的 dynamic_cast方法。
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