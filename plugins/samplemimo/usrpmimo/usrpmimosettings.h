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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

/**
 * These are the settings individual to each hardware channel or software Rx chain
 * Plus the settings to be saved in the presets
 */
struct USRPMIMOSettings
{
    typedef enum {
        GAIN_AUTO,
        GAIN_MANUAL
    } GainMode;

	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER,
		FC_POS_END
	} fcPos_t;

    // Common
    int      m_masterClockRate;
    int      m_devSampleRate;
    QString  m_clockSource;
    // Rx Common
    quint64  m_rxCenterFrequency;
    bool     m_dcBlock;
    bool     m_iqCorrection;
    uint32_t m_log2SoftDecim;
    fcPos_t  m_fcPosRx;
    bool     m_rxTransverterMode;
    qint64   m_rxTransverterDeltaFrequency;
    bool     m_iqOrder;
    // Rx0  channel settings
    int      m_rx0LOOffset;
    float    m_rx0LpfBW;        //!< Analog lowpass filter bandwidth (Hz)
    uint32_t m_rx0Gain;         //!< Optimally distributed gain (dB)
    QString  m_rx0AntennaPath;
    GainMode m_rx0GainMode;     //!< Gain mode: auto or manual
    // Rx1  channel settings
    int      m_rx1LOOffset;
    float    m_rx1LpfBW;        //!< Analog lowpass filter bandwidth (Hz)
    uint32_t m_rx1Gain;         //!< Optimally distributed gain (dB)
    QString  m_rx1AntennaPath;
    GainMode m_rx1GainMode;     //!< Gain mode: auto or manual
    // Tx Common
    quint64  m_txCenterFrequency;
    uint32_t m_log2SoftInterp;
    fcPos_t  m_fcPosTx;
    bool     m_txTransverterMode;
    qint64   m_txTransverterDeltaFrequency;
    // Tx0 channel settings
    int      m_tx0LOOffset;
    float    m_tx0LpfBW;        //!< Analog lowpass filter bandwidth (Hz)
    uint32_t m_tx0Gain;         //!< Optimally distributed gain (dB)
    QString  m_tx0AntennaPath;
    // Tx1 channel settings
    int      m_tx1LOOffset;
    float    m_tx1LpfBW;        //!< Analog lowpass filter bandwidth (Hz)
    uint32_t m_tx1Gain;         //!< Optimally distributed gain (dB)
    QString  m_tx1AntennaPath;
    // Reverse API
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    // B210 supports up to 50MSa/s, so a fairly large FIFO is probably a good idea
    // Should it be bigger still?
    // Sample FIFO is made 16 times larger
    static const int m_usrplockSizeSamples = 128*1024; //!< sample fifo is 16 times larger

    USRPMIMOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_ */
