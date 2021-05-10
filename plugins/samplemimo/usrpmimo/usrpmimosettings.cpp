///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "util/simpleserializer.h"
#include "usrpmimosettings.h"

USRPMIMOSettings::USRPMIMOSettings()
{
    resetToDefaults();
}

void USRPMIMOSettings::resetToDefaults()
{
    m_masterClockRate = -1; // Calculated by UHD
    m_devSampleRate = 3000000;
    m_clockSource = "internal";
    m_rxCenterFrequency = 435000*1000;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_fcPosRx = FC_POS_CENTER;
    m_rxTransverterMode = false;
    m_rxTransverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_rx0LOOffset = 0;
    m_rx0LpfBW = 10e6f;
    m_rx0Gain = 50;
    m_rx0AntennaPath = "TX/RX";
    m_rx0GainMode = GAIN_AUTO;
    m_rx1LOOffset = 0;
    m_rx1LpfBW = 10e6f;
    m_rx1Gain = 50;
    m_rx1AntennaPath = "TX/RX";
    m_rx1GainMode = GAIN_AUTO;
    m_txCenterFrequency = 435000*1000;
    m_log2SoftInterp = 0;
    m_fcPosRx = FC_POS_CENTER;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;
    m_tx0LOOffset = 0;
    m_tx0LpfBW = 10e6f;
    m_tx0Gain = 50;
    m_tx0AntennaPath = "TX/RX";
    m_tx1LOOffset = 0;
    m_tx1LpfBW = 10e6f;
    m_tx1Gain = 50;
    m_tx1AntennaPath = "TX/RX";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray USRPMIMOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeString(2, m_clockSource);
    s.writeU64(3, m_rxCenterFrequency);
    s.writeBool(4, m_dcBlock);
    s.writeBool(5, m_iqCorrection);
    s.writeU32(6, m_log2SoftDecim);
    s.writeS32(7,  m_fcPosRx);
    s.writeBool(8, m_rxTransverterMode);
    s.writeS64(9, m_rxTransverterDeltaFrequency);
    s.writeBool(10, m_iqOrder);
    s.writeS32(20, m_rx0LOOffset);
    s.writeFloat(21, m_rx0LpfBW);
    s.writeU32(22, m_rx0Gain);
    s.writeString(23, m_rx0AntennaPath);
    s.writeS32(24, (int) m_rx0GainMode);
    s.writeS32(30, m_rx1LOOffset);
    s.writeFloat(31, m_rx1LpfBW);
    s.writeU32(32, m_rx1Gain);
    s.writeString(33, m_rx1AntennaPath);
    s.writeS32(34, (int) m_rx1GainMode);
    s.writeU64(40, m_txCenterFrequency);
    s.writeU32(41, m_log2SoftInterp);
    s.writeS32(42,  m_fcPosRx);
    s.writeBool(43, m_txTransverterMode);
    s.writeS64(44, m_txTransverterDeltaFrequency);
    s.writeS32(50, m_tx0LOOffset);
    s.writeFloat(51, m_tx0LpfBW);
    s.writeU32(52, m_tx0Gain);
    s.writeString(53, m_tx0AntennaPath);
    s.writeS32(60, m_tx1LOOffset);
    s.writeFloat(61, m_tx1LpfBW);
    s.writeU32(62, m_tx1Gain);
    s.writeString(63, m_tx1AntennaPath);
    s.writeBool(70, m_useReverseAPI);
    s.writeString(71, m_reverseAPIAddress);
    s.writeU32(72, m_reverseAPIPort);
    s.writeU32(73, m_reverseAPIDeviceIndex);
    return s.final();
}

bool USRPMIMOSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int intval;
        uint32_t uintval;

        d.readS32(1, &m_devSampleRate, 5000000);
        d.readString(2, &m_clockSource, "internal");
        d.readU64(3, &m_rxCenterFrequency, 435000*1000);
        d.readBool(4, &m_dcBlock, false);
        d.readBool(5, &m_iqCorrection, false);
        d.readU32(6, &m_log2SoftDecim, 0);
		d.readS32(7, &intval, 0);
		if ((intval < 0) || (intval > 2)) {
		    m_fcPosRx = FC_POS_CENTER;
		} else {
	        m_fcPosRx = (fcPos_t) intval;
		}
        d.readBool(8, &m_rxTransverterMode, false);
        d.readS64(9, &m_rxTransverterDeltaFrequency, 0);
        d.readBool(10, &m_iqOrder, true);
        d.readS32(20, &m_rx0LOOffset, 0);
        d.readFloat(21, &m_rx0LpfBW, 1.5e6);
        d.readU32(22, &m_rx0Gain, 50);
        d.readString(23, &m_rx0AntennaPath, "TX/RX");
        d.readS32(24, &intval, 0);
        m_rx0GainMode = (GainMode) intval;
        d.readS32(30, &m_rx1LOOffset, 0);
        d.readFloat(31, &m_rx1LpfBW, 1.5e6);
        d.readU32(32, &m_rx1Gain, 50);
        d.readString(33, &m_rx1AntennaPath, "TX/RX");
        d.readS32(34, &intval, 0);
        m_rx1GainMode = (GainMode) intval;
        d.readU64(40, &m_txCenterFrequency, 435000*1000);
        d.readU32(41, &m_log2SoftInterp, 0);
		d.readS32(42, &intval, 0);
		if ((intval < 0) || (intval > 2)) {
		    m_fcPosTx = FC_POS_CENTER;
		} else {
	        m_fcPosTx = (fcPos_t) intval;
		}
        d.readBool(43, &m_txTransverterMode, false);
        d.readS64(44, &m_txTransverterDeltaFrequency, 0);
        d.readS32(50, &m_tx0LOOffset, 0);
        d.readFloat(51, &m_tx0LpfBW, 1.5e6);
        d.readU32(52, &m_tx0Gain, 4);
        d.readString(53, &m_tx0AntennaPath, "TX/RX");
        d.readS32(60, &m_tx1LOOffset, 0);
        d.readFloat(61, &m_tx1LpfBW, 1.5e6);
        d.readU32(61, &m_tx1Gain, 4);
        d.readString(62, &m_tx1AntennaPath, "TX/RX");
        d.readBool(70, &m_useReverseAPI, false);
        d.readString(71, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(72, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(73, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}
