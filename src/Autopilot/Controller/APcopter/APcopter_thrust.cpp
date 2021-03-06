#include "APcopter_thrust.h"

namespace kai
{

APcopter_thrust::APcopter_thrust()
{
	m_pAP = NULL;
	m_pSB = NULL;
	m_pCmd = NULL;
	m_pMavAP = NULL;
	m_pMavGCS = NULL;

	m_pTarget = -1;
	m_targetMin = 0;
	m_targetMax = DBL_MAX;
	m_dCollision = 2.0;
	m_pwmLow = 1000;
	m_pwmMid = 1500;
	m_pwmHigh = 2000;
	m_rcTimeOut = USEC_1SEC;
	m_rcDeadband = 100;
	m_enableAPmode = 2;

	m_pRoll = NULL;
	m_pPitch = NULL;
	m_pYaw = NULL;
	m_pAlt = NULL;

	m_tF.m_iChan = 9;
	m_tB.m_iChan = 10;
	m_tL.m_iChan = 11;
	m_tR.m_iChan = 12;

	m_tF.m_sign = 1;
	m_tB.m_sign = -1;
	m_tL.m_sign = 1;
	m_tR.m_sign = -1;
	resetAllPwm();
}

APcopter_thrust::~APcopter_thrust()
{
}

bool APcopter_thrust::init(void* pKiss)
{
	IF_F(!this->ActionBase::init(pKiss));
	Kiss* pK = (Kiss*) pKiss;
	pK->m_pInst = this;

	KISSm(pK,rcTimeOut);
	KISSm(pK,rcDeadband);
	KISSm(pK,enableAPmode);

	KISSm(pK,pwmLow);
	KISSm(pK,pwmMid);
	KISSm(pK,pwmHigh);

	pK->v("targetX", &m_pTarget.x);
	pK->v("targetY", &m_pTarget.y);
	pK->v("targetZ", &m_pTarget.z);

	KISSm(pK,targetMin);
	KISSm(pK,targetMax);
	KISSm(pK,dCollision);

	pK->v("chanF", &m_tF.m_iChan);
	pK->v("chanB", &m_tB.m_iChan);
	pK->v("chanL", &m_tL.m_iChan);
	pK->v("chanR", &m_tR.m_iChan);

	return true;
}

bool APcopter_thrust::link(void)
{
	IF_F(!this->ActionBase::link());
	Kiss* pK = (Kiss*) m_pKiss;
	string iName;

	iName = "";
	pK->v("APcopter_base", &iName);
	m_pAP = (APcopter_base*) (pK->parent()->getChildInstByName(&iName));
	IF_Fl(!m_pAP, iName + ": not found");

	iName = "";
	pK->v("_SlamBase", &iName);
	m_pSB = (_SlamBase*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pSB, iName + ": not found");

	iName = "";
	pK->v("_WebSocket", &iName);
	m_pCmd = (_WebSocket*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pCmd, iName + ": not found");

	iName = "";
	pK->v("_Mavlink_GCS", &iName);
	m_pMavGCS = (_Mavlink*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pMavGCS, iName + ": not found");

	iName = "";
	pK->v("PIDroll", &iName);
	m_pRoll = (PIDctrl*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pRoll, iName + ": not found");

	iName = "";
	pK->v("PIDpitch", &iName);
	m_pPitch = (PIDctrl*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pPitch, iName + ": not found");

	iName = "";
	pK->v("PIDyaw", &iName);
	m_pYaw = (PIDctrl*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pYaw, iName + ": not found");

	iName = "";
	pK->v("PIDalt", &iName);
	m_pAlt = (PIDctrl*) (pK->root()->getChildInstByName(&iName));
	IF_Fl(!m_pAlt, iName + ": not found");

	return true;
}

void APcopter_thrust::update(void)
{
	this->ActionBase::update();

	NULL_(m_pAP);
	m_pMavAP = m_pAP->m_pMavlink;
	NULL_(m_pMavAP);
	NULL_(m_pMavGCS);
	NULL_(m_pSB);
	NULL_(m_pRoll);
	NULL_(m_pPitch);
	NULL_(m_pYaw);
	NULL_(m_pAlt);

	cmd();
	vDouble3* pPos = &m_pSB->m_pos;
	int o;

	resetAllPwm();

	//Pitch = Y axis
	if(pPos->y > 0 && m_pTarget.y > m_targetMin)
	{
		o = (int)m_pPitch->update(pPos->y, m_pTarget.y);
		m_tF.m_pwm = constrain(m_pwmLow + o*m_tF.m_sign, m_pwmLow, m_pwmHigh);
		m_tB.m_pwm = constrain(m_pwmLow + o*m_tB.m_sign, m_pwmLow, m_pwmHigh);
	}

	//Roll = X axis
	if(pPos->x > 0 && m_pTarget.x > m_targetMin)
	{
		o = (int)m_pRoll->update(pPos->x, m_pTarget.x);
		m_tL.m_pwm = constrain(m_pwmLow + o*m_tL.m_sign, m_pwmLow, m_pwmHigh);
		m_tR.m_pwm = constrain(m_pwmLow + o*m_tR.m_sign, m_pwmLow, m_pwmHigh);
	}

	//Alt = Z axis
	if(pPos->z > 0 && m_pTarget.z > m_targetMin)
	{
		m_pwmAlt += (int)m_pAlt->update(pPos->z, m_pTarget.z);
	}

	if(!isActive())
	{
		resetAllPwm();
		LOG_F("OFF: Disabled");
	}
	else if(m_pMavAP->m_msg.heartbeat.custom_mode != m_enableAPmode)
	{
		resetAllPwm();
		LOG_F("OFF: Flight mode = " +
				i2str((int)m_pMavAP->m_msg.heartbeat.custom_mode) +
				" != " + i2str((int)m_enableAPmode));
	}
	else
	{
		LOG_I("ON");
	}

	m_pMavAP->clDoSetServo(m_tF.m_iChan, m_tF.m_pwm);
	m_pMavAP->clDoSetServo(m_tB.m_iChan, m_tB.m_pwm);
	m_pMavAP->clDoSetServo(m_tL.m_iChan, m_tL.m_pwm);
	m_pMavAP->clDoSetServo(m_tR.m_iChan, m_tR.m_pwm);

	IF_(*m_pAM->getCurrentStateName() != "CC_ON_ALT");
	IF_(m_pMavAP->m_msg.heartbeat.custom_mode != m_enableAPmode);

	mavlink_rc_channels_override_t rc;
	rc.chan1_raw = UINT16_MAX;
	rc.chan2_raw = UINT16_MAX;
	rc.chan3_raw = (uint16_t)m_pwmAlt;
	rc.chan4_raw = UINT16_MAX;
	rc.chan5_raw = UINT16_MAX;
	rc.chan6_raw = UINT16_MAX;
	rc.chan7_raw = UINT16_MAX;
	rc.chan8_raw = UINT16_MAX;
	m_pMavAP->rcChannelsOverride(rc);
	LOG_I("ON + ALT");

}

void APcopter_thrust::resetAllPwm(void)
{
	m_tF.m_pwm = m_pwmLow;
	m_tB.m_pwm = m_pwmLow;
	m_tL.m_pwm = m_pwmLow;
	m_tR.m_pwm = m_pwmLow;
	m_pwmAlt = m_pwmMid;
}

void APcopter_thrust::cmd(void)
{
	NULL_(m_pCmd);

	char buf[N_IO_BUF];
	int nB = m_pCmd->read((uint8_t*) &buf, N_IO_BUF);
	IF_(nB <= 0);
	buf[nB]=0;
	string str = buf;
	string cmd;
	double v;

	std::string::size_type iDiv = str.find(' ');
	if(iDiv == std::string::npos)
	{
		cmd = str;
		v = -1.0;
	}
	else
	{
		cmd = str.substr(0, iDiv);
		IF_(++iDiv >= str.length());
		v = atof(str.substr(iDiv, str.length()-iDiv).c_str());
	}

	if(cmd=="set")
	{
		m_pTarget = m_pSB->m_pos;
		if(m_pTarget.x < m_targetMin || m_pTarget.x > m_targetMax)m_pTarget.x = -1.0;
		if(m_pTarget.y < m_targetMin || m_pTarget.y > m_targetMax)m_pTarget.y = -1.0;
		if(m_pTarget.z < m_targetMin || m_pTarget.z > m_targetMax)m_pTarget.z = -1.0;

		str = "SET: X=" + f2str(m_pTarget.x) +
				", Y=" + f2str(m_pTarget.y) +
				", Z=" + f2str(m_pTarget.z);

		string strOn = "CC_ON";
		m_pAM->transit(&strOn);
	}
	else if(cmd=="set_alt")
	{
		m_pTarget = m_pSB->m_pos;
		if(m_pTarget.x < m_targetMin || m_pTarget.x > m_targetMax)m_pTarget.x = -1.0;
		if(m_pTarget.y < m_targetMin || m_pTarget.y > m_targetMax)m_pTarget.y = -1.0;
		if(m_pTarget.z < m_targetMin || m_pTarget.z > m_targetMax)m_pTarget.z = -1.0;

		str = "SET_ALT: X=" + f2str(m_pTarget.x) +
				", Y=" + f2str(m_pTarget.y) +
				", Z=" + f2str(m_pTarget.z);

		string strOn = "CC_ON_ALT";
		m_pAM->transit(&strOn);
	}
	else if(cmd=="off")
	{
		string strOff = "CC_STANDBY";
		m_pAM->transit(&strOff);
		str = "OFF";
	}
	else if(cmd=="x")
	{
		if(v < m_targetMin || v > m_targetMax)v = -1.0;

		m_pTarget.x = v;
		str = "SET: X=" + f2str(m_pTarget.x) +
				", Y=" + f2str(m_pTarget.y) +
				", Z=" + f2str(m_pTarget.z);
	}
	else if(cmd=="y")
	{
		if(v < m_targetMin || v > m_targetMax)v = -1.0;

		m_pTarget.y = v;
		str = "SET: X=" + f2str(m_pTarget.x) +
				", Y=" + f2str(m_pTarget.y) +
				", Z=" + f2str(m_pTarget.z);
	}
	else if(cmd=="z")
	{
		if(v < m_targetMin || v > m_targetMax)v = -1.0;

		m_pTarget.z = v;
		str = "SET: X=" + f2str(m_pTarget.x) +
				", Y=" + f2str(m_pTarget.y) +
				", Z=" + f2str(m_pTarget.z);
	}
	else
	{
		str = "Invalid Cmd: " + cmd;
	}

	m_pCmd->write((uint8_t*)str.c_str(), str.length(), WS_MODE_TXT);
}

bool APcopter_thrust::draw(void)
{
	IF_F(!this->ActionBase::draw());
	Window* pWin = (Window*) this->m_pWindow;
	Mat* pMat = pWin->getFrame()->m();

	return true;
}

bool APcopter_thrust::cli(int& iY)
{
	IF_F(!this->ActionBase::cli(iY));

	NULL_F(m_pAP->m_pMavlink);
	NULL_F(m_pSB);
	vDouble3* pPos = &m_pSB->m_pos;

	string msg;

	//Roll = X axis
	msg = "targetX=" + f2str(m_pTarget.x) +
		  " | X=" + f2str(pPos->x) +
		  " | pwmL=" + i2str(m_tL.m_pwm) +
		  " | pwmR=" + i2str(m_tR.m_pwm);
	COL_MSG;
	iY++;
	mvaddstr(iY, CLI_X_MSG, msg.c_str());

	//Pitch = Y axis
	msg = "targetY=" + f2str(m_pTarget.y) +
		  " | Y=" + f2str(pPos->y) +
		  " | pwmF=" + i2str(m_tF.m_pwm)  +
		  " | pwmB=" + i2str(m_tB.m_pwm);
	COL_MSG;
	iY++;
	mvaddstr(iY, CLI_X_MSG, msg.c_str());

	//Alt = Z axis
	msg = "targetZ=" + f2str(m_pTarget.z) +
		  " | Z=" + f2str(pPos->z) +
		  " | pwm=" + i2str(m_pwmAlt);
	COL_MSG;
	iY++;
	mvaddstr(iY, CLI_X_MSG, msg.c_str());

	return true;
}

}
