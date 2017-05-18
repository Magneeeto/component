#ifndef CLOUDROOM_LOGIC_EASYTHREADPOOL_H
#define CLOUDROOM_LOGIC_EASYTHREADPOOL_H

#include <queue>
#include "Common/AutoLock.h"
#include "TransStruct.h"

//�����̳߳�˼��+����������� �ɸ��ݵ�ǰ����״̬��̬���������߳���
//��Ŀ��Ա��������ʼ�߳��� ����߳��� ���ٿ����߳��� �������߳���
//ԭ���Ͻӵ�����ӿ����̶߳���ȡ�߳�ִ�У�ȡ�˺��ж�����߳��������ٿ����߳��������Ƿ񴴽������߳�
//�߳�ִ�дӿ����л�����æ��ִ�����������������ȡ����ִ�У���������п����ٷ��ÿ��ж���
//���õ����ж��к��жϿ��ж��������ѡ��ɵ������߳�

///////////////////////////////////////////////////////////////////////////////////////////////
//���ȶ��в����������������Ϊ�����������ȡ���Ƴ�����������һ�°�
template <typename _Ty>
class TravelablePQ : public std::priority_queue<_Ty>
{
public:
	bool removeElem(const _Ty& elem)
	{
		bool bRet = false;
		auto it = find_if(c.begin(), c.end(), [&](const _Ty& curElem)->bool
		{return elem == curElem;});
		if (it != c.end())
		{
			bRet = true;
			c.erase(it);
			push_heap(c.begin(), c.end(), comp);
		}

		return bRet;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////

//�������� ���ܵ���չ �����Ÿ��ṹ
struct stMTPQMgrStartUpPara
{
	unsigned int nMaxThrdNum;//����߳��� busy + idle
	unsigned int nMinIdleThrdNum;//���ٱ��������߳��� 
	unsigned int nMaxIdleThrdNum;//��ౣ���Ŀ����߳���

	bool bQuitImmd;//�˳���ʽ �ɵ��߳�|�ȴ�ִ�������˳�

	stMTPQMgrStartUpPara()
	{
		nMaxThrdNum = 5;
		nMinIdleThrdNum = 1;
		nMaxIdleThrdNum = 4;

		bQuitImmd = false;
	}
};

class CEasyThreadPoolMgr
{
	class CWorkThread;
	friend class CWorkThread;
public:
	CEasyThreadPoolMgr(int nInitThrdNum = 2, stMTPQMgrStartUpPara* pPara = NULL);
	~CEasyThreadPoolMgr();

public:
	void ExecJob(stJob* job);
	void TerminateJob(LPCSTR szJobKey);

	void ClearJobs();
	
	bool IsJobsDone()
	{
		CAutoLock _lock(&m_csBusyThreads);
		return m_listBusyThreads.size() == 0;
	}
private:
	stJob* GetJob();
	CWorkThread* CreatWorkThread();

	bool MoveBusyToIdle(CWorkThread* pThread);//�������������ɵ�һЩ suspend
	bool MoveIdleToBusy(CWorkThread* pThread, stJob* pJob);//resume
	
	// ���������У���ת��������ɾ����CPU����Ӱ����Խ��ܣ�����������ʱ���������ıȽϲ���
	// û�п��Ǻ�ɾ�����и�����Ŀǰ����������ɾһ�룬�д���������ת����Ҳ�д�����������
	void DeleteIdleThreads(/*int nDelNum = 1, */int nSpinTime = 500);
	
private:
	class CWorkThread
	{
	public:
		CWorkThread(const char* cstrThreadName);
		~CWorkThread();

	public:
		//mgr��thread��idle��busy�еĵ��� 
		//�Լ���busy�в��ã�ͨ��mgr��ȡ�Ŷ�������
		void SetJob(stJob* pJob)
		{
			//����Ҫ���������˴ӿ��е���æmgr����ʱ�Ż�������߳�setJob
			//��ʱ�߳��������Լ��������job�����඼��ֻ���߳��Լ��Ų���job
			//CAutoLock _lock(&m_csJob);
			m_pJob = pJob;

			m_bIsSuspend = false;
			if (ResumeThread(m_hThread) == -1)
			{
				RetrieveErrCall(_T("EasyThrdPool��SetJob"));
			}
		}

		const stJob* GetJob() const
		{
			return m_pJob;
		}

		HANDLE GetThreadHandle()
		{
			return m_hThread;
		}

		unsigned int GetThreadID()
		{
			return m_nThreadID;
		}

		void RegisteMgr(CEasyThreadPoolMgr* pMgr)
		{
			m_pThreadsMgr = pMgr;
		}

		//���������߳� ���ڽ���ǰ�ͷŶ���
		//��û��ô��ʱ�����ٵ���ɵ�ǰ���񡣡���
		static void DeleteThread(CWorkThread* pThread)
		{
			if (pThread == NULL)
			{
				return;
			}

			pThread->m_bIsRun = false;
			if (pThread->m_bIsSuspend)
			{
				ResumeThread(pThread->m_hThread);
			}
		}
		
		void SetThrdSusp(bool bIsSusp = true)
		{
			m_bIsSuspend = bIsSusp;
		}

		bool IsRun()
		{
			return m_bIsRun;
		}
	
	private:
		static unsigned int __stdcall Run(void*);

	private:
		CEasyThreadPoolMgr* m_pThreadsMgr;
		HANDLE m_hThread;
		unsigned int  m_nThreadID;
		
		CCriticalSection m_csJob;
		stJob* m_pJob;

		bool m_bIsSuspend;//ͨ��mgr����������ͬ��
		bool m_bIsRun;//ͨ������run �����߳� ����ͬ��
	};

private:
	unsigned int m_nInitThrdNum;//��ʼ�߳��� 
	unsigned int m_nMaxThrdNum;//����߳��� busy + idle
	unsigned int m_nMinIdleThrdNum;//���ٱ��������߳��� 
	unsigned int m_nMaxIdleThrdNum;//��ౣ���Ŀ����߳��� 
	
	std::list<CWorkThread*> m_listBusyThreads;
	CCriticalSection m_csBusyThreads;
	std::list<CWorkThread*> m_listIdleThreads;
	CCriticalSection m_csIdleThreads;

	TravelablePQ<stJob*> m_TaskQueue;//job���ڴ�����ڸ�workThread����
	CCriticalSection m_csTaskQueue;

	int m_nCreatedThreadNum;

	bool m_bQuitImmd;//�˳���ʽ
	std::list<HANDLE> m_listQuitBusyThrd;//�ȴ������˳��߳̾��
};

#endif //CLOUDROOM_LOGIC_EASYTHREADPOOL_H