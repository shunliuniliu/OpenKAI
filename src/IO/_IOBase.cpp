/*
 * _IOBase.cpp
 *
 *  Created on: June 16, 2016
 *      Author: yankai
 */

#include "_IOBase.h"

namespace kai
{

_IOBase::_IOBase()
{
	m_rThreadID = 0;
	m_bRThreadON = false;
	m_ioType = io_none;
	m_ioStatus = io_unknown;
	m_bStream = true;
	m_pCmdW = NULL;
	m_nCmdW = 0;
	m_nCmdType = 256;
	m_iCmdID = 5; //Default: Message ID in Mavlink v1.0
	m_iCmdW = 0;
	m_iCmdSeq = 2;

	pthread_mutex_init(&m_mutexW, NULL);
	pthread_mutex_init(&m_mutexR, NULL);
}

_IOBase::~_IOBase()
{
	DEL(m_pCmdW);
	pthread_mutex_destroy(&m_mutexW);
	pthread_mutex_destroy(&m_mutexR);
}

bool _IOBase::init(void* pKiss)
{
	IF_F(!this->_ThreadBase::init(pKiss));
	Kiss* pK = (Kiss*) pKiss;
	pK->m_pInst = this;

	KISSm(pK,bStream);
	IF_T(m_bStream);

	// for stream mode
	KISSm(pK, iCmdID);
	IF_Fl(m_iCmdID < 0, "iCmdID < 0");

	KISSm(pK, nCmdType);
	IF_Fl(m_nCmdType <= 0, "nCmdType <= 0");

	m_pCmdW = new IO_BUF[m_nCmdType];
	for(int i=0; i<m_nCmdType; i++)
	{
		m_pCmdW[i].init();
	}

	m_iCmdW = 0;
	m_nCmdW = 0;

	return true;
}

bool _IOBase::open(void)
{
	return false;
}

bool _IOBase::isOpen(void)
{
	return (m_ioStatus == io_opened);
}

IO_TYPE _IOBase::ioType(void)
{
	return m_ioType;
}

void _IOBase::close(void)
{
	while (!m_queW.empty())
		m_queW.pop();

	while (!m_queR.empty())
		m_queR.pop();

	m_ioStatus = io_closed;
}

bool _IOBase::write(IO_BUF& ioB)
{
	IF_F(m_ioStatus != io_opened);
	IF_F(ioB.bEmpty());

	if(m_bStream)
	{
		pthread_mutex_lock(&m_mutexW);

		m_queW.push(ioB);

		pthread_mutex_unlock(&m_mutexW);
	}
	else
	{
		IF_F(ioB.m_nB <= m_iCmdID);
		uint8_t iCmdID = ioB.m_pB[m_iCmdID];
		IF_F(iCmdID >= m_nCmdType);
		uint8_t iCmdSeq = ioB.m_pB[m_iCmdSeq];

		IO_BUF* pI = &m_pCmdW[iCmdID];
		if(!pI->bEmpty())
		{
			uint8_t iExistingCmdSeq = pI->m_pB[m_iCmdSeq];
			IF_F(iExistingCmdSeq < 250 && iCmdSeq < iExistingCmdSeq);
			//TODO: verify
		}

		pthread_mutex_lock(&m_mutexW);

		*pI = ioB;
		m_nCmdW++;

		pthread_mutex_unlock(&m_mutexW);
	}

	this->wakeUp();
	return true;
}

bool _IOBase::write(uint8_t* pBuf, int nB)
{
	IF_F(m_ioStatus != io_opened);
	IF_F(nB <= 0);
	NULL_F(pBuf);
	IF_Fl(!m_bStream, "cmd mode not supported, use _IOBase::write(IO_BUF&) instead");

	IO_BUF ioB;
	int nW = 0;

	pthread_mutex_lock(&m_mutexW);

	while (nW < nB)
	{
		ioB.m_nB = nB - nW;
		if(ioB.m_nB > N_IO_BUF)
			ioB.m_nB = N_IO_BUF;

		memcpy(ioB.m_pB, &pBuf[nW], ioB.m_nB);
		nW += ioB.m_nB;

		m_queW.push(ioB);
	}

	pthread_mutex_unlock(&m_mutexW);

	this->wakeUp();
	return true;
}

bool _IOBase::writeLine(uint8_t* pBuf, int nB)
{
	const char pCRLF[] = "\x0d\x0a";

	IF_F(!write(pBuf, nB));
	return write((unsigned char*)pCRLF, 2);
}

bool _IOBase::toBufW(IO_BUF* pB)
{
	NULL_F(pB);

	if(m_bStream)
	{
		IF_F(m_queW.empty());

		pthread_mutex_lock(&m_mutexW);
		*pB = m_queW.front();
		m_queW.pop();
		pthread_mutex_unlock(&m_mutexW);
	}
	else
	{
		IF_F(m_nCmdW <= 0);

		IO_BUF* pIOB;
		while((pIOB = &m_pCmdW[m_iCmdW])->bEmpty())
		{
			if(++m_iCmdW >= m_nCmdType)m_iCmdW=0;
		}

		pthread_mutex_lock(&m_mutexW);
		*pB = *pIOB;
		pIOB->init();
		m_nCmdW--;
		pthread_mutex_unlock(&m_mutexW);
	}

	return true;
}

int _IOBase::read(uint8_t* pBuf, int nB)
{
	if(m_ioStatus != io_opened)return -1;
	if(pBuf == NULL)return -1;
	if(nB < N_IO_BUF)return -1;
	if(m_queR.empty())return 0;

	pthread_mutex_lock(&m_mutexR);
	IO_BUF ioB = m_queR.front();
	m_queR.pop();
	pthread_mutex_unlock(&m_mutexR);

	memcpy(pBuf, ioB.m_pB, ioB.m_nB);
	return ioB.m_nB;
}

void _IOBase::toQueR(IO_BUF* pB)
{
	NULL_(pB);
	IF_(pB->bEmpty());

	pthread_mutex_lock(&m_mutexR);
	m_queR.push(*pB);
	pthread_mutex_unlock(&m_mutexR);

	if(m_pWakeUp)
	{
		m_pWakeUp->wakeUp();
	}
}

bool _IOBase::draw(void)
{
	IF_F(!this->_ThreadBase::draw());
	Window* pWin = (Window*)this->m_pWindow;

	return true;
}

}
