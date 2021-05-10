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

#include "usrpmothread.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QDebug>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/interpolators.h"
#include "dsp/samplemofifo.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

USRPMOThread::USRPMOThread(uhd::tx_streamer::sptr stream, size_t bufSamples, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_packets(0),
    m_underflows(0),
    m_droppedPackets(0),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_sampleFifo(nullptr),
    m_log2Interp(0),
    m_fcPos(2)
{
    qDebug("USRPMOThread::USRPMOThread");

    for (unsigned int i = 0; i < 2; i++) {
        m_buf[i] = new qint16[2*m_bufSamples]; // *2 as samples are I+Q
    }

    resetStats();
}

USRPMOThread::~USRPMOThread()
{
    qDebug("USRPMOThread::~USRPMOThread");

    if (m_running) {
        stopWork();
    }

    for (unsigned int i = 0; i < 2; i++) {
        delete[] m_buf[i];
    }
}

void USRPMOThread::startWork()
{
    if (m_running) {
        return;
    }

    resetStats();
    m_startWaitMutex.lock();
    start();

    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}


void USRPMOThread::stopWork()
{
    uhd::async_metadata_t md;

    if (!m_running) {
        return; // return if not running
    }

    m_running = false;
    wait();

    try
    {
        // Get message indicating underflow, so it doesn't appear if we restart
        m_stream->recv_async_msg(md);
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMOThread::stopWork: exception: " << e.what();
    }

    qDebug("USRPMOThread::stopWork: stream stopped");
}

void USRPMOThread::resetStats()
{
    m_packets = 0;
    m_underflows = 0;
    m_droppedPackets = 0;
}

void USRPMOThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

unsigned int USRPMOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void USRPMOThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int USRPMOThread::getFcPos() const
{
    return m_fcPos;
}

void USRPMOThread::run()
{
    uhd::tx_metadata_t md;
    md.start_of_burst = false;
    md.end_of_burst   = false;

    m_running = true;
    m_startWaiter.wakeAll();

    qDebug("USRPOutputThread::run");

    while (m_running)
    {
        callback(m_buf, m_bufSamples);

        try
        {
            const size_t samples_sent = m_stream->send(m_buf, m_bufSamples, md);
            m_packets++;

            if (samples_sent != m_bufSamples) {
                qDebug("USRPOutputThread::run written %ld/%ld samples", samples_sent, m_bufSamples);
            }
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPOutputThread::run: exception: " << e.what();
        }
    }

    m_running = false;
}

void USRPMOThread::callback(qint16* buf[2], qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(buf, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        qint16 *buf2[2];
        buf2[0] = buf[0] + 2*shift;
        buf2[1] = buf[1] + 2*shift;
        callbackPart(buf2, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void USRPMOThread::callbackPart(qint16* buf[2], qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&begin, buf[channel], 2*nSamples);
        }
        else
        {
            if (m_fcPos == 0) // Infra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_inf(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_inf(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 1) // Supra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_sup(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_sup(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 2) // Center
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_cen(&begin, buf[channel], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_cen(&begin, buf[channel], 2*nSamples);
                    break;
                default:
                    break;
                }
            }

        }
    }
}
