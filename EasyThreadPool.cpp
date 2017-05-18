#include "stdafx.h"
#include "EasyThreadPool.h"
#include "TransStruct.h"

#define DEL_THRD(pThrd) CEasyThreadPoolMgr::CWorkThread::DeleteThread(pThrd)

//ͷ�ļ�ǰ������ ֻ����ָ��װװ���� �漰����Ա���á�����̳л�������
stJob::~stJob()
{	
	if(data)
		delete data;
}

CEasyThreadPoolMgr::CWorkThread::CWorkThread(const char* cstrThreadName)
{
	//SECURITY_ATTRIBUTES sa;
	//SECURITY_DESCRIPTOR sd;

	//sa.nLength = sizeof(sa);
	//sa.lpSecurityDescriptor = &sd;
	//sa.bInheritHandle = FALSE;

	//InitializeSecurityDescriptor(&sd, THREAD_SUSPEND_RESUME);
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, Run, this, CREATE_SUSPENDED, &m_nThreadID);
	//DWORD d = SuspendThread(m_hThread);
	::SetThreadName(m_nThreadID, cstrThreadName);

	m_pThreadsMgr = NULL;
	m_pJob = NULL;
	m_bIsRun = true;
	m_bIsSuspend = true;
}

CEasyThreadPoolMgr::CWorkThread::~CWorkThread()
{
	//::PostThreadMessage(m_nThreadID, WM_QUIT, 0, 0);
	//ExitThread(m_nThreadID);
	CloseHandle(m_hThread);

	if (m_pJob != NULL)
	{
		delete m_pJob;
	}
}

//�Լ��������Լ� ��������п�ʱ ��manager������
unsigned int CEasyThreadPoolMgr::CWorkThread::Run(void* data)
{
	CWorkThread* pThis = static_cast<CWorkThread*>(data);
	ASSERT(pThis);
	if (pThis != NULL)
	{
		while(pThis->m_bIsRun)
		{
			//����Ҫ���������˴ӿ��е���æmgr����ʱ�Ż�������߳�setJob
			//��ʱ�߳��������Լ��������job�����඼��ֻ���߳��Լ��Ų���job
			{
				//CAutoLock _lock(&m_csJob);
				if (pThis->m_pJob != NULL)
				{
					pThis->m_pJob->callback(pThis->m_pJob->data);//����ҵ��ʵ�ֺ�������Ҫ�����ڴ�

					delete pThis->m_pJob;
					pThis->m_pJob = NULL;
				}
			}

			if (pThis->m_pJob == NULL && pThis->m_bIsRun)
			{
				pThis->m_pJob = pThis->m_pThreadsMgr->GetJob();
				if (pThis->m_pJob == NULL && pThis->m_bIsRun)
				{
					pThis->m_pThreadsMgr->MoveBusyToIdle(pThis);
				}
			}
		}

		delete pThis;
	}

	return 0;
}

CEasyThreadPoolMgr::CEasyThreadPoolMgr(int nInitThrdNum/* = 2*/, stMTPQMgrStartUpPara* pPara /*= NULL*/)
{
	m_nInitThrdNum = nInitThrdNum;
	m_nMaxThrdNum = pPara == NULL ? 5 : pPara->nMaxThrdNum;
	m_nMinIdleThrdNum = pPara == NULL ? 1 : pPara->nMinIdleThrdNum;
	m_nMaxIdleThrdNum = pPara == NULL ? 4 : pPara->nMaxIdleThrdNum;
	m_bQuitImmd = pPara == NULL ? false : pPara->bQuitImmd;

	m_nCreatedThreadNum = 0;
	
	while(nInitThrdNum--)
	{	
		CWorkThread* pWorkThread = CreatWorkThread();
		m_listIdleThreads.push_back(pWorkThread);
	}
}

CEasyThreadPoolMgr::~CEasyThreadPoolMgr()
{
	LD("enter");
	ClearJobs();
	for (auto it = m_listQuitBusyThrd.begin(); it != m_listQuitBusyThrd.end(); it++)
	{
		LD("begin wait");
		WaitForSingleObject(*it, 1000);
		LD("end wait");
	}
	LD("exit");
}

void CEasyThreadPoolMgr::ClearJobs()
{
	{
		auto TERM_THRD = [](CWorkThread* pThread)
		{
			::TerminateThread(pThread->GetThreadHandle(), 0);
			delete pThread;
		};

		CAutoLock _lock(&m_csBusyThreads);
		for (auto it = m_listBusyThreads.begin(); it != m_listBusyThreads.end(); it++)
		{
			m_bQuitImmd ? TERM_THRD(*it) : DEL_THRD(*it);
			m_bQuitImmd ? void() : m_listQuitBusyThrd.push_back((*it)->GetThreadHandle());
		}

		m_listBusyThreads.clear();
	}

	{
		CAutoLock _lock(&m_csIdleThreads);
		for (auto it = m_listIdleThreads.begin(); it != m_listIdleThreads.end(); it++)
		{
			DEL_THRD(*it);
		}
		
		m_listIdleThreads.clear();
	}
	
	CAutoLock _lock(&m_csTaskQueue);
	while(!m_TaskQueue.empty())
	{
		auto it = m_TaskQueue.top();
		m_TaskQueue.pop();
		delete it;
	}		
}

CEasyThreadPoolMgr::CWorkThread* CEasyThreadPoolMgr::CreatWorkThread()
{
	std::string strThreadName = "WorkThread No.";
	strThreadName += CI2A(m_nCreatedThreadNum++);
	CWorkThread* pWorkThread = new CWorkThread(strThreadName.c_str());
	pWorkThread->RegisteMgr(this);

	return pWorkThread;
}

bool CEasyThreadPoolMgr::MoveBusyToIdle(CWorkThread* pThread)
{
	if (pThread == NULL)
	{
		return false;
	}

	//����ֻ�е�ǰ�߳��Լ����ߵ� �����ȹ���
	bool bRet = true;
	{
		CAutoLock _lock(&m_csBusyThreads);
		auto it = find(m_listBusyThreads.begin(), m_listBusyThreads.end(), pThread);
		ASSERT(it != m_listBusyThreads.end());
		if (it != m_listBusyThreads.end())
		{
			m_listBusyThreads.erase(it);
		}
		else
		{
			bRet = false;
			DEL_THRD(pThread);
			return bRet;
		}
	}

	//�Ƿ���������� ���������߳���
	DeleteIdleThreads();

	{
		CAutoLock _lock(&m_csIdleThreads);
		m_listIdleThreads.push_back(pThread);
	}

	//�����Լ�
	if (pThread->IsRun())
	{
		pThread->SetThrdSusp();

		if (SuspendThread(pThread->GetThreadHandle()) == -1)
		{
			RetrieveErrCall(_T("EasyThrdPool��MoveBusyToIdel"));
		}
	}
	
	return bRet;
}

bool CEasyThreadPoolMgr::MoveIdleToBusy(CWorkThread* pThread, stJob* pJob)
{
	if (pThread == NULL || pJob == NULL)
	{
		assert(false);
		return false;
	}

	bool bRet = true;
	{
		CAutoLock _lock(&m_csIdleThreads);
		auto it = find(m_listIdleThreads.begin(), m_listIdleThreads.end(), pThread);
		ASSERT(it != m_listIdleThreads.end());
		if (it != m_listIdleThreads.end())
		{
			m_listIdleThreads.erase(it);
			//�Ƿ�С����С���� ������
			if (m_listIdleThreads.size() < m_nMinIdleThrdNum
				&& m_listIdleThreads.size() + m_listBusyThreads.size() < m_nMaxThrdNum)
			{
				CWorkThread* pIncreamentThrd = CreatWorkThread();
				m_listIdleThreads.push_back(pIncreamentThrd);
			}
		}
		else
		{
			ASSERT(false);
			bRet = false;
		}
	}

	if (bRet)
	{
		CAutoLock _lock(&m_csBusyThreads);
		m_listBusyThreads.push_back(pThread);
		pThread->SetJob(pJob);
	}
	
	return bRet;
}

void CEasyThreadPoolMgr::DeleteIdleThreads(int nSpinTime /* = 500 */)
{
	if (m_listIdleThreads.size() > m_nMaxIdleThrdNum)
	{
		while(nSpinTime--)
		{

		}

		//����ȡsize֮ǰ����
		CAutoLock _lock(&m_csIdleThreads);
		if (m_listIdleThreads.size() > m_nMaxIdleThrdNum)
		{
			//���滹һ�����Ž�list
			int nCount = (m_listIdleThreads.size() + 1) / 2;
			while(nCount-- > 0)
			{
				auto it = m_listIdleThreads.begin();
				ASSERT(it != m_listIdleThreads.end());
				if (it != m_listIdleThreads.end())
				{
					//���������һ����suspend��
					DEL_THRD(*it);
					m_listIdleThreads.erase(it);
				}
			}
		}
	}
}

stJob* CEasyThreadPoolMgr::GetJob()
{
	stJob* pRet = NULL;

	CAutoLock _lock(&m_csTaskQueue);
	if (!m_TaskQueue.empty())
	{
		pRet = m_TaskQueue.top();
		m_TaskQueue.pop();
	}

	return pRet;
}

void CEasyThreadPoolMgr::ExecJob(stJob* pJob)
{
	if (pJob == NULL)
	{
		return;
	}

	//ִ�������ȳ��Դӿ����л�ȡ
	if (!m_listIdleThreads.empty())
	{
		CWorkThread* pCurIdleThread = m_listIdleThreads.front();
		ASSERT(pCurIdleThread != NULL);
		if(!MoveIdleToBusy(pCurIdleThread, pJob))
		{
			//fall��back �����ϲ���Զ�������������
			ASSERT(false);
			CAutoLock _lock(&m_csTaskQueue);
			m_TaskQueue.push(pJob);
		}	
	}
	else 
	{
		//������û�� ���Դ���������һ������С���������ǳ����������������Ǹ�����
		//æ���߳����ڴ�����δ������У�

		//�������ѳ�������߳�������ȥ�Ŷ�
		if (m_listBusyThreads.size() + m_listIdleThreads.size() >= m_nMaxThrdNum)
		{
			CAutoLock _lock(&m_csTaskQueue);
			m_TaskQueue.push(pJob);
		}
		else
		{
			CAutoLock _lock(&m_csBusyThreads);
			CWorkThread* pIncreamentThrd = CreatWorkThread();
			m_listBusyThreads.push_back(pIncreamentThrd);
			pIncreamentThrd->SetJob(pJob);
		}
	}
}

void CEasyThreadPoolMgr::TerminateJob(LPCSTR szJobKey)
{
	if (szJobKey == "")
	{
		return;
	}
	
	//�Ŷӵĸɵ�
	{
		CAutoLock _lock(&m_csTaskQueue);
		stJob tempJob(NULL, NULL, szJobKey);
		if (m_TaskQueue.removeElem(&tempJob))
		{
			return;
		}
	}

	//����ִ�е� �ɵ��߳�
	{
		CAutoLock _lock(&m_csBusyThreads);
		auto it = find_if(m_listBusyThreads.begin(), m_listBusyThreads.end(),
		[&](const CWorkThread* pThread)->bool
		{
			bool bRet = false;
			const stJob* pJob = pThread->GetJob();
			if (pJob != NULL)
			{
				bRet = pJob->strJobKey == szJobKey;
			}

			return bRet;
		});

		if (it != m_listBusyThreads.end())
		{
			CWorkThread* pThread = *it;
			::TerminateThread(pThread->GetThreadHandle(), 0);
			delete pThread;
			m_listBusyThreads.erase(it);
		}
	}
}