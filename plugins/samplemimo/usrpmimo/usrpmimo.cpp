///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"

#include "usrpmithread.h"
#include "usrpmothread.h"
#include "usrpmimo.h"

MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgConfigureUSRPMIMO, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgReportStreamInfo, Message)

USRPMIMO::USRPMIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_mutex(QMutex::Recursive),
    m_deviceParams(nullptr),
    m_rxStream(nullptr),
    m_txStream(nullptr),
    m_rxBufSamples(0),
    m_txBufSamples(0),
    m_rxChannelAcquired(false),
    m_txChannelAcquired(false),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("USRPMIMO"),
	m_runningRx(false),
    m_runningTx(false),
    m_open(false),
    m_nbRx(0),
    m_nbTx(0)
{
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 16 * USRPMIMOSettings::m_usrplockSizeSamples);
    m_sampleMOFifo.init(2, 16 * USRPMIMOSettings::m_usrplockSizeSamples);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    m_open = openDevice();

}

USRPMIMO::~USRPMIMO()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    closeDevice();
}

void USRPMIMO::destroy()
{
    delete this;
}

bool USRPMIMO::openDevice()
{
    qDebug("USRPMIMO::openDevice: open device here");

    m_deviceParams = new DeviceUSRPParams();
    char serial[256];
    strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
    m_deviceParams->open(serial, false);

    return true;
}

void USRPMIMO::closeDevice()
{
    if (m_deviceParams == nullptr) { // was never open
        return;
    }

    if (m_runningRx) {
        stopRx();
    }

    if (m_runningTx) {
        stopTx();
    }

    m_deviceParams->close();
    m_deviceParams = nullptr;
    m_open = false;
}

void USRPMIMO::init()
{
    applySettings(m_settings, false, true);
}

bool USRPMIMO::startRx()
{
    qDebug("USRPMIMO::startRx");

    if (!m_open)
    {
        qCritical("USRPMIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    if (m_rxStream == nullptr)
    {
        try
        {
            uhd::usrp::multi_usrp::sptr usrp = m_deviceParams->getDevice();

            // Apply settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            applySettings(m_settings, true, true);
            usrp->set_rx_bandwidth(56000000, 0);
            usrp->set_rx_bandwidth(56000000, 1);

            // set up the stream
            std::string cpu_format("sc16");
            std::string wire_format("sc16");
            std::vector<size_t> channel_nums;
            channel_nums.push_back(0);
            channel_nums.push_back(1);

            uhd::stream_args_t stream_args(cpu_format, wire_format);
            stream_args.channels = channel_nums;

            m_rxStream = m_deviceParams->getDevice()->get_rx_stream(stream_args);

            // Match our receive buffer size to what UHD uses
            m_rxBufSamples = m_rxStream->get_max_num_samps();

            // Wait for reference and LO to lock
            DeviceUSRP::waitForLock(usrp, m_settings.m_clockSource, 0);
            DeviceUSRP::waitForLock(usrp, m_settings.m_clockSource, 1);

            // Now we can set desired bandwidth
            usrp->set_rx_bandwidth(m_settings.m_rx0LpfBW, 0);
            usrp->set_rx_bandwidth(m_settings.m_rx0LpfBW, 1);
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPInput::acquireChannel: exception: " << e.what();
        }
    }

    m_sourceThread = new USRPMIThread(m_rxStream, m_rxBufSamples);
    m_sampleMIFifo.reset();
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setFcPos(m_settings.m_fcPosRx);
    m_sourceThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_sourceThread->setIQOrder(m_settings.m_iqOrder);
	m_sourceThread->startWork();
	mutexLocker.unlock();
	m_runningRx = true;
    m_rxChannelAcquired = true;

    return true;
}

bool USRPMIMO::startTx()
{
    qDebug("USRPMIMO::startTx");

    if (!m_open)
    {
        qCritical("USRPMIMO::startTx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        stopTx();
    }

    if (m_txStream == nullptr)
    {
        try
        {
            uhd::usrp::multi_usrp::sptr usrp = m_deviceParams->getDevice();

            // Apply settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            applySettings(m_settings, true, true);
            usrp->set_tx_bandwidth(56000000, 0);
            usrp->set_tx_bandwidth(56000000, 1);

            // set up the stream
            std::string cpu_format("sc16");
            std::string wire_format("sc16");
            std::vector<size_t> channel_nums;
            channel_nums.push_back(0);
            channel_nums.push_back(1);

            uhd::stream_args_t stream_args(cpu_format, wire_format);
            stream_args.channels = channel_nums;

            m_txStream = usrp->get_tx_stream(stream_args);

            // Match our transmit buffer size to what UHD uses
            m_txBufSamples = m_txStream->get_max_num_samps();

            // Wait for reference and LO to lock
            DeviceUSRP::waitForLock(usrp, m_settings.m_clockSource, 0);
            DeviceUSRP::waitForLock(usrp, m_settings.m_clockSource, 1);

            // Now we can set desired bandwidth
            usrp->set_tx_bandwidth(m_settings.m_tx0LpfBW, 0);
            usrp->set_tx_bandwidth(m_settings.m_tx1LpfBW, 1);
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPMIMO::startTx: exception: " << e.what();
        }
    }

    m_sinkThread = new USRPMOThread(m_txStream, m_txBufSamples);
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setFcPos(m_settings.m_fcPosTx);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;
    m_txChannelAcquired = true;

    return true;
}

void USRPMIMO::stopRx()
{
    qDebug("USRPMIMO::stopRx");

    if (!m_sourceThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sourceThread->stopWork();
    delete m_sourceThread;
    m_sourceThread = nullptr;
    m_runningRx = false;
    m_rxStream = nullptr;
    m_rxChannelAcquired = false;
}

void USRPMIMO::stopTx()
{
    qDebug("USRPMIMO::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;
    m_txStream = nullptr;
    m_txChannelAcquired = false;
}

QByteArray USRPMIMO::serialize() const
{
    return m_settings.serialize();
}

bool USRPMIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& USRPMIMO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int USRPMIMO::getSourceSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

int USRPMIMO::getSinkSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 USRPMIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void USRPMIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    USRPMIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 USRPMIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void USRPMIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    USRPMIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool USRPMIMO::handleMessage(const Message& message)
{
    if (MsgConfigureUSRPMIMO::match(message))
    {
        MsgConfigureUSRPMIMO& conf = (MsgConfigureUSRPMIMO&) message;
        qDebug() << "USRPMIMO::handleMessage: MsgConfigureUSRPMIMO";

        bool success = applySettings(conf.getSettings(), false, conf.getForce());

        if (!success) {
            qDebug("USRPMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "USRPMIMO::handleMessage: "
            << " " << (cmd.getRxElseTx() ? "Rx" : "Tx")
            << " USRPMIMO: " << (cmd.getStartStop() ? "start" : "stop");

        bool startStopRxElseTx = cmd.getRxElseTx();

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine(startStopRxElseTx ? 0 : 1)) {
                m_deviceAPI->startDeviceEngine(startStopRxElseTx ? 0 : 1);
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine(startStopRxElseTx ? 0 : 1);
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool USRPMIMO::applySettings(const USRPMIMOSettings& settings, bool preGetStream, bool force)
{
    bool forwardChangeRxDSP  = false;
    bool forwardChangeTxDSP  = false;
    bool forwardClockSource  = false;
    bool reapplySomeSettings = false;
    bool checkRates          = false;
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force) {
        reverseAPIKeys.append("devSampleRate");
    }
    if ((m_settings.m_clockSource != settings.m_clockSource) || force) {
        reverseAPIKeys.append("clockSource");
    }
    if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency) || force) {
        reverseAPIKeys.append("rxCenterFrequency");
    }
    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }
    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }
    if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force) {
        reverseAPIKeys.append("log2SoftDecim");
    }
    if ((m_settings.m_fcPosRx != settings.m_fcPosRx) || force) {
        reverseAPIKeys.append("fcPosRx");
    }
    if ((m_settings.m_rxTransverterMode != settings.m_rxTransverterMode) || force) {
        reverseAPIKeys.append("rxTransverterMode");
    }
    if ((m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency) || force) {
        reverseAPIKeys.append("rxTransverterDeltaFrequency");
    }
    if ((m_settings.m_iqOrder != settings.m_iqOrder) || force) {
        reverseAPIKeys.append("iqOrder");
    }
    if ((m_settings.m_rx0LOOffset != settings.m_rx0LOOffset) || force) {
        reverseAPIKeys.append("rx0LOOffset");
    }
    if ((m_settings.m_rx0LpfBW != settings.m_rx0LpfBW) || force) {
        reverseAPIKeys.append("rx0LpfBW");
    }
    if ((m_settings.m_rx0Gain != settings.m_rx0Gain) || force) {
        reverseAPIKeys.append("rx0Gain");
    }
    if ((m_settings.m_rx0AntennaPath != settings.m_rx0AntennaPath) || force) {
        reverseAPIKeys.append("rx0AntennaPath");
    }
    if ((m_settings.m_rx0GainMode != settings.m_rx0GainMode) || force) {
        reverseAPIKeys.append("rx0GainMode");
    }
    if ((m_settings.m_rx1LOOffset != settings.m_rx1LOOffset) || force) {
        reverseAPIKeys.append("rx1LOOffset");
    }
    if ((m_settings.m_rx1LpfBW != settings.m_rx1LpfBW) || force) {
        reverseAPIKeys.append("rx1LpfBW");
    }
    if ((m_settings.m_rx1Gain != settings.m_rx1Gain) || force) {
        reverseAPIKeys.append("rx1Gain");
    }
    if ((m_settings.m_rx1AntennaPath != settings.m_rx1AntennaPath) || force) {
        reverseAPIKeys.append("rx1AntennaPath");
    }
    if ((m_settings.m_rx1GainMode != settings.m_rx1GainMode) || force) {
        reverseAPIKeys.append("rx1GainMode");
    }
    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency) || force) {
        reverseAPIKeys.append("txCenterFrequency");
    }
    if ((m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force) {
        reverseAPIKeys.append("log2SoftInterp");
    }
    if ((m_settings.m_fcPosTx != settings.m_fcPosTx) || force) {
        reverseAPIKeys.append("fcPosTx");
    }
    if ((m_settings.m_txTransverterMode != settings.m_txTransverterMode) || force) {
        reverseAPIKeys.append("txTransverterMode");
    }
    if ((m_settings.m_txTransverterDeltaFrequency != settings.m_txTransverterDeltaFrequency) || force) {
        reverseAPIKeys.append("txTransverterDeltaFrequency");
    }
    if ((m_settings.m_tx0LOOffset != settings.m_tx0LOOffset) || force) {
        reverseAPIKeys.append("tx0LOOffset");
    }
    if ((m_settings.m_tx0LpfBW != settings.m_tx0LpfBW) || force) {
        reverseAPIKeys.append("tx0LpfBW");
    }
    if ((m_settings.m_tx0Gain != settings.m_tx0Gain) || force) {
        reverseAPIKeys.append("tx0Gain");
    }
    if ((m_settings.m_tx0AntennaPath != settings.m_tx0AntennaPath) || force) {
        reverseAPIKeys.append("tx0AntennaPath");
    }
    if ((m_settings.m_tx1LOOffset != settings.m_tx1LOOffset) || force) {
        reverseAPIKeys.append("tx1LOOffset");
    }
    if ((m_settings.m_tx1LpfBW != settings.m_tx1LpfBW) || force) {
        reverseAPIKeys.append("tx1LpfBW");
    }
    if ((m_settings.m_tx1Gain != settings.m_tx1Gain) || force) {
        reverseAPIKeys.append("tx1Gain");
    }
    if ((m_settings.m_tx1AntennaPath != settings.m_tx1AntennaPath) || force) {
        reverseAPIKeys.append("tx1AntennaPath");
    }

    try
    {
        qint64 rxDeviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
            settings.m_rxCenterFrequency,
            settings.m_rxTransverterDeltaFrequency,
            settings.m_log2SoftDecim,
            (DeviceSampleSource::fcPos_t) settings.m_fcPosRx,
            settings.m_devSampleRate,
            DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
            settings.m_rxTransverterMode
        );

        qint64 txDeviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
            settings.m_txCenterFrequency,
            settings.m_txTransverterDeltaFrequency,
            settings.m_log2SoftInterp,
            (DeviceSampleSink::fcPos_t) settings.m_fcPosTx,
            settings.m_devSampleRate,
            settings.m_txTransverterMode
        );

        // apply settings

        if ((m_settings.m_clockSource != settings.m_clockSource) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || m_txChannelAcquired || preGetStream))
            {
                try
                {
                    m_deviceParams->getDevice()->set_clock_source(settings.m_clockSource.toStdString(), 0);
                    forwardClockSource = true;
                    reapplySomeSettings = true;
                    qDebug() << "USRPMIMO::applySettings: clock set to " << settings.m_clockSource;
                }
                catch (std::exception &e)
                {
                    // An exception will be thrown if the clock is not detected
                    // however, get_clock_source called below will still say the clock has is set
                    qCritical() << "USRPMIMO::applySettings: could not set clock " << settings.m_clockSource;
                    // So, default back to internal
                    m_deviceParams->getDevice()->set_clock_source("internal", 0);
                    // notify GUI that source couldn't be set
                    forwardClockSource = true;
                }
            }
            else
            {
                qCritical() << "USRPMIMO::applySettings: could not set clock " << settings.m_clockSource;
            }
        }

        if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
           || (m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
        {

#if defined(_MSC_VER)
            unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp);
            fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
            unsigned int fifoRate = std::max(
                (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftDecim),
                DeviceUSRPShared::m_sampleFifoMinRate);
#endif
            m_sampleMIFifo.resize(SampleSinkFifo::getSizePolicy(fifoRate));
            qDebug("USRPMIMO::applySettings: resize MI FIFO: rate %u", fifoRate);
        }

        if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
           || (m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
        {

#if defined(_MSC_VER)
            unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp);
            fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
            unsigned int fifoRate = std::max(
                (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp),
                DeviceUSRPShared::m_sampleFifoMinRate);
#endif
            m_sampleMOFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
            qDebug("USRPMIMO::applySettings: resize MO FIFO: rate %u", fifoRate);
        }

        if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
        {
            forwardChangeRxDSP = true;
            forwardChangeTxDSP = true;

            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_rate(settings.m_devSampleRate, 0);
                m_deviceParams->getDevice()->set_rx_rate(settings.m_devSampleRate, 1);
                qDebug("USRPMIMO::applySettings: Rx[0,1] sample rate set to %d", settings.m_devSampleRate);
                checkRates = true;
                reapplySomeSettings = true;
            }

            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_tx_rate(settings.m_devSampleRate, 0);
                m_deviceParams->getDevice()->set_tx_rate(settings.m_devSampleRate, 1);
                qDebug("USRPMIMO::applySettings: Tx[0,1] sample rate set to %d", settings.m_devSampleRate);
                checkRates = true;
                reapplySomeSettings = true;
            }
        }

        if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency)
            || (m_settings.m_rx0LOOffset != settings.m_rx0LOOffset)
            || (m_settings.m_rx1LOOffset != settings.m_rx1LOOffset)
            || (m_settings.m_rxTransverterMode != settings.m_rxTransverterMode)
            || (m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency)
            || force)
        {
            forwardChangeRxDSP = true;

            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                if (settings.m_rx0LOOffset != 0)
                {
                    uhd::tune_request_t tune_request(rxDeviceCenterFrequency, settings.m_rx0LOOffset);
                    m_deviceParams->getDevice()->set_rx_freq(tune_request, 0);
                }
                else
                {
                    uhd::tune_request_t tune_request(rxDeviceCenterFrequency);
                    m_deviceParams->getDevice()->set_rx_freq(tune_request, 0);
                }

                qDebug("USRPMIMO::applySettings: Rx frequency channel 0 set to %lld with LO offset %d",
                    rxDeviceCenterFrequency, settings.m_rx0LOOffset);

                if (settings.m_rx1LOOffset != 0)
                {
                    uhd::tune_request_t tune_request(rxDeviceCenterFrequency, settings.m_rx1LOOffset);
                    m_deviceParams->getDevice()->set_rx_freq(tune_request, 1);
                }
                else
                {
                    uhd::tune_request_t tune_request(rxDeviceCenterFrequency);
                    m_deviceParams->getDevice()->set_rx_freq(tune_request, 1);
                }

                qDebug("USRPMIMO::applySettings: Rx frequency channel 1 set to %lld with LO offset %d",
                    rxDeviceCenterFrequency, settings.m_rx1LOOffset);
            }
        }

        if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency)
            || (m_settings.m_tx0LOOffset != settings.m_tx0LOOffset)
            || (m_settings.m_tx1LOOffset != settings.m_tx1LOOffset)
            || (m_settings.m_txTransverterMode != settings.m_txTransverterMode)
            || (m_settings.m_txTransverterDeltaFrequency != settings.m_txTransverterDeltaFrequency)
            || force)
        {
            forwardChangeTxDSP = true;

            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                if (settings.m_tx0LOOffset != 0)
                {
                    uhd::tune_request_t tune_request(txDeviceCenterFrequency, settings.m_tx0LOOffset);
                    m_deviceParams->getDevice()->set_tx_freq(tune_request, 0);
                }
                else
                {
                    uhd::tune_request_t tune_request(txDeviceCenterFrequency);
                    m_deviceParams->getDevice()->set_tx_freq(tune_request, 0);
                }

                qDebug("USRPMIMO::applySettings: Tx frequency channel 0 set to %lld with LO offset %d",
                    txDeviceCenterFrequency, settings.m_tx0LOOffset);
            }

            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                if (settings.m_tx1LOOffset != 0)
                {
                    uhd::tune_request_t tune_request(txDeviceCenterFrequency, settings.m_tx1LOOffset);
                    m_deviceParams->getDevice()->set_tx_freq(tune_request, 1);
                }
                else
                {
                    uhd::tune_request_t tune_request(txDeviceCenterFrequency);
                    m_deviceParams->getDevice()->set_tx_freq(tune_request, 1);
                }

                qDebug("USRPMIMO::applySettings: Tx frequency channel 1 set to %lld with LO offset %d",
                    txDeviceCenterFrequency, settings.m_tx1LOOffset);
            }
        }

        if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_dc_offset(settings.m_dcBlock, 0);
                m_deviceParams->getDevice()->set_rx_dc_offset(settings.m_dcBlock, 1);
            }
        }

        if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_iq_balance(settings.m_iqCorrection, 0);
                m_deviceParams->getDevice()->set_rx_iq_balance(settings.m_iqCorrection, 1);
            }
        }

        if ((m_settings.m_rx0GainMode != settings.m_rx0GainMode) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                if (settings.m_rx0GainMode == USRPMIMOSettings::GAIN_AUTO)
                {
                    m_deviceParams->getDevice()->set_rx_agc(true, 0);
                    qDebug() << "USRPMIMO::applySettings: AGC enabled for Rx channel 0";
                }
                else
                {
                    m_deviceParams->getDevice()->set_rx_agc(false, 0);
                    m_deviceParams->getDevice()->set_rx_gain(settings.m_rx0Gain, 0);
                    qDebug() << "USRPInput::applySettings: AGC disabled for Rx channel 0 set to " << settings.m_rx0Gain;
                }
            }
        }

        if ((m_settings.m_rx1GainMode != settings.m_rx1GainMode) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                if (settings.m_rx1GainMode == USRPMIMOSettings::GAIN_AUTO)
                {
                    m_deviceParams->getDevice()->set_rx_agc(true, 1);
                    qDebug() << "USRPMIMO::applySettings: AGC enabled for Rx channel 1";
                }
                else
                {
                    m_deviceParams->getDevice()->set_rx_agc(false, 1);
                    m_deviceParams->getDevice()->set_rx_gain(settings.m_rx1Gain, 1);
                    qDebug() << "USRPMIMO::applySettings: AGC disabled for Rx channel 1 set to " << settings.m_rx0Gain;
                }
            }
        }

        if ((m_settings.m_rx0Gain != settings.m_rx0Gain) || force)
        {
            if ((settings.m_rx0GainMode != USRPMIMOSettings::GAIN_AUTO) && m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_gain(settings.m_rx0Gain, 0);
                qDebug() << "USRPMIMO::applySettings: Gain set to " << settings.m_rx0Gain << " for Rx channel 0";
            }
        }

        if ((m_settings.m_rx1Gain != settings.m_rx1Gain) || force)
        {
            if ((settings.m_rx1GainMode != USRPMIMOSettings::GAIN_AUTO) && m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_gain(settings.m_rx1Gain, 1);
                qDebug() << "USRPMIMO::applySettings: Gain set to " << settings.m_rx0Gain << " for Rx channel 1";
            }
        }

        if ((m_settings.m_rx0LpfBW != settings.m_rx0LpfBW) || force)
        {
            // Don't set bandwidth before get_rx_stream (See above)
            if (m_deviceParams->getDevice() && m_rxChannelAcquired)
            {
                m_deviceParams->getDevice()->set_rx_bandwidth(settings.m_rx0LpfBW, 0);
                qDebug("USRPMIMO::applySettings: LPF BW: %f for Rx channel 0", settings.m_rx0LpfBW);
            }
        }

        if ((m_settings.m_rx1LpfBW != settings.m_rx1LpfBW) || force)
        {
            // Don't set bandwidth before get_rx_stream (See above)
            if (m_deviceParams->getDevice() && m_rxChannelAcquired)
            {
                m_deviceParams->getDevice()->set_rx_bandwidth(settings.m_rx1LpfBW, 1);
                qDebug("USRPMIMO::applySettings: LPF BW: %f for Rx channel 1", settings.m_rx1LpfBW);
            }
        }

        if ((m_settings.m_tx0Gain != settings.m_tx0Gain) || force)
        {
            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_tx_gain(settings.m_tx0Gain, 0);
                qDebug() << "USRPMIMO::applySettings: Tx channel 0 gain set to " << settings.m_tx0Gain;
            }
        }

        if ((m_settings.m_tx1Gain != settings.m_tx1Gain) || force)
        {
            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_tx_gain(settings.m_tx1Gain, 1);
                qDebug() << "USRPMIMO::applySettings: Tx channel 1 gain set to " << settings.m_tx1Gain;
            }
        }

        if ((m_settings.m_tx0LpfBW != settings.m_tx0LpfBW) || force)
        {
            // Don't set bandwidth before get_tx_stream (See above)
            if (m_deviceParams->getDevice() && m_txChannelAcquired)
            {
                m_deviceParams->getDevice()->set_tx_bandwidth(settings.m_tx0LpfBW, 0);
                qDebug("USRPMIMO::applySettings: LPF BW: %f for Tx channel 0", settings.m_tx0LpfBW);
            }
        }

        if ((m_settings.m_tx1LpfBW != settings.m_tx1LpfBW) || force)
        {
            // Don't set bandwidth before get_tx_stream (See above)
            if (m_deviceParams->getDevice() && m_txChannelAcquired)
            {
                m_deviceParams->getDevice()->set_tx_bandwidth(settings.m_tx1LpfBW, 0);
                qDebug("USRPMIMO::applySettings: LPF BW: %f for Tx channel 1", settings.m_tx1LpfBW);
            }
        }

        if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
        {
            forwardChangeRxDSP = true;

            if (m_sourceThread)
            {
                m_sourceThread->setLog2Decimation(settings.m_log2SoftDecim);
                qDebug() << "USRPMIMO::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
            }
        }

        if ((m_settings.m_fcPosRx != settings.m_fcPosRx) || force)
        {
            forwardChangeRxDSP = true;

            if (m_sourceThread)
            {
                m_sourceThread->setFcPos(settings.m_fcPosRx);
                qDebug() << "USRPMIMO::applySettings: set Rx Fc pos to " << (1<<settings.m_fcPosRx);
            }
        }

        if ((m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
        {
            forwardChangeTxDSP = true;

            if (m_sinkThread)
            {
                m_sinkThread->setLog2Interpolation(settings.m_log2SoftInterp);
                qDebug() << "USRPMIMO::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
            }
        }

        if ((m_settings.m_fcPosTx != settings.m_fcPosTx) || force)
        {
            forwardChangeTxDSP = true;

            if (m_sinkThread)
            {
                m_sinkThread->setFcPos(settings.m_fcPosTx);
                qDebug() << "USRPMIMO::applySettings: set Tx Fc pos to " << (1<<settings.m_fcPosTx);
            }
        }

        if ((m_settings.m_rx0AntennaPath != settings.m_rx0AntennaPath) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_antenna(settings.m_rx0AntennaPath.toStdString(), 0);
                qDebug("USRPMIMO::applySettings: set antenna path to %s on Rx channel 0", qPrintable(settings.m_rx0AntennaPath));
            }
        }

        if ((m_settings.m_rx1AntennaPath != settings.m_rx1AntennaPath) || force)
        {
            if (m_deviceParams->getDevice() && (m_rxChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_rx_antenna(settings.m_rx1AntennaPath.toStdString(), 1);
                qDebug("USRPMIMO::applySettings: set antenna path to %s on Rx channel 1", qPrintable(settings.m_rx1AntennaPath));
            }
        }

        if ((m_settings.m_tx0AntennaPath != settings.m_tx0AntennaPath) || force)
        {
            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_tx_antenna(settings.m_tx0AntennaPath.toStdString(), 0);
                qDebug("USRPOutput::applySettings: set antenna path to %s on Tx channel 0", qPrintable(settings.m_tx0AntennaPath));
            }
        }

        if ((m_settings.m_tx1AntennaPath != settings.m_tx1AntennaPath) || force)
        {
            if (m_deviceParams->getDevice() && (m_txChannelAcquired || preGetStream))
            {
                m_deviceParams->getDevice()->set_tx_antenna(settings.m_tx1AntennaPath.toStdString(), 1);
                qDebug("USRPOutput::applySettings: set antenna path to %s on Tx channel 1", qPrintable(settings.m_tx1AntennaPath));
            }
        }

        if (settings.m_useReverseAPI)
        {
            bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                    (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                    (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                    (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
            webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
        }

        if (reapplySomeSettings)
        {
            // Need to re-set bandwidth and AGG after changing samplerate (and possibly clock source)
            m_deviceParams->getDevice()->set_rx_bandwidth(settings.m_rx0LpfBW, 0);
            m_deviceParams->getDevice()->set_rx_bandwidth(settings.m_rx1LpfBW, 1);

            if (settings.m_rx0GainMode == USRPMIMOSettings::GAIN_AUTO)
            {
                m_deviceParams->getDevice()->set_rx_agc(true, 0);
            }
            else
            {
                m_deviceParams->getDevice()->set_rx_agc(false, 0);
                m_deviceParams->getDevice()->set_rx_gain(settings.m_rx0Gain, 0);
            }

            if (settings.m_rx1GainMode == USRPMIMOSettings::GAIN_AUTO)
            {
                m_deviceParams->getDevice()->set_rx_agc(true, 1);
            }
            else
            {
                m_deviceParams->getDevice()->set_rx_agc(false, 1);
                m_deviceParams->getDevice()->set_rx_gain(settings.m_rx1Gain, 1);
            }
        }

        m_settings = settings;

        if (checkRates)
        {
            // Check if requested rate could actually be met and what master clock rate we ended up with
            double actualSampleRate = m_deviceParams->getDevice()->get_rx_rate(0);
            qDebug("USRPInput::applySettings: actual sample rate %f", actualSampleRate);
            double masterClockRate = m_deviceParams->getDevice()->get_master_clock_rate();
            qDebug("USRPInput::applySettings: master_clock_rate %f", masterClockRate);
            m_settings.m_devSampleRate = actualSampleRate;
            m_settings.m_masterClockRate = masterClockRate;
        }

        // forward changes to buddies or oneself

        if (forwardChangeRxDSP)
        {
            int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2SoftDecim);
            DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 0);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
            DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 1);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
        }

        if (forwardChangeTxDSP)
        {
            int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2SoftInterp);
            DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency, false, 0);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
            DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency, false, 1);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
        }

        if (forwardClockSource)
        {
            // get what clock is actually set, in case requested clock couldn't be set
            if (m_deviceParams->getDevice())
            {
                try
                {
                    // In B210 clock source is the same for all channels
                    m_settings.m_clockSource = QString::fromStdString(m_deviceParams->getDevice()->get_clock_source(0));
                    qDebug() << "USRPMIMO::applySettings: clock source is " << m_settings.m_clockSource;
                }
                catch (std::exception &e)
                {
                    qDebug() << "USRPMIMO::applySettings: could not get clock source";
                }
            }

            // send to GUI in case requested clock isn't detected
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                DeviceUSRPShared::MsgReportClockSourceChange *report = DeviceUSRPShared::MsgReportClockSourceChange::create(
                        m_settings.m_clockSource);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        QLocale loc;

        qDebug().noquote() << "USRPMIMO::applySettings: Rx center freq: " << m_settings.m_rxCenterFrequency << " Hz"
                << " m_rxTransverterMode: " << m_settings.m_rxTransverterMode
                << " m_rxTransverterDeltaFrequency: " << m_settings.m_rxTransverterDeltaFrequency
                << " rxDeviceCenterFrequency: " << rxDeviceCenterFrequency
                << " m_clockSource: " << m_settings.m_clockSource
                << " device stream sample rate: " << loc.toString(m_settings.m_devSampleRate) << "S/s"
                << " sample rate with soft decimation: " << loc.toString( m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim)) << "S/s"
                << " m_log2SoftDecim: " << m_settings.m_log2SoftDecim
                << " m_fcPosRx: " << (int) m_settings.m_fcPosRx
                << " m_rx0GainMode: " << (int) m_settings.m_rx0GainMode
                << " m_rx0Gain: " << m_settings.m_rx0Gain
                << " m_rx0LpfBW: " << loc.toString(static_cast<int>(m_settings.m_rx0LpfBW))
                << " m_rx0AntennaPath: " << m_settings.m_rx0AntennaPath
                << " m_rx1GainMode: " << (int) m_settings.m_rx1GainMode
                << " m_rx1Gain: " << m_settings.m_rx1Gain
                << " m_rx1LpfBW: " << loc.toString(static_cast<int>(m_settings.m_rx1LpfBW))
                << " m_rx1AntennaPath: " << m_settings.m_rx1AntennaPath
                << " Tx center freq: " << m_settings.m_txCenterFrequency << " Hz"
                << " m_txTransverterMode: " << m_settings.m_txTransverterMode
                << " m_txTransverterDeltaFrequency: " << m_settings.m_txTransverterDeltaFrequency
                << " txDeviceCenterFrequency: " << txDeviceCenterFrequency
                << " sample rate with soft interpolation: " << loc.toString( m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp)) << "S/s"
                << " m_log2SoftInterp: " << m_settings.m_log2SoftInterp
                << " m_fcPosTx: " << (int) m_settings.m_fcPosTx
                << " m_tx0Gain: " << m_settings.m_tx0Gain
                << " m_tx0LpfBW: " << loc.toString(static_cast<int>(m_settings.m_tx0LpfBW))
                << " m_tx0AntennaPath: " << m_settings.m_tx0AntennaPath
                << " m_tx1Gain: " << m_settings.m_tx1Gain
                << " m_tx1LpfBW: " << loc.toString(static_cast<int>(m_settings.m_tx1LpfBW))
                << " m_tx1AntennaPath: " << m_settings.m_tx1AntennaPath
                << " force: " << force;

        return true;
    }
    catch (std::exception &e)
    {
        qDebug() << "USRPInput::applySettings: exception: " << e.what();
        return false;
    }
}

int USRPMIMO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpMimoSettings(new SWGSDRangel::SWGUSRPMIMOSettings());
    response.getUsrpMimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int USRPMIMO::webapiSettingsPutPatch(
    bool force,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response, // query + response
    QString& errorMessage)
{
    (void) errorMessage;
    USRPMIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureUSRPMIMO *msg = MsgConfigureUSRPMIMO::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUSRPMIMO *msgToGUI = MsgConfigureUSRPMIMO::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void USRPMIMO::webapiUpdateDeviceSettings(
        USRPMIMOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getUsrpMimoSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("clockSource")) {
        settings.m_clockSource = *response.getUsrpMimoSettings()->getClockSource();
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getUsrpMimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getUsrpMimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getUsrpMimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getUsrpMimoSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        settings.m_fcPosRx = static_cast<USRPMIMOSettings::fcPos_t>(response.getUsrpMimoSettings()->getFcPosRx());
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getUsrpMimoSettings()->getRxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getUsrpMimoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getUsrpMimoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("rx0LOOffset")) {
        settings.m_rx0LOOffset = response.getUsrpMimoSettings()->getRx0LoOffset();
    }
    if (deviceSettingsKeys.contains("rx0LpfBW")) {
        settings.m_rx0LpfBW = response.getUsrpMimoSettings()->getRx0LpfBw();
    }
    if (deviceSettingsKeys.contains("rx0Gain")) {
        settings.m_rx0Gain = response.getUsrpMimoSettings()->getRx0Gain();
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath")) {
        settings.m_rx0AntennaPath = *response.getUsrpMimoSettings()->getRx0AntennaPath();
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        settings.m_rx0GainMode = static_cast<USRPMIMOSettings::GainMode>(response.getUsrpMimoSettings()->getRx0GainMode());
    }
    if (deviceSettingsKeys.contains("rx1LOOffset")) {
        settings.m_rx1LOOffset = response.getUsrpMimoSettings()->getRx1LoOffset();
    }
    if (deviceSettingsKeys.contains("rx1LpfBW")) {
        settings.m_rx1LpfBW = response.getUsrpMimoSettings()->getRx1LpfBw();
    }
    if (deviceSettingsKeys.contains("rx1Gain")) {
        settings.m_rx1Gain = response.getUsrpMimoSettings()->getRx1Gain();
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath")) {
        settings.m_rx1AntennaPath = *response.getUsrpMimoSettings()->getRx1AntennaPath();
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        settings.m_rx1GainMode = static_cast<USRPMIMOSettings::GainMode>(response.getUsrpMimoSettings()->getRx1GainMode());
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getUsrpMimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getUsrpMimoSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        settings.m_fcPosTx = static_cast<USRPMIMOSettings::fcPos_t>(response.getUsrpMimoSettings()->getFcPosTx());
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getUsrpMimoSettings()->getTxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getUsrpMimoSettings()->getTxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("tx0LOOffset")) {
        settings.m_tx0LOOffset = response.getUsrpMimoSettings()->getTx0LoOffset();
    }
    if (deviceSettingsKeys.contains("m_tx0LpfBW")) {
        settings.m_tx0LpfBW = response.getUsrpMimoSettings()->getTx0LpfBw();
    }
    if (deviceSettingsKeys.contains("tx0Gain")) {
        settings.m_tx0Gain = response.getUsrpMimoSettings()->getTx0Gain();
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath")) {
        settings.m_tx0AntennaPath = *response.getUsrpMimoSettings()->getTx0AntennaPath();
    }
    if (deviceSettingsKeys.contains("tx1LOOffset")) {
        settings.m_tx1LOOffset = response.getUsrpMimoSettings()->getTx1LoOffset();
    }
    if (deviceSettingsKeys.contains("m_tx1LpfBW")) {
        settings.m_tx1LpfBW = response.getUsrpMimoSettings()->getTx1LpfBw();
    }
    if (deviceSettingsKeys.contains("tx1Gain")) {
        settings.m_tx1Gain = response.getUsrpMimoSettings()->getTx1Gain();
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath")) {
        settings.m_tx1AntennaPath = *response.getUsrpMimoSettings()->getTx1AntennaPath();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf2MimoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf2MimoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf2MimoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf2MimoSettings()->getReverseApiDeviceIndex();
    }
}

void USRPMIMO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const USRPMIMOSettings& settings)
{
    response.getUsrpMimoSettings()->setDevSampleRate(settings.m_devSampleRate);

    if (response.getUsrpMimoSettings()->getClockSource()) {
        *response.getUsrpMimoSettings()->getClockSource() = settings.m_clockSource;
    } else {
        response.getUsrpMimoSettings()->setClockSource(new QString(settings.m_clockSource));
    }

    response.getUsrpMimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getUsrpMimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getUsrpMimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getUsrpMimoSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getUsrpMimoSettings()->setFcPosRx((int) settings.m_fcPosRx);
    response.getUsrpMimoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    response.getUsrpMimoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getUsrpMimoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);

    response.getUsrpMimoSettings()->setRx0LoOffset(settings.m_rx0LOOffset);
    response.getUsrpMimoSettings()->setRx0LpfBw(settings.m_rx0LpfBW);
    response.getUsrpMimoSettings()->setRx0Gain(settings.m_rx0Gain);

    if (response.getUsrpMimoSettings()->getRx0AntennaPath()) {
        *response.getUsrpMimoSettings()->getRx0AntennaPath() = settings.m_rx0AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setRx0AntennaPath(new QString(settings.m_rx0AntennaPath));
    }

    response.getUsrpMimoSettings()->setRx0GainMode((int) settings.m_rx0GainMode);

    response.getUsrpMimoSettings()->setRx1LoOffset(settings.m_rx1LOOffset);
    response.getUsrpMimoSettings()->setRx1LpfBw(settings.m_rx1LpfBW);
    response.getUsrpMimoSettings()->setRx1Gain(settings.m_rx1Gain);

    if (response.getUsrpMimoSettings()->getRx1AntennaPath()) {
        *response.getUsrpMimoSettings()->getRx1AntennaPath() = settings.m_rx1AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setRx1AntennaPath(new QString(settings.m_rx1AntennaPath));
    }

    response.getUsrpMimoSettings()->setRx1GainMode((int) settings.m_rx1GainMode);

    response.getUsrpMimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getUsrpMimoSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getUsrpMimoSettings()->setFcPosTx((int) settings.m_fcPosTx);
    response.getUsrpMimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    response.getUsrpMimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);

    response.getUsrpMimoSettings()->setTx0LoOffset(settings.m_tx0LOOffset);
    response.getUsrpMimoSettings()->setTx0LpfBw(settings.m_tx0LpfBW);
    response.getUsrpMimoSettings()->setTx0Gain(settings.m_tx0Gain);

    if (response.getUsrpMimoSettings()->getTx0AntennaPath()) {
        *response.getUsrpMimoSettings()->getTx0AntennaPath() = settings.m_tx0AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setTx0AntennaPath(new QString(settings.m_tx0AntennaPath));
    }

    response.getUsrpMimoSettings()->setTx1LoOffset(settings.m_tx1LOOffset);
    response.getUsrpMimoSettings()->setTx1LpfBw(settings.m_tx1LpfBW);
    response.getUsrpMimoSettings()->setTx1Gain(settings.m_tx1Gain);

    if (response.getUsrpMimoSettings()->getTx1AntennaPath()) {
        *response.getUsrpMimoSettings()->getTx1AntennaPath() = settings.m_tx1AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setTx1AntennaPath(new QString(settings.m_tx1AntennaPath));
    }

    response.getUsrpMimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUsrpMimoSettings()->getReverseApiAddress()) {
        *response.getUsrpMimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUsrpMimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUsrpMimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUsrpMimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int USRPMIMO::webapiRunGet(
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if ((subsystemIndex == 0) || (subsystemIndex == 1))
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState(), subsystemIndex);
        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem invalid: must be 0 (Rx) or 1 (Tx)");
        return 404;
    }
}

int USRPMIMO::webapiRun(
        bool run,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if ((subsystemIndex == 0) || (subsystemIndex == 1))
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState(), subsystemIndex);
        MsgStartStop *message = MsgStartStop::create(run, subsystemIndex == 0);
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgStartStop *msgToGUI = MsgStartStop::create(run, subsystemIndex == 0);
            m_guiMessageQueue->push(msgToGUI);
        }

        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem invalid: must be 0 (Rx) or 1 (Tx)");
        return 404;
    }
}

void USRPMIMO::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const USRPMIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));
    swgDeviceSettings->setUsrpMimoSettings(new SWGSDRangel::SWGUSRPMIMOSettings());
    SWGSDRangel::SWGUSRPMIMOSettings *swgUSRPMIMOSettings = swgDeviceSettings->getUsrpMimoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgUSRPMIMOSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("clockSource")) {
        swgUSRPMIMOSettings->setClockSource(new QString(settings.m_clockSource));
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        swgUSRPMIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        swgUSRPMIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        swgUSRPMIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        swgUSRPMIMOSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        swgUSRPMIMOSettings->setFcPosRx((int) settings.m_fcPosRx);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        swgUSRPMIMOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        swgUSRPMIMOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        swgUSRPMIMOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rx0LOOffset")) {
        swgUSRPMIMOSettings->setRx0LoOffset(settings.m_rx0LOOffset);
    }
    if (deviceSettingsKeys.contains("rx0LpfBW")) {
        swgUSRPMIMOSettings->setRx0LpfBw(settings.m_rx0LpfBW);
    }
    if (deviceSettingsKeys.contains("rx0Gain")) {
        swgUSRPMIMOSettings->setRx0Gain(settings.m_rx0Gain);
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        swgUSRPMIMOSettings->setRx0GainMode((int) settings.m_rx0GainMode);
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath")) {
        swgUSRPMIMOSettings->setRx0AntennaPath(new QString(settings.m_rx0AntennaPath));
    }
    if (deviceSettingsKeys.contains("rx1LOOffset")) {
        swgUSRPMIMOSettings->setRx0LoOffset(settings.m_rx0LOOffset);
    }
    if (deviceSettingsKeys.contains("rx1LpfBW")) {
        swgUSRPMIMOSettings->setRx1LpfBw(settings.m_rx1LpfBW);
    }
    if (deviceSettingsKeys.contains("rx1Gain")) {
        swgUSRPMIMOSettings->setRx1Gain(settings.m_rx1Gain);
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        swgUSRPMIMOSettings->setRx1GainMode((int) settings.m_rx1GainMode);
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath")) {
        swgUSRPMIMOSettings->setRx1AntennaPath(new QString(settings.m_rx1AntennaPath));
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        swgUSRPMIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        swgUSRPMIMOSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        swgUSRPMIMOSettings->setFcPosTx((int) settings.m_fcPosTx);
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        swgUSRPMIMOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        swgUSRPMIMOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("tx0LOOffset")) {
        swgUSRPMIMOSettings->setTx0LoOffset(settings.m_tx0LOOffset);
    }
    if (deviceSettingsKeys.contains("tx0LpfBW")) {
        swgUSRPMIMOSettings->setTx0LpfBw(settings.m_tx0LpfBW);
    }
    if (deviceSettingsKeys.contains("tx0Gain")) {
        swgUSRPMIMOSettings->setTx0Gain(settings.m_tx0Gain);
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath")) {
        swgUSRPMIMOSettings->setTx0AntennaPath(new QString(settings.m_tx0AntennaPath));
    }
    if (deviceSettingsKeys.contains("tx1LOOffset")) {
        swgUSRPMIMOSettings->setTx1LoOffset(settings.m_tx1LOOffset);
    }
    if (deviceSettingsKeys.contains("tx1LpfBW")) {
        swgUSRPMIMOSettings->setTx1LpfBw(settings.m_tx1LpfBW);
    }
    if (deviceSettingsKeys.contains("tx1Gain")) {
        swgUSRPMIMOSettings->setTx1Gain(settings.m_tx1Gain);
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath")) {
        swgUSRPMIMOSettings->setTx1AntennaPath(new QString(settings.m_tx1AntennaPath));
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void USRPMIMO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

int USRPMIMO::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpMimoReport(new SWGSDRangel::SWGUSRPMIMOReport());
    response.getUsrpMimoReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void USRPMIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    bool active = false;
    quint32 overflows = 0;
    quint32 timeouts = 0;

    if (m_rxStream && m_sourceThread && m_rxChannelAcquired)
    {
        m_sourceThread->getStreamStatus(active, overflows, timeouts);
        success = true;
    }

    response.getUsrpMimoReport()->setRxSuccess(success ? 1 : 0);
    response.getUsrpMimoReport()->setRxStreamActive(active ? 1 : 0);
    response.getUsrpMimoReport()->setRxOverrunCount(overflows);
    response.getUsrpMimoReport()->setRxTimeoutCount(timeouts);

    success = false;
    active = false;
    quint32 underflows = 0;
    quint32 droppedPackets = 0;

    if (m_txStream && m_sinkThread && m_txChannelAcquired)
    {
        m_sinkThread->getStreamStatus(active, underflows, droppedPackets);
        success = true;
    }

    response.getUsrpMimoReport()->setTxSuccess(success ? 1 : 0);
    response.getUsrpMimoReport()->setTxStreamActive(active ? 1 : 0);
    response.getUsrpMimoReport()->setTxUnderrunCount(underflows);
    response.getUsrpMimoReport()->setTxDroppedPacketsCount(droppedPackets);
}

void USRPMIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "USRPMIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("USRPMIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
