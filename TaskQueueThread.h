#pragma once

#include <queue>

#include "AutoLock.h"

typedef int FNCallBack(void *pParam);

typedef struct tagTask
{
	FNCallBack *pFunc;
	void* param;
}stTask, *LPTask;

//�����������࣬�ɼ̳�ʵ�������ȡ��������ͣ������
class CTaskQueueThread
{
public:
	CTaskQueueThread(LPCSTR szThreadName = nullptr);
	CTaskQueueThread(FNCallBack *pFunc, void* param, bool bSingleTask = true, LPCSTR szThreadName = nullptr);
	~CTaskQueueThread(void);

	//����ֵ��ʾ�Ƿ�����ִ�У������������ֻ�е�ǰһ������
	BOOL PushTask(FNCallBack *pFunc, void* param);
	bool ClearTask(bool bExit = true);

	BOOL Start();

	void Terminate();

	HANDLE GetThreadHandle();//������WaitForSingleObject
	
	bool isRunning();
	void Wait(); //�ȴ��߳̽���

protected:

	static unsigned int __stdcall Run(void *param);
	void ExecuteTasks();

	std::queue<stTask> m_queueFnCallBack;
	CCriticalSection m_csQueueFnCallBack;

	HANDLE m_hThread;

	bool m_bSingleTask;
	HANDLE m_hEvent;
	bool m_bExit;
	std::string m_strThreadName;

	static int m_nID;
};
