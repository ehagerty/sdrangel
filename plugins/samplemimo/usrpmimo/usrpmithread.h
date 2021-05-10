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

#ifndef _USRPMIMO_USRPMITHREAD_H_
#define _USRPMIMO_USRPMITHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

class SampleMIFifo;

class USRPMIThread : public QThread
{
    Q_OBJECT

public:
    USRPMIThread(uhd::rx_streamer::sptr stream, int bufSamples, QObject* parent = nullptr);
    ~USRPMIThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Decimation(unsigned int log2Decim);
    unsigned int getLog2Decimation() const;
    void getStreamStatus(bool& active, quint32& overflows, quint32& m_timeouts);
    void setFcPos(int fcPos);
    int getFcPos() const;
    void setFifo(SampleMIFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMIFifo *getFifo() { return m_sampleFifo; }
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }
    void issueStreamCmd(bool start);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    quint64 m_packets;
    quint32 m_overflows;
    quint32 m_timeouts;

    uhd::rx_streamer::sptr m_stream;
    size_t m_bufSamples; //!< USRP buffer size i.e. max number of samples per buffer per packet (given by parent).
    qint16 *m_buf[2]; //!< one buffer per I/Q channel
	SampleVector m_convertBuffer[2];
    SampleMIFifo *m_sampleFifo;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ[2];
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI[2];
    unsigned int m_log2Decim;
    int m_fcPos;
    bool m_iqOrder;

    void resetStats();
    void run();
    int channelCallbackIQ(const qint16* buf, qint32 len, int channel);
    int channelCallbackQI(const qint16* buf, qint32 len, int channel);
};



#endif // _USRPMIMO_USRPMITHREAD_H_
