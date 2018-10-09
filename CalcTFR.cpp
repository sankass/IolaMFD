//ID361
//A - aperture, P power, bIsMTF - MTF or OTF
void CalcTFR(IMF_FILE_HEADER_3& LD, float A, float P, float lpm, std::vector<FPOINT>& tfr, BOOL bIsMTF, BOOL b90Deg,std::vector<FPOINT>* ext_es)
{
	DWORD dbg_handle = g_LogInfo.StartBlock("CalcTFR");

	std::vector<FPOINT> ES;
	float* omatrix_re = g_pOptics->GetOmatrix_re();
	float* omatrix = g_pOptics->GetOmatrix();

	int osize = g_pOptics->GetOsize();
	
	//GrabFramesForShowOptics(LD,  LD.image_0_ptr(), LD.image_90_ptr());
	//g_pOptics->CreateOptics(LD,  LD.image_0_ptr(), FSIZE, A, 0, FALSE,b90Deg);


	double Pm = P;
	g_LogInfo.AddLog(dbg_handle, "Elevation: %4.2f, Pm=%4.2f", LD.elevation, Pm);
	double elevation = LD.elevation;
	LD.elevation = 0;

	if (ext_es != NULL) //ES graph already supplied from external source
	{
		ES.assign(ext_es->begin(), ext_es->end());
	}
	else
	{
		g_pOptics->CreateOpticsNImages(LD, A, 0);
		g_pOptics->CreateOpticsCS(LD, omatrix_re, osize, int_round(g_pOptics->GetT()), ES);
	}

	//(1) Calculate the area Am
	float Am = 0;
	float dP;
	std::vector<FPOINT>::iterator it_next = ES.begin();	it_next++;
	std::vector<FPOINT>::iterator it_last = ES.end();	it_last--;
	
	g_LogInfo.AddLog(dbg_handle, "Energy vector len: %i", ES.size());
	CString csES, S;
	FPOINT* pES = &ES[0];

	//use only 129 points from the center of graph
	int len = ES.size();
	int istart = (len - LOW_OSIZE) / 2;
	/*
	for(int i = 0; i < LOW_OSIZE-1; i++)
	{
		dP = fabs(pES[istart+i+1].x - pES[istart+i].x);
		Am += pES[istart+i].y * dP;
		S.Format("%6.4f\t%6.4f\n", pES[istart+i].x,pES[istart+i].y);
		csES += S;

	}
	*/
	for(std::vector<FPOINT>::iterator it = ES.begin(); it != it_last; ++it, ++it_next)
	{
		dP = fabs(it_next->x - it->x);
		Am += it->y * dP;

		S.Format("%6.4f\t%6.4f\n", it->x, it->y);
		csES += S;
	}
	

	{
		CStdioFile file;
		CString csName;
		csName.Format("optics_%4.2fmm_%i.txt", A, b90Deg ? 90 : 0);
		CString csFileName = g_csDebugPath + csName;
		if (file.Open(csFileName, CFile::modeCreate | CFile::modeWrite))
		{
			file.WriteString(csES);
			file.Close();
		}
	}


	//(11) Calculate Theoretical Area, AT(D,P) 
	double At50 = C00 + C10*A + C01*Pm + C20*A*A + C11*A*Pm + C02*Pm*Pm +  C30*A*A*A + C21*A*A*Pm + C12*A*Pm*Pm;

	/*double P00 = 3.602, P10 = -2.178,  P01 = 0.02086, P20 = 0.5521, P11 = -0.01255,
		   P02 = 0.001324, P30 = -0.04864, P21 = 0.001725, P12 = -8.842e-005, P03 = -2.114e-005;  
*/

  //  double At25 = P00 + P10*A + P01*P + P20*A*A + P11*A*P + P02*P*P + P30*A*A*A + P21*A*A*P+ 
  //                P12*A*P*P + P03*P*P*P;
        


	double Cutoff = CALC_CUT_OFF(Pm, A);
	double MTFDif50 = CALC_MTF_VAL(50 / Cutoff);


	double f27CalibMTF =  g_IniFile.GetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, 1, 1);
	double f27factor = f27CalibMTF * (1 + 0.0015 * POW2(Pm-19.75));
	double A_Ratio =    (At50 / Am);//f27factor * (bMTF25 ?   (MTFDif25 * At25 / Am) : (MTFDif50 * At50 / Am));

	
	g_LogInfo.AddLog(dbg_handle, "P=%4.2f D, A=%4.2f mm, lpm=%i, Am=%5.4f, At50=%5.4f, A_Ratio=%4.3f", P, A, (int)lpm,  Am, At50, A_Ratio);
	g_LogInfo.AddLog(dbg_handle, "f27CalibMTF=%5.3f, f27factor=%4.3f",f27CalibMTF, f27factor);
	g_LogInfo.AddLog(dbg_handle, "DiffLimited(50)=%4.3f", MTFDif50);

	//(3) TFR
	double max_mtf = 0, MTF;
	tfr.resize(ES.size());
	std::vector<FPOINT>::iterator it_tfr = tfr.begin();
	for(std::vector<FPOINT>::iterator it = ES.begin(); it != ES.end(); ++it, ++it_tfr)
	{
		it_tfr->x = it->x;

		//correct with elevation
		it_tfr->x = ELEVATION(it_tfr->x , elevation);   //not neccecary, already corrected in CreateOpticsCS

		MTF = it->y * A_Ratio;// * f27factor;
		if (bIsMTF)
			MTF = fabs(MTF);

		it_tfr->y = (float)MTF;
		max_mtf = max(max_mtf, it_tfr->y);
	}

	g_LogInfo.AddLog(dbg_handle, "MaxTFR(before multiply by f27factor)=%4.2f", max_mtf);
	max_mtf = 0;
	for(std::vector<FPOINT>::iterator it = tfr.begin(); it != tfr.end(); ++it)
	{
		it->y *= f27factor;
		max_mtf = max(max_mtf, it->y);
	}

	g_LogInfo.AddLog(dbg_handle, "MaxTFR(after multiply by f27factor)=%4.2f", max_mtf);

	//adjust to diffraction limited
	if( max_mtf > 0.98 * MTFDif50)
	{
		float max_mtf2 = 0;
		float factor = (0.98f * MTFDif50 /  max_mtf);
		for(std::vector<FPOINT>::iterator it = tfr.begin(); it != tfr.end(); ++it)
		{
				it->y *= factor;
				max_mtf2 = max(max_mtf2, it->y);
		}
		g_LogInfo.AddLog(dbg_handle, "MaxTFR(after adjust to DL)=%4.2f", max_mtf2);
	}

	
	
	

	LD.elevation = elevation;

	g_LogInfo.EndBlock(dbg_handle);
}