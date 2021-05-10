 ///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef _USRPMIMO_USRPMOTHREAD_H_
#define _USRPMIMO_USRPMOTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/interpolators.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

class SampleMOFifo;

class USRPMOThread : public QThread
{
    Q_OBJECT

public:
    USRPMOThread(uhd::tx_streamer::sptr stream, size_t bufSamples, QObject* parent = nullptr);
    ~USRPMOThread();

    virtual void startWork();
    virtual void stopWork();
    virtual bool isRunning() { return m_running; }
    void setLog2Interpolation(unsigned int log2_ioterp);
    unsigned int getLog2Interpolation() const;
    void getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets);
    void setFcPos(int fcPos);
    int getFcPos() const;
    void setFifo(SampleMOFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMOFifo *getFifo() { return m_sampleFifo; }

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    quint64 m_packets;
    quint32 m_underflows;
    quint32 m_droppedPackets;

    uhd::tx_streamer::sptr m_stream;
    qint16 *m_buf[2];
    size_t m_bufSamples;
    SampleMOFifo* m_sampleFifo;

    unsigned int m_log2Interp; // soft decimation
    int m_fcPos;

    Interpolators<qint16, SDR_TX_SAMP_SZ, 12> m_interpolators[2];

    void resetStats();
    void run();
    void callback(qint16* buf[2], qint32 samplesPerChannel);
    void callbackPart(qint16* buf[2], qint32 nSamples, int iBegin);
};



#endif /* _USRPMIMO_USRPMOTHREAD_H_ */
