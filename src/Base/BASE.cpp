/*
 * BASE.cpp
 *
 *  Created on: Sep 15, 2016
 *      Author: root
 */

#include "BASE.h"
#include "../Script/Kiss.h"
#include "../UI/Window.h"

namespace kai
{

BASE::BASE()
{
	m_pKiss = NULL;
	m_pWindow = NULL;
	m_bLog = false;
	m_bDraw = true;

	m_cliMsg = "";
	m_cliMsgLevel = -1;
}

BASE::~BASE()
{
}

bool BASE::init(void* pKiss)
{
	NULL_F(pKiss);
	Kiss* pK = (Kiss*)pKiss;

	string name="";
	F_ERROR_F(pK->v("name",&name));
	IF_F(name.empty());

	bool bLog = false;
	pK->root()->o("APP")->v("bLog",&bLog);
	if(bLog)
	{
		pK->v("bLog",&m_bLog);
	}

	pK->v("bDraw",&m_bDraw);

	m_pKiss = pKiss;
	return true;
}

bool BASE::link(void)
{
	NULL_F(m_pKiss);
	Kiss* pK = (Kiss*)m_pKiss;

	string iName = "";
	F_INFO(pK->v("Window",&iName));
	m_pWindow = (Window*)(pK->root()->getChildInstByName(&iName));

	return true;
}

string* BASE::getName(void)
{
	NULL_N(m_pKiss);
	return &((Kiss*)m_pKiss)->m_name;
}

string* BASE::getClass(void)
{
	NULL_N(m_pKiss);
	return &((Kiss*)m_pKiss)->m_class;
}

bool BASE::start(void)
{
	return true;
}

int BASE::serialize(uint8_t* pB, int nB)
{
	return 0;
}

int BASE::deSerialize(uint8_t* pB, int nB)
{
	return 0;
}

bool BASE::draw(void)
{
	IF_F(!m_bDraw);
	NULL_F(m_pWindow);

	Window* pWin = (Window*)m_pWindow;
	NULL_F(pWin->getFrame());

	Mat* pMat = pWin->getFrame()->m();
	IF_F(pMat->empty());

	return true;
}

bool BASE::cli(int& iY)
{
	COL_NAME;
    mvaddstr(iY, CLI_X_NAME, this->getName()->c_str());

    if(m_cliMsgLevel > 0)
    	COL_ERROR;
    else
    	COL_MSG;

    mvaddstr(iY, CLI_X_MSG, m_cliMsg.c_str());

    return true;
}

}
