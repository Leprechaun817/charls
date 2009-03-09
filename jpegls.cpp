// 
// (C) Jan de Vaan 2007-2009, all rights reserved. See the accompanying "License.txt" for licensed use. 
// 

#include "stdafx.h"
#include "streams.h"
#include "header.h"
               

#include <math.h>
#include <limits>
#include <vector>
#include <STDIO.H>
#include <iostream>

#include "util.h"
 
#include "decoderstrategy.h"
#include "encoderstrategy.h"
#include "context.h"
#include "contextrunmode.h"
#include "lookuptable.h"

// used to determine how large runs should be encoded at a time. 
const int J[32]			= {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15};

const int BASIC_T1		= 3;
const int BASIC_T2		= 7;
const int BASIC_T3		= 21;

#include "losslesstraits.h"
#include "defaulttraits.h"

#include "scan.h"

signed char QuantizeGratientOrg(const Presets& preset, int NEAR, int Di)
{
	if (Di <= -preset.T3) return  -4;
	if (Di <= -preset.T2) return  -3;
	if (Di <= -preset.T1) return  -2;
	if (Di < -NEAR)  return  -1;
	if (Di <=  NEAR) return   0;
	if (Di < preset.T1)   return   1;
	if (Di < preset.T2)   return   2;
	if (Di < preset.T3)   return   3;
	
	return  4;
}



std::vector<signed char> CreateQLutLossless(int cbit)
{
	Presets preset = ComputeDefault((1 << cbit) - 1, 0);
	int range = preset.MAXVAL + 1;

	std::vector<signed char> lut;
	lut.resize(range * 2);
	
	for (int diff = -range; diff < range; diff++)
	{
		lut[range + diff] = QuantizeGratientOrg(preset, 0,diff);
	}
	return lut;
}

CTable rgtableShared[16] = { InitTable(0), InitTable(1), InitTable(2), InitTable(3), 
							 InitTable(4), InitTable(5), InitTable(6), InitTable(7), 
							 InitTable(8), InitTable(9), InitTable(10), InitTable(11), 
							 InitTable(12), InitTable(13), InitTable(14),InitTable(15) };

std::vector<signed char> rgquant8Ll = CreateQLutLossless(8);
std::vector<signed char> rgquant10Ll = CreateQLutLossless(10);
std::vector<signed char> rgquant12Ll = CreateQLutLossless(12);
std::vector<signed char> rgquant16Ll = CreateQLutLossless(16);


template<class STRATEGY>
STRATEGY* JlsCodecFactory<STRATEGY>::GetCodec(const JlsParamaters& _info, const JlsCustomParameters& presets)
{
	STRATEGY* pstrategy = NULL;
	if (presets.RESET != 0 && presets.RESET != BASIC_RESET)
	{
		typename DefaultTraitsT<BYTE,BYTE> traits((1 << _info.bitspersample) - 1, _info.allowedlossyerror); 
		traits.MAXVAL = presets.MAXVAL;
		traits.RESET = presets.RESET;
		pstrategy = new JlsCodec<DefaultTraitsT<BYTE, BYTE>, STRATEGY>(traits); 
	}
	else
	{
		pstrategy = GetCodecImpl(_info);
	}

	if (pstrategy == NULL)
		return NULL;

	pstrategy->SetPresets(presets);
	return pstrategy;
}

template<class STRATEGY>
STRATEGY* JlsCodecFactory<STRATEGY>::GetCodecImpl(const JlsParamaters& _info)
{
	if (_info.components != 1 && _info.ilv == ILV_SAMPLE)
	{
		if (_info.components == 3 && _info.bitspersample == 8)
		{
			if (_info.allowedlossyerror == 0)
				return new JlsCodec<LosslessTraitsT<Triplet,8>, STRATEGY>();

			typename DefaultTraitsT<BYTE,Triplet> traits((1 << _info.bitspersample) - 1, _info.allowedlossyerror); 
			return new JlsCodec<DefaultTraitsT<BYTE,Triplet>, STRATEGY>(traits); 	
		}
	
		return NULL;
	}

	// optimized lossless versions common monochrome formats (lossless)
	if (_info.allowedlossyerror == 0)
	{		
		switch (_info.bitspersample)
		{
			case  8: return new JlsCodec<LosslessTraitsT<BYTE,   8>, STRATEGY>(); 
			case 10: return new JlsCodec<LosslessTraitsT<USHORT,10>, STRATEGY>(); 
			case 12: return new JlsCodec<LosslessTraitsT<USHORT,12>, STRATEGY>();
			case 16: return new JlsCodec<LosslessTraitsT<USHORT,16>, STRATEGY>();
		}
	}


	if (_info.bitspersample <= 8)
	{
		typename DefaultTraitsT<BYTE, BYTE> traits((1 << _info.bitspersample) - 1, _info.allowedlossyerror); 
		return new JlsCodec<DefaultTraitsT<BYTE, BYTE>, STRATEGY>(traits); 
	}

	if (_info.bitspersample <= 16)
	{
		typename DefaultTraitsT<USHORT, USHORT> traits((1 << _info.bitspersample) - 1, _info.allowedlossyerror); 
		return new JlsCodec<DefaultTraitsT<USHORT, USHORT>, STRATEGY>(traits); 
	}
	return NULL;
}


template class JlsCodecFactory<DecoderStrategy>;
template class JlsCodecFactory<EncoderStrategy>;