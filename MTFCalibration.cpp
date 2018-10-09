void CalibrateMTF()
{
	UpdateData(TRUE);
	g_LogInfo.Init();
	DWORD dbg_handle = g_LogInfo.StartBlock("CalibrateMTF()");

	g_ld_tmp.load_defaults(0);
	std::vector<FPOINT> tfr;

	g_pMotorA->move_to_mm_position(dp_to_mm(g_ld_tmp, m_fPwrNom), TRUE);
	AdjustImageBrightness(0, 50,0);

	double f27CalibMTF =  g_IniFile.GetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, 1, 1);
	g_IniFile.SetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, 1); //set MTF calibration constant to 1

	g_ld_tmp.fZ3 = g_pMotorA->get_mm_position();

	//
    	int iWidthSaved  = g_IniFile.GetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_W,  2, 1);
    	int iHeightSaved = g_IniFile.GetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_H,  2, 1);
	int iHeightTFR = g_IniFile.GetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_TFR_H,  2, 1);
    	g_IniFile.SetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_W,  0);
    	g_IniFile.SetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_H,  iHeightTFR);


	//calculate TFR graph at 3mm/50lpm
	CalcTFR(g_ld_tmp, 3, m_fPwrNom, 50.0f, tfr, TRUE, FALSE);

	std::vector<FPOINT> mi;  
	GetHighestVectorValues(tfr, 1, mi, 1.0, TRUE, FALSE); //get highest peak on TFR graph
	if (!mi.empty())
	{
		
		float fMTF = mi[0].y;
		double f27CalibMTFNew = ( 0.83 / 0.98) / (fMTF == 0.0f ? 1 : fMTF); //new mtf calibration constant
		CString S;

		g_LogInfo.AddLog(dbg_handle,"( 0.83 / 0.98) / %5.3f)=%5.3f", fMTF, f27CalibMTFNew);

		S.Format("Old calibration constant: %6.5f\nNew calibration constant: %6.5f", f27CalibMTF, f27CalibMTFNew);
		S+="\n\nDo you want to save the new calibration ?";
		if (MessageBox(S,"Calibration", MB_YESNO | MB_ICONINFORMATION) == IDYES)
			g_IniFile.SetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, f27CalibMTFNew);
		else
			g_IniFile.SetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, f27CalibMTF);

	}
	else
	{
		MessageBox("Failed to calibrate MTF");
		g_IniFile.SetValueD(SEC_TFD_DLG,  KEY_FACTOR_27, f27CalibMTF);
	}

	g_IniFile.SetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_W,  iWidthSaved);
    	g_IniFile.SetValueI(SEC_ENERGY_SCAN2, KEY_INTENSITY_H,  iHeightSaved);

	g_LogInfo.EndBlock(dbg_handle);
}
