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

#include <QDebug>

#include "usrpmithread.h"

USRPMIThread::USRPMIThread(uhd::rx_streamer::sptr stream, int bufSamples, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_packets(0),
    m_overflows(0),
    m_timeouts(0),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_sampleFifo(nullptr),
    m_iqOrder(true)
{
    qDebug("USRPMIThread::USRPMIThread");

    for (unsigned int i = 0; i < 2; i++)
    {
        m_buf[i] = new qint16[2*m_bufSamples]; // *2 as samples are I+Q
        m_convertBuffer[i].resize(m_bufSamples, Sample{0,0});
    }

    resetStats();
}

USRPMIThread::~USRPMIThread()
{
    qDebug("USRPMIThread::~USRPMIThread");

    if (m_running) {
        stopWork();
    }

    for (unsigned int i = 0; i < 2; i++) {
        delete[] m_buf[i];
    }
}

void USRPMIThread::startWork()
{
    if (m_running) {
        return;
    }

    try
    {
        // Start streaming
        issueStreamCmd(true);

        // Reset stats
        m_packets = 0;
        m_overflows = 0;
        m_timeouts = 0;

        qDebug("USRPMIThread::startWork: stream started");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMIThread::startWork: exception: " << e.what();
        return;
    }

    m_startWaitMutex.lock();
    start();

    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void USRPMIThread::stopWork()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    wait();

    try
    {
        uhd::rx_metadata_t md;

        // Stop streaming
        issueStreamCmd(false);

        // Clear out any data left in the stream, otherwise we'll get an
        // exception 'recv buffer smaller than vrt packet offset' when restarting
        md.end_of_burst = false;
        md.error_code = uhd::rx_metadata_t::ERROR_CODE_NONE;

        while (!md.end_of_burst && (md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT))
        {
            try
            {
                md.reset();
                m_stream->recv(m_buf, m_bufSamples, md);
            }
            catch (std::exception& e)
            {
                qDebug() << "USRPMIThread::stopWork: exception ignored while flushing buffers: " << e.what();
            }
        }

        qDebug("USRPMIThread::stopWork: stream stopped");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMIThread::stopWork: exception: " << e.what();
    }
}

void USRPMIThread::setLog2Decimation(unsigned int log2Decim)
{
    m_log2Decim = log2Decim;
}

unsigned int USRPMIThread::getLog2Decimation() const
{
    return m_log2Decim;
}

void USRPMIThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int USRPMIThread::getFcPos() const
{
    return m_fcPos;
}

void USRPMIThread::getStreamStatus(bool& active, quint32& overflows, quint32& timeouts)
{
    //qDebug() << "USRPInputThread::getStreamStatus " << m_packets << " " << m_overflows << " " << m_timeouts;
    active = m_packets > 0;
    overflows = m_overflows;
    timeouts = m_timeouts;
}

void USRPMIThread::issueStreamCmd(bool start)
{
    uhd::stream_cmd_t stream_cmd(start ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS : uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    stream_cmd.num_samps = size_t(0);
    stream_cmd.stream_now = true;
    stream_cmd.time_spec = uhd::time_spec_t();

    m_stream->issue_stream_cmd(stream_cmd);
    qDebug() << "USRPMIThread::issueStreamCmd " << (start ? "start" : "stop");
}

void USRPMIThread::resetStats()
{
    m_packets = 0;
    m_overflows = 0;
    m_timeouts = 0;
}

void USRPMIThread::run()
{
    uhd::rx_metadata_t md;

    m_running = true;
    m_startWaiter.wakeAll();

    try
    {
        while (m_running)
        {
            md.reset();
            const size_t samples_received = m_stream->recv(m_buf, m_bufSamples, md);

            m_packets++;

            if (samples_received != m_bufSamples) {
                qDebug("USRPMIThread::run - received %ld/%ld samples", samples_received, m_bufSamples);
            }

            if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            {
                qDebug("USRPMIThread::run - timeout - ending thread");
                m_timeouts++;
                // Restart streaming
                issueStreamCmd(false);
                issueStreamCmd(true);
                qDebug("USRPMIThread::run - timeout - restarting");
            }
            else if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_OVERFLOW)
            {
                qDebug("USRPMIThread::run - overflow");
                m_overflows++;
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND)
            {
                qDebug("USRPMIThread::run - late command error");
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN)
            {
                qDebug("USRPMIThread::run - broken chain error");
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_ALIGNMENT)
            {
                qDebug("USRPMIThread::run - alignment error");
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET)
            {
                qDebug("USRPMIThread::run - bad packet error");
            }

            if (samples_received > 0)
            {
                if (m_iqOrder)
                {
                    channelCallbackIQ(m_buf[0], 2 * samples_received, 0);
                    channelCallbackIQ(m_buf[1], 2 * samples_received, 1);
                }
                else
                {
                    channelCallbackQI(m_buf[0], 2 * samples_received, 0);
                    channelCallbackQI(m_buf[1], 2 * samples_received, 1);
                }
            }
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMIThread::run: exception: " << e.what();
    }

    m_running = false;
}

int USRPMIThread::channelCallbackIQ(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsIQ[channel].decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ[channel].decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 1) // Supra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ[channel].decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 2) // Center
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ[channel].decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    return it - m_convertBuffer[channel].begin();
}

int USRPMIThread::channelCallbackQI(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsQI[channel].decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI[channel].decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 1) // Supra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI[channel].decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 2) // Center
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI[channel].decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    return it - m_convertBuffer[channel].begin();
}
