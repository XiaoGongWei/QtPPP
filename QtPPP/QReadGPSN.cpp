#include "QReadGPSN.h"


QReadGPSN::QReadGPSN(void)
{
}


QReadGPSN::~QReadGPSN(void)
{
}

void QReadGPSN::initVar()
{
	isReadHead = false;
	m_epochDataNum = 7;
	m_leapSec = 0;
	m_BaseYear = 2000;
	IonAlpha[0] = 0;IonAlpha[1] = 0;IonAlpha[2] = 0;IonAlpha[3] = 0;
	IonBeta[0] = 0;IonBeta[1] = 0;IonBeta[2] = 0;IonBeta[3] = 0;
	DeltaA01[0] = 0;DeltaA01[1] = 0;
	DeltaTW[0] = 0;DeltaTW[1] = 0;
}

QReadGPSN::QReadGPSN(QString NFileName)
{
	initVar();
	if (NFileName.trimmed().isEmpty())
		ErroTrace("File Name is Empty!(QReadGPSN::QReadGPSN(QString NFileName))");
	m_NfileName = NFileName;

	//读取文件头部信息并解析
	getHeadInf();//此处并未关闭文件
	//GLONASS只有三行12个
	if (FileIdType == 'R')	m_epochDataNum = 3;
}

//读取导航文件的头文件
void QReadGPSN::getHeadInf()
{
	if (isReadHead)	return ;
	//打开文件
	m_readGPSNFile.setFileName(m_NfileName);
	if (!m_readGPSNFile.open(QFile::ReadOnly))
		ErroTrace("Open file bad!(QReadOFile::QReadOFile(QString OfileName))");
	//读取头文件
	while (!m_readGPSNFile.atEnd())
	{
		tempLine = m_readGPSNFile.readLine();
		if (tempLine.contains("END OF HEADER",Qt::CaseInsensitive))
			break;
		if (tempLine.contains("RINEX VERSION",Qt::CaseInsensitive))
		{

			RinexVersion =  tempLine.mid(0,10).trimmed().toDouble();
			if (tempLine.mid(20,20).contains("GPS",Qt::CaseInsensitive))
				FileIdType = 'G';
			else if (tempLine.mid(20,20).contains("CMP",Qt::CaseInsensitive))
				FileIdType = 'C';
			else if (tempLine.mid(20,20).contains("GLONASS",Qt::CaseInsensitive))
				FileIdType = 'R';
		} 
		else if (tempLine.contains("PGM / RUN BY / DATE",Qt::CaseInsensitive))
		{
			PGM = tempLine.mid(0,20).trimmed();
			RUNBY = tempLine.mid(20,20).trimmed();
			CreatFileDate = tempLine.mid(40,20).trimmed();
			//qDebug()<<CreatFileDate;
		}
		else if (tempLine.contains("COMMENT",Qt::CaseInsensitive))
		{
			CommentInfo+=tempLine.mid(0,60).trimmed() + "\n";
			//qDebug()<<CommentInfo;
		}
		else if (tempLine.contains("ION ALPHA",Qt::CaseInsensitive))
		{
			for(int i = 0;i < 4;i++)
			{
				QString strReplaceTemp = tempLine.mid(2+i*12,12).replace('D','E').trimmed();
				IonAlpha[i] = strReplaceTemp.toDouble();
			}
		}
		else if (tempLine.contains("ION BETA",Qt::CaseInsensitive))
		{
			for(int i = 0;i < 4;i++)
			{
				QString strReplaceTemp = tempLine.mid(2+i*12,12).replace('D','E').trimmed();
				IonBeta[i] = strReplaceTemp.toDouble();
			}
		}
		else if (tempLine.contains("DELTA-UTC",Qt::CaseInsensitive))
		{
			for(int i = 0;i < 2;i++)
			{
				QString strReplaceTemp = tempLine.mid(3+i*19,19).replace('D','E').trimmed();
				DeltaA01[i] = strReplaceTemp.toDouble();
				DeltaTW[i] = tempLine.mid(41+9*i,9).trimmed().toInt();
			}
		}
	}//读取头文件结束
	isReadHead = true;
}

//读取Rinex 2.X广播星历数据
void QReadGPSN::readNFileVer2(QVector< BrdData > &allBrdData)
{
	if (!isReadHead) getHeadInf();
	if (isReadAllData) return ;	
	tempLine = m_readGPSNFile.readLine();
	//进入数据区域读取
	while (!m_readGPSNFile.atEnd())
	{
		BrdData epochBrdData;
		if (!tempLine.mid(0,2).trimmed().isEmpty())
		{
			tempLine.replace('D','E');
			epochBrdData.PRN = tempLine.mid(0,2).toInt();
			epochBrdData.SatType = FileIdType;
			epochBrdData.UTCTime.Year = tempLine.mid(3,2).toInt() + m_BaseYear;//m_BaseYear设置成2000
			epochBrdData.UTCTime.Month = tempLine.mid(6,2).toInt();
			epochBrdData.UTCTime.Day = tempLine.mid(9,2).toInt();
			epochBrdData.UTCTime.Hours = tempLine.mid(12,2).toInt();
			epochBrdData.UTCTime.Minutes = tempLine.mid(15,2).toInt();
			epochBrdData.UTCTime.Seconds = tempLine.mid(17,5).toDouble();
			epochBrdData.TimeDiv = tempLine.mid(22,19).toDouble();
			epochBrdData.TimeMove = tempLine.mid(41,19).toDouble();
			epochBrdData.TimeMoveSpeed = tempLine.mid(60,19).toDouble();
			//读取下一行数据
			tempLine = m_readGPSNFile.readLine();
			tempLine.replace('D','E');
			double tempdb = 0.0;
			for (int i = 0; i < m_epochDataNum;i++)
			{
				for (int j = 0;j < 4;j++)
				{
					tempdb = tempLine.mid(3 + j*19,19).toDouble();
					epochBrdData.epochNData.append(tempdb);
				}
				tempLine = m_readGPSNFile.readLine();
				tempLine.replace('D','E');
			}
			allBrdData.append(epochBrdData);//保存一个数据段
		}
		else
		{
			continue;
		}
	}//while (!m_readGPSNFile.atEnd())读取到文件尾部结束
	isReadAllData = true;
	//计算跳秒
	BrdData fistEpoch = allBrdData.at(0);
	m_leapSec = getLeapSecond(fistEpoch.UTCTime.Year,fistEpoch.UTCTime.Month,fistEpoch.UTCTime.Day,
		fistEpoch.UTCTime.Hours,fistEpoch.UTCTime.Minutes,fistEpoch.UTCTime.Seconds);
}

//读取所有广播星历数据到allBrdData
QVector< BrdData > QReadGPSN::getAllData()
{
	if (isReadAllData) return m_allBrdData;	
	if (RinexVersion < 3.0)
	{
		readNFileVer2(m_allBrdData);
	}

	return m_allBrdData;
}

//搜索最近的导航数据
int QReadGPSN::SearchNFile(int PRN,char SatType,double GPSOTime)
{//返回匹配的N导航电文
	//找到距离2h前导航报文
	int flag = 0;
	int lenNHead = m_allBrdData.length();

	QVector< int > Fileflag;
	QVector< double > FileflagTime;
	for (int i = 0;i < lenNHead;i++)
	{
		BrdData epochNData = m_allBrdData.at(i);
		if (PRN != epochNData.PRN || SatType != epochNData.SatType)
			continue;
		else
		{
			double GPSNTime = YMD2GPSTime(epochNData.UTCTime.Year,epochNData.UTCTime.Month,epochNData.UTCTime.Day,
				epochNData.UTCTime.Hours,epochNData.UTCTime.Minutes,epochNData.UTCTime.Seconds);
			Fileflag.append(i);
			FileflagTime.append(GPSNTime);
		}
	}
	//发现最小时间差<2h
	int FileflagLen = Fileflag.length();
	if (FileflagLen != FileflagTime.length())
		return -1;
	int flagMin = 0;
	if(FileflagLen == 0)
		return -1;
	double GPSNTime = FileflagTime.at(0);
	int Min = qAbs(GPSOTime - GPSNTime);
	for (int i = 0;i < FileflagLen;i++)
	{
		double tGPSNTime = FileflagTime.at(i);
		if (Min > qAbs((GPSOTime - tGPSNTime)))
		{
			flagMin = i;
			Min = GPSOTime - tGPSNTime;
		}
	}
	flag = Fileflag.at(flagMin);
	//下面应该加上绝对值qAbs()
	if (qAbs(GPSOTime - FileflagTime.at(flagMin)) > 2*3600+120)
	{
		flag = -1;
	}
	return flag;//-1表示未找到
}

//PRN:卫星号，SatType:卫星类型(G,C,R,E),年月日时分秒为观测时刻UTC时间(BDS和GLONASS函数内部自动转换)
void QReadGPSN::getSatPos(int PRN,char SatType,double CA,int Year,int Month,int Day,int Hours,int Minutes,double Seconds,double *pXYZ,double *pdXYZ)
{
	pXYZ[0] = 0;pXYZ[1] = 0;pXYZ[2] = 0;
	pdXYZ[0] = 0;pdXYZ[1] = 0;pdXYZ[2] = 0;
	if (!isReadAllData)  getAllData();
	double GPSOTime = YMD2GPSTime(Year,Month,Day,Hours,Minutes,Seconds);
	int flag = -1;
	flag = SearchNFile(PRN,SatType,GPSOTime);//匹配导航文件
	if (flag < 0) return ;

	BrdData epochBrdData = m_allBrdData.at(flag);

	//计算多系统（GPS+BDS+GLONASS）坐标
	double X = 0,Y = 0,Z = 0;//卫星坐标
	double dX = 0,dY = 0,dZ = 0;//卫星的速度
	double t = 0;//卫星信号发射时刻的GPS时间
	double Ek = 0;//要计算的E

	if (SatType == 'G' || SatType == 'C')
	{
		double n0 = qSqrt(M_GM)/qPow(epochBrdData.epochNData.at(7),3);
		double n = n0 + epochBrdData.epochNData.at(2);
		t = GPSOTime - CA/M_C;//卫星发出信号的时刻GPS周内秒
		if (SatType == 'C')
		{
			t -= 14;
		}
		double dltt_toe = t - epochBrdData.epochNData.at(8);
		if (dltt_toe > 302400)
			dltt_toe-=604800;
		else if (dltt_toe < -302400)
			dltt_toe+=604800;
		double M = epochBrdData.epochNData.at(3) + n*dltt_toe;
		if (M < 0)
			M = M + 2*PI;
		//使用迭代吗计算E
		double eps = 1e-13;
		double dv = 9999;
		double E0 = M;
		while(dv > eps)
		{
			Ek = M + epochBrdData.epochNData.at(5)*qSin(E0);
			dv = qAbs(Ek - E0);
			E0 = Ek;
		}
		//计算f
		double cosf = (qCos(Ek) - epochBrdData.epochNData.at(5))/(1 - epochBrdData.epochNData.at(5)*qCos(Ek));
		double sinf = (qSin(Ek)*qSqrt(1-epochBrdData.epochNData.at(5)*epochBrdData.epochNData.at(5)))/(1 - epochBrdData.epochNData.at(5)*qCos(Ek));
		double f = qAtan2(qSin(Ek)*qSqrt(1-epochBrdData.epochNData.at(5)*epochBrdData.epochNData.at(5)), qCos(Ek) - epochBrdData.epochNData.at(5));
		//double f = qAtan((qSin(Ek)*qSqrt(1-epochBrdData.epochNData.at(5)*epochBrdData.epochNData.at(5)))/(qCos(Ek) - epochBrdData.epochNData.at(5)));
		//计算ud
		double ud = epochBrdData.epochNData.at(14) + f;
		//计算摄动改正
		double Cuc = epochBrdData.epochNData.at(4);
		double Cus = epochBrdData.epochNData.at(6);
		double Crc = epochBrdData.epochNData.at(13);
		double Crs = epochBrdData.epochNData.at(1);
		double Cic = epochBrdData.epochNData.at(9);
		double Cis = epochBrdData.epochNData.at(11);
		double dltaU = Cuc*qCos(2*ud) + Cus*qSin(2*ud);
		double dltaR = Crc*qCos(2*ud) + Crs*qSin(2*ud);
		double dltaI = Cic*qCos(2*ud) + Cis*qSin(2*ud);
		double U = ud + dltaU;
		double R = epochBrdData.epochNData.at(7)*epochBrdData.epochNData.at(7)*(1 - epochBrdData.epochNData.at(5)*qCos(Ek)) + dltaR;
		double I = epochBrdData.epochNData.at(12) + dltaI + epochBrdData.epochNData.at(16)*(t - epochBrdData.epochNData.at(8));
		//计算卫星轨道面内的坐标
		//double  dU = qCos(U);
		double x = R*qCos(U);
		double y = R*qSin(U);
		//计算瞬间升交点的精度L
		//double L = epochNData[10] + (epochNData[15] - M_We)*t - epochNData[15]*epochNData[8];
		double L = epochBrdData.epochNData.at(10) + epochBrdData.epochNData.at(15)*(dltt_toe) - M_We*t;
		if (SatType == 'C' && PRN < 6)
		{//转换为惯性坐标系下面的L
			L = epochBrdData.epochNData.at(10) + epochBrdData.epochNData.at(15)*(dltt_toe) - M_We*epochBrdData.epochNData.at(8);
		}
		if (L<0)
			L = L + 2*PI;
		//计算卫星瞬时坐标
		X = x*qCos(L) - y*qCos(I)*qSin(L);
		Y = x*qSin(L) + y*qCos(I)*qCos(L);
		Z = y*qSin(I);
		//计算卫星速度
		double dM = n;
		double dE = dM/(1-epochBrdData.epochNData.at(5)*qCos(Ek));
		double df = qSqrt(1-epochBrdData.epochNData.at(5)*epochBrdData.epochNData.at(5))*dE/(1-epochBrdData.epochNData.at(5)*qCos(Ek));
		double du = df;
		double DdltaU = 2*du*(Cus*qCos(2*ud) - Cuc*qSin(2*ud));
		double DdltaR = 2*du*(Crs*qCos(2*ud) - Crc*qSin(2*ud));
		double DdltaI = 2*du*(Cis*qCos(2*ud) - Cic*qSin(2*ud));
		double dU = du + DdltaU;
		double dR = epochBrdData.epochNData.at(7)*epochBrdData.epochNData.at(7)*epochBrdData.epochNData.at(5)*dE*qSin(Ek)+DdltaR;
		double dI = epochBrdData.epochNData.at(16) + DdltaI;
		double dL = epochBrdData.epochNData.at(15) - M_We;
		if (SatType == 'C' && PRN < 6)
		{//转换为惯性坐标系下面的L
			dL = epochBrdData.epochNData.at(15);
		}
		double dx = dR*qCos(U) - R*dU*qSin(U);
		double dy = dR*qSin(U) + R*dU*qCos(U);
		//计算速度
		dX = -Y*dL - (dy*qCos(I) - Z*dI)*qSin(L) + dx*qCos(L);
		dY = X*dL + (dy*qCos(I) - Z*dI)*qCos(L) + dx*qSin(L);
		dZ = dy*qSin(I) + dy*dI*qCos(I);
		//判断是否是BDS 1-5号卫星
		if (SatType == 'C' && PRN < 6)
		{
			//L===============
			double Wet_toe = M_We*(dltt_toe);
			//经过Rz（Wet_toe） Rx（-5）转换坐标
			Matrix3d Rz,Rx,dRz;
			Vector3d Vx3,dVx3;//未进行坐标转换前位置和速度
			Vector3d VX,dVX;//转换后位置和速度
			Vx3<<X,Y,Z;
			dVx3<<dX,dY,dZ;
			Rz<<qCos(Wet_toe),qSin(Wet_toe),0,
				-qSin(Wet_toe),qCos(Wet_toe),0,
				0,0,1;
			dRz<<-qSin(Wet_toe),qCos(Wet_toe),0,
				-qCos(Wet_toe),-qSin(Wet_toe),0,
				0,0,0;
			Rx<<1,0,0,
				0,qCos(-5*PI/180),qSin(-5*PI/180),
				0,-qSin(-5*PI/180),qCos(-5*PI/180);
			VX = Rz*Rx*Vx3;
			dVX = Rz*Rx*dVx3 + M_We*dRz*Rx*Vx3;
			X = VX(0);Y = VX(1);Z = VX(2);
			dX = dVX(0);dY = dVX(1);dZ = dVX(2);
		}
	}
	else if (SatType == 'R')
	{
		t = GPSOTime - CA/M_C;//卫星发出信号的时刻GPS周内秒
		double t0 = YMD2GPSTime(epochBrdData.UTCTime.Year,epochBrdData.UTCTime.Month,epochBrdData.UTCTime.Day,
			epochBrdData.UTCTime.Hours,epochBrdData.UTCTime.Minutes,epochBrdData.UTCTime.Seconds);
		////使用龙格库塔计算GLONASS卫星坐标/////
		Vector3d GLOXYZ,GLOdX;
		t = t - m_leapSec;//将GPS时间转换为GLONASS时间（G文件并非GLONASS时间，而是标准UTC所以只差一个整数跳秒）
		GLOXYZ = RungeKuttaforGlonass(epochBrdData,t,t0,GLOdX);//计算G文件坐标并为
		//根据经验七参数转换到WGS84
		GLOXYZ = GLOXYZ*1000;
		GLOdX = GLOdX*1000;
		Matrix3d CM;
		Vector3d dltaX;
		dltaX<<-0.47,-0.51,-1.56;
		CM<<1,1.728e-6,-0.0178e-6,
			1.728e-6,1,0.076e-6,
			0.0178e-6,-0.076e-6,1;
		//GLOXYZ = dltaX + (1+22e-9)*CM*GLOXYZ;//PZ-90转换到WGS84坐标下(可以忽略)
		X = GLOXYZ(0);Y = GLOXYZ(1);Z = GLOXYZ(2); 
		dX = GLOdX(0);dY = GLOdX(1);dZ = GLOdX(2);
		//计算GLONASS相对论效应
		//GlonassRel = -2*(GLOXYZ(0)*GLOdX(0)+GLOXYZ(1)*GLOdX(1)+GLOXYZ(2)*GLOdX(2))/(M_C);
	}
	pXYZ[0] = X;pXYZ[1] = Y;pXYZ[2] = Z;
	pdXYZ[0] = dX;pdXYZ[1] = dY;pdXYZ[2] = dZ;
}

//计算GPS时间
double QReadGPSN::YMD2GPSTime(int Year,int Month,int Day,int HoursInt,int Minutes,int Seconds,int *WeekN)//,int *GPSTimeArray
{
	double Hours = HoursInt + ((Minutes * 60) + Seconds)/3600.0;
	//Get JD
	double JD = 0.0;
	if(Month<=2)
		JD = (int)(365.25*(Year-1)) + (int)(30.6001*(Month+12+1)) + Day + Hours/24.0 + 1720981.5;
	else
		JD = (int)(365.25*(Year)) + (int)(30.6001*(Month+1)) + Day + Hours/24.0 + 1720981.5;
	//Get GPS Week and Days
	int Week = (int)((JD - 2444244.5) / 7);
	int N =(int)(JD + 1.5)%7;
	if (WeekN) *WeekN = Week;
	return (N*24*3600 + HoursInt*3600 + Minutes*60 + Seconds);
}

//计算儒略日
double QReadGPSN::computeJD(int Year,int Month,int Day,int HoursInt,int Minutes,double Seconds)
{
	double Hours = HoursInt + ((Minutes * 60) + Seconds)/3600.0;
	//Get JD
	double JD = 0.0;
	if(Month<=2)
		JD = (int)(365.25*(Year-1)) + (int)(30.6001*(Month+12+1)) + Day + Hours/24.0 + 1720981.5;
	else
		JD = (int)(365.25*(Year)) + (int)(30.6001*(Month+1)) + Day + Hours/24.0 + 1720981.5;
	return JD;
}

//计算跳秒
double QReadGPSN::getLeapSecond(int Year,int Month,int Day,int Hours/* =0 */,int Minutes/* =0 */,int Seconds/* =0 */)
{
	double jd = computeJD(Year,Month,Day,Hours);
	double leapseconds=0;
	double Leap_seconds[50]={0};
	double TAImUTCData[50]={0};
	Leap_seconds[0]=10;
	TAImUTCData[0]=computeJD(1972,1,1,0);
	Leap_seconds[1]=11;
	TAImUTCData[1]=computeJD(1972,7,1,0);
	Leap_seconds[2]=12;
	TAImUTCData[2]=computeJD(1973,1,1,0);
	Leap_seconds[3]=13;
	TAImUTCData[3]=computeJD(1974,1,1,0);
	Leap_seconds[4]=14;
	TAImUTCData[4]=computeJD(1975,1,1,0);
	Leap_seconds[5]=15;
	TAImUTCData[5]=computeJD(1976,1,1,0);
	Leap_seconds[6]=16;
	TAImUTCData[6]=computeJD(1977,1,1,0);
	Leap_seconds[7]=17;
	TAImUTCData[7]=computeJD(1978,1,1,0);
	Leap_seconds[8]=18;
	TAImUTCData[8]=computeJD(1979,1,1,0);
	Leap_seconds[9]=19;
	TAImUTCData[9]=computeJD(1980,1,1,0);
	Leap_seconds[10]=20;
	TAImUTCData[10]=computeJD(1981,7,1,0);
	Leap_seconds[11]=21;
	TAImUTCData[11]=computeJD(1982,7,1,0);
	Leap_seconds[12]=22;
	TAImUTCData[12]=computeJD(1983,7,1,0);
	Leap_seconds[13]=23;
	TAImUTCData[13]=computeJD(1985,7,1,0);
	Leap_seconds[14]=24;
	TAImUTCData[14]=computeJD(1988,1,1,0);
	Leap_seconds[15]=25;
	TAImUTCData[15]=computeJD(1990,1,1,0);
	Leap_seconds[16]=26;
	TAImUTCData[16]=computeJD(1991,1,1,0);
	Leap_seconds[17]=27;
	TAImUTCData[17]=computeJD(1992,7,1,0);
	Leap_seconds[18]=28;
	TAImUTCData[18]=computeJD(1993,7,1,0);
	Leap_seconds[19]=29;
	TAImUTCData[19]=computeJD(1994,7,1,0);
	Leap_seconds[20]=30;
	TAImUTCData[20]=computeJD(1996,1,1,0);
	Leap_seconds[21]=31;
	TAImUTCData[21]=computeJD(1997,7,1,0);
	Leap_seconds[22]=32;
	TAImUTCData[22]=computeJD(1999,1,1,0);
	Leap_seconds[23]=33;
	TAImUTCData[23]=computeJD(2006,1,1,0);
	Leap_seconds[24]=34;
	TAImUTCData[24]=computeJD(2009,1,1,0);
	Leap_seconds[25]=35;
	TAImUTCData[25]=computeJD(2012,7,1,0);
	Leap_seconds[26]=36;
	TAImUTCData[26]=computeJD(2015,7,1,0);
	Leap_seconds[27]=37;
	TAImUTCData[27]=computeJD(2016,7,1,0);
	Leap_seconds[28]=38;
	TAImUTCData[28]=computeJD(2017,7,1,0);
	if (jd<TAImUTCData[0])
	{
		leapseconds=0;
	}
	else if (jd>TAImUTCData[28])
	{
		leapseconds=Leap_seconds[28];
	}
	else
	{
		int iter=0;
		for (int i=1;i<28;i++)
		{
			if (jd<=TAImUTCData[i] && jd>TAImUTCData[i-1])
			{
				iter=i;
				break;
			}
		}
		leapseconds=Leap_seconds[iter-1];
	}
	return (leapseconds-19);
}

//四阶龙格库塔方法
Vector3d QReadGPSN::RungeKuttaforGlonass(const BrdData &epochBrdData,double tk,double t0,Vector3d &dX)
{
	Vector3d X0,dX0,ddX0;
	X0<<epochBrdData.epochNData.at(0),epochBrdData.epochNData.at(4),epochBrdData.epochNData.at(8);
	dX0<<epochBrdData.epochNData.at(1),epochBrdData.epochNData.at(5),epochBrdData.epochNData.at(9);
	ddX0<<epochBrdData.epochNData.at(2),epochBrdData.epochNData.at(6),epochBrdData.epochNData.at(10);

	double dh = 0;//定义步长
	if (tk > t0)
		dh = 30;//(秒s)
	else
		dh = -30;
	if (qAbs(tk - t0) < 30)
		dh = tk - t0;
	//使用RungeKutta计算
	int n = (int)((tk - t0)/dh);
	Vector3d Xtn,Ztn,Xtn1,Ztn1;
	Xtn = X0;
	Ztn = dX0;
	Xtn1.setZero();
	Ztn1.setZero();
	if (qAbs(tk - t0) < 30)
		n = 0;
	for (int i = 0;i < n + 1;i++)
	{
		double h1 = tk - t0 - i*dh;//判断剩余步长????????正负关系
		if (qAbs(h1) < qAbs(dh))
			dh = h1;
		//计算龙格库塔系数(参考《数值分析第四版》P133化二阶为方程组求解)
		Vector3d L1,L2,L3,L4;
		L1 = GlonassFun(Xtn,Ztn,ddX0);
		L2 = GlonassFun(Xtn+(dh/2)*Ztn,Ztn+(dh/2)*L1,ddX0);
		L3 = GlonassFun(Xtn+(dh/2)*Ztn+(dh*dh/4)*L1,Ztn+(dh/2)*L2,ddX0);
		L4 = GlonassFun(Xtn+dh*Ztn+(dh*dh/2)*L2,Ztn+dh*L3,ddX0);
		Xtn1 = Xtn + dh*Ztn + (dh*dh/6)*(L1 + L2 + L3);
		Ztn1 = Ztn + (dh/6)*(L1 + 2*L2 + 2*L3 + L4);
		Xtn = Xtn1;
		Ztn = Ztn1;
	}
	dX = Ztn;
	return Xtn;
}

//GLONASS运动方程
Vector3d QReadGPSN::GlonassFun(Vector3d Xt,Vector3d dXt,Vector3d ddX0)
{
	Vector3d ddXt;
	Vector3d Xrz;
	Vector3d Xwt;
	double r = qSqrt(Xt(0)*Xt(0)+Xt(1)*Xt(1)+Xt(2)*Xt(2));
	Xrz<<Xt(0)*(1 - 5*Xt(2)*Xt(2)/(r*r)),Xt(1)*(1 - 5*Xt(2)*Xt(2)/(r*r)),Xt(2)*(3 - 5*Xt(2)*Xt(2)/(r*r));
	Xwt<<M_We*M_We*Xt(0) + 2*M_We*dXt(1),M_We*M_We*Xt(1) - 2*M_We*dXt(0),0;
	ddXt = (-M_GMK/(r*r*r))*Xt + ((1.5*M_GMK*M_ReK*M_ReK*M_C20)*Xrz/(r*r*r*r*r))+ ddX0 + Xwt;
	return ddXt;
}