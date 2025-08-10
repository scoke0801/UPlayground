#pragma once

class FPGHandler
{
public:
	virtual ~FPGHandler() { Finalize();}
	
public:
	virtual void Initialize() {}
	virtual void Finalize() {}

public:
	template<class T>
	static T* Create();
};

template <class Handler>
Handler* FPGHandler::Create()
{
	static_assert(std::is_base_of_v<FPGHandler, Handler>,  "Handler는 FPGHandler하위 클래스여야 합니다.");
	
	Handler* NewHandler = new Handler();
	NewHandler->Initialize();

	return NewHandler;
}
