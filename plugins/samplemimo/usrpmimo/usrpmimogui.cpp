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
#include <QMessageBox>
#include <QFileDialog>

#include <algorithm>

#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "usrpmimo.h"
#include "ui_usrpmimogui.h"
#include "usrpmimogui.h"

USRPMIMOGUI::USRPMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::USRPMIMOGUI),
    m_deviceUISet(deviceUISet),
    m_settings(),
    m_rxElseTx(true),
    m_streamIndex(0),
    m_spectrumRxElseTx(true),
    m_spectrumStreamIndex(0),
    m_sampleRateMode(true),
    // m_sampleRate(0),
    m_rxBasebandSampleRate(3072000),
    m_txBasebandSampleRate(3072000),
    m_rxDeviceCenterFrequency(435000*1000),
    m_txDeviceCenterFrequency(435000*1000),
    m_rxLastEngineState(DeviceAPI::StNotStarted),
    m_txLastEngineState(DeviceAPI::StNotStarted),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_statusCounter(0),
    m_deviceStatusCounter(0)
{
    m_usrpMIMO = (USRPMIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    ui->setupUi(this);

    float minF, maxF;

    m_usrpMIMO->getRxLORange(minF, maxF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_usrpMIMO->getSRRange(minF, maxF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    ui->loOffset->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->loOffset->setValueRange(false, 5, (int32_t)-maxF/2/1000, (int32_t)maxF/2/1000); // LO offset shouldn't be greater than half the sample rate

    m_usrpMIMO->getRxLPRange(minF, maxF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(5, (minF/1000)+1, maxF/1000);

    m_usrpMIMO->getRxGainRange(minF, maxF);
    ui->rxGain->setRange((int)minF, (int)maxF);

    m_usrpMIMO->getTxGainRange(minF, maxF);
    ui->txGain->setRange((int)minF, (int)maxF);

    ui->antenna->addItems(m_usrpMIMO->getRxAntennas());
    ui->clockSource->addItems(m_usrpMIMO->getClockSources());

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_usrpMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    CRightClickEnabler *startStopRightClickEnabler = new CRightClickEnabler(ui->startStopRx);
    connect(startStopRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
}

USRPMIMOGUI::~USRPMIMOGUI()
{
    delete ui;
}

void USRPMIMOGUI::destroy()
{
    delete this;
}

void USRPMIMOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

QByteArray USRPMIMOGUI::serialize() const
{
    return m_settings.serialize();
}

bool USRPMIMOGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void USRPMIMOGUI::displaySettings()
{
    if (m_rxElseTx)
    {
        ui->transverter->setDeltaFrequency(m_settings.m_rxTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_rxTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);
        updateFrequencyLimits();
        ui->centerFrequency->setValue(m_settings.m_rxCenterFrequency / 1000);
        displaySampleRate();

        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

        ui->swDecimInterp->setCurrentIndex(m_settings.m_log2SoftDecim);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosRx);

        ui->rxGainMode->setEnabled(true);
        ui->rxGain->setEnabled(true);
        ui->txGain->setEnabled(false);

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_rx0LpfBW / 1000);
            ui->rxGainMode->setCurrentIndex((int) m_settings.m_rx0GainMode);
            ui->rxGain->setValue(m_settings.m_rx0Gain);
            ui->rxGainText->setText(tr("%1").arg(m_settings.m_rx0Gain));
            ui->antenna->setCurrentIndex(ui->antenna->findText(m_settings.m_rx0AntennaPath));
        }
        else
        {
            ui->lpf->setValue(m_settings.m_rx1LpfBW / 1000);
            ui->rxGainMode->setCurrentIndex((int) m_settings.m_rx1GainMode);
            ui->rxGain->setValue(m_settings.m_rx1Gain);
            ui->rxGainText->setText(tr("%1").arg(m_settings.m_rx1Gain));
            ui->antenna->setCurrentIndex(ui->antenna->findText(m_settings.m_rx1AntennaPath));
        }
    }
    else
    {
        ui->transverter->setDeltaFrequency(m_settings.m_txTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_txTransverterMode);
        updateFrequencyLimits();
        ui->centerFrequency->setValue(m_settings.m_txCenterFrequency / 1000);
        displaySampleRate();

        ui->swDecimInterp->setCurrentIndex(m_settings.m_log2SoftInterp);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosTx);

        ui->rxGainMode->setEnabled(false);
        ui->rxGain->setEnabled(false);
        ui->txGain->setEnabled(true);

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_tx0LpfBW / 1000);
            ui->txGain->setValue(m_settings.m_tx0Gain);
            ui->txGainText->setText(tr("%1dB").arg(m_settings.m_tx0Gain));
            ui->antenna->setCurrentIndex(ui->antenna->findText(m_settings.m_tx0AntennaPath));
        }
        else
        {
            ui->lpf->setValue(m_settings.m_tx1LpfBW / 1000);
            ui->txGain->setValue(m_settings.m_tx1Gain);
            ui->txGainText->setText(tr("%1dB").arg(m_settings.m_tx1Gain));
            ui->antenna->setCurrentIndex(ui->antenna->findText(m_settings.m_tx1AntennaPath));
        }
    }
}

void USRPMIMOGUI::displaySampleRate()
{
    float minF, maxF;
    m_usrpMIMO->getSRRange(minF, maxF);

    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<m_settings.m_log2SoftInterp), (uint32_t) maxF/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void USRPMIMOGUI::displayFcTooltip()
{
    if (m_rxElseTx)
    {
        int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
            m_settings.m_log2SoftDecim,
            (DeviceSampleSource::fcPos_t) m_settings.m_fcPosRx,
            m_settings.m_devSampleRate,
            DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
        );
        ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
    }
    else
    {
        int32_t fShift = DeviceSampleSink::calculateFrequencyShift(
            m_settings.m_log2SoftInterp,
            (DeviceSampleSink::fcPos_t) m_settings.m_fcPosTx,
            m_settings.m_devSampleRate
        );
        ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
    }
}

void USRPMIMOGUI::sendSettings(bool forceSettings)
{
    m_forceSettings = forceSettings;
    if(!m_updateTimer.isActive()) { m_updateTimer.start(100); }
}

void USRPMIMOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "PlutoSDRMIMOGUI::updateHardware";
        USRPMIMO::MsgConfigureUSRPMIMO* message = USRPMIMO::MsgConfigureUSRPMIMO::create(m_settings, m_forceSettings);
        m_usrpMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void USRPMIMOGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void USRPMIMOGUI::updateSampleRateAndFrequency()
{
    if (m_rxElseTx)
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim));
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_rxDeviceCenterFrequency);
    }
    else
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp));
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_txDeviceCenterFrequency);
    }

    displaySampleRate();
}

void USRPMIMOGUI::updateSampleRate()
{
    uint32_t sr = m_settings.m_devSampleRate;
    int cr = m_settings.m_masterClockRate;

    if (sr < 100000000) {
        ui->sampleRateLabel->setText(tr("%1k").arg(QString::number(sr / 1000.0f, 'g', 5)));
    } else {
        ui->sampleRateLabel->setText(tr("%1M").arg(QString::number(sr / 1000000.0f, 'g', 5)));
    }
    if (cr < 0) {
       ui->masterClockRateLabel->setText("-");
    } else if (cr < 100000000) {
        ui->masterClockRateLabel->setText(tr("%1k").arg(QString::number(cr / 1000.0f, 'g', 5)));
    } else {
        ui->masterClockRateLabel->setText(tr("%1M").arg(QString::number(cr / 1000000.0f, 'g', 5)));
    }

    // LO offset shouldn't be greater than half the sample rate
    ui->loOffset->setValueRange(false, 5, -(int32_t)sr/2/1000, (int32_t)sr/2/1000);
}

void USRPMIMOGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency;

    if (m_rxElseTx)
    {
        deltaFrequency = m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency/1000 : 0;
        m_usrpMIMO->getRxLORange(minF, maxF);
    }
    else
    {
        deltaFrequency = m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency/1000 : 0;
        m_usrpMIMO->getTxLORange(minF, maxF);
    }

    qint64 minLimit = minF/1000 + deltaFrequency;
    qint64 maxLimit = maxF/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("USRPMIMOGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

bool USRPMIMOGUI::handleMessage(const Message& message)
{
    if (DSPMIMOSignalNotification::match(message))
    {
        const DSPMIMOSignalNotification& notif = (const DSPMIMOSignalNotification&) message;
        int istream = notif.getIndex();
        bool sourceOrSink = notif.getSourceOrSink();

        if (sourceOrSink)
        {
            m_rxBasebandSampleRate = notif.getSampleRate();
            m_rxDeviceCenterFrequency = notif.getCenterFrequency();
        }
        else
        {
            m_txBasebandSampleRate = notif.getSampleRate();
            m_txDeviceCenterFrequency = notif.getCenterFrequency();
        }

        qDebug("USRPMIMOGUI::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (USRPMIMO::MsgConfigureUSRPMIMO::match(message))
    {
        const USRPMIMO::MsgConfigureUSRPMIMO& notif = (const USRPMIMO::MsgConfigureUSRPMIMO&) message;
        m_settings = notif.getSettings();
        displaySettings();

        return true;
    }

    return false;
}

void USRPMIMOGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        } else {
            qDebug("USRPMIMOGUI::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }
    }
}

void USRPMIMOGUI::updateStatus()
{
    int stateRx = m_deviceUISet->m_deviceAPI->state(0);
    int stateTx = m_deviceUISet->m_deviceAPI->state(1);

    if (m_rxLastEngineState != stateRx)
    {
        switch(stateRx)
        {
            case DeviceAPI::StNotStarted:
                ui->startStopRx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_rxLastEngineState = stateRx;
    }

    if (m_txLastEngineState != stateTx)
    {
        switch(stateTx)
        {
            case DeviceAPI::StNotStarted:
                ui->startStopTx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_txLastEngineState = stateTx;
    }

    // 500 ms

    USRPMIMO::MsgGetStreamInfo* message = USRPMIMO::MsgGetStreamInfo::create();
    m_usrpMIMO->getInputMessageQueue()->push(message);

    if (m_statusCounter % 10 == 0) // 10s
    {
        USRPMIMO::MsgGetDeviceInfo* message = USRPMIMO::MsgGetDeviceInfo::create();
        m_usrpMIMO->getInputMessageQueue()->push(message);
    }

    m_statusCounter++;
}

void USRPMIMOGUI::setRxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_rxCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void USRPMIMOGUI::setTxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_txCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void USRPMIMOGUI::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    displaySettings();
}

void USRPMIMOGUI::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    displaySettings();
}

void USRPMIMOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
}

void USRPMIMOGUI::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void USRPMIMOGUI::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        USRPMIMO::MsgStartStop *message = USRPMIMO::MsgStartStop::create(checked, true);
        m_usrpMIMO->getInputMessageQueue()->push(message);
    }
}

void USRPMIMOGUI::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        USRPMIMO::MsgStartStop *message = USRPMIMO::MsgStartStop::create(checked, false);
        m_usrpMIMO->getInputMessageQueue()->push(message);
    }
}

void USRPMIMOGUI::on_centerFrequency_changed(quint64 value)
{
    if (m_rxElseTx) {
        m_settings.m_rxCenterFrequency = value * 1000;
    } else {
        m_settings.m_txCenterFrequency = value * 1000;
    }

    sendSettings();
}

void USRPMIMOGUI::on_antenna_currentIndexChanged(int index)
{
    (void) index;

    if (m_rxElseTx)
    {
        if (m_streamIndex == 0) {
            m_settings.m_rx0AntennaPath = ui->antenna->currentText();
        } else {
            m_settings.m_rx1AntennaPath = ui->antenna->currentText();
        }
    }
    else
    {
        if (m_streamIndex == 0) {
            m_settings.m_tx0AntennaPath = ui->antenna->currentText();
        } else {
            m_settings.m_tx1AntennaPath = ui->antenna->currentText();
        }
    }

    sendSettings();
}

void USRPMIMOGUI::on_transverter_clicked()
{
    if (m_rxElseTx)
    {
        m_settings.m_rxTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_rxTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        qDebug("m_rxElseTx::on_transverter_clicked: Rx %lld Hz %s",
            m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }
    else
    {
        m_settings.m_rxTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_rxTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        qDebug("m_rxElseTx::on_transverter_clicked: Rx %lld Hz %s",
            m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }

    updateFrequencyLimits();

    if (m_rxElseTx) {
        setRxCenterFrequencySetting(ui->centerFrequency->getValueNew());
    } else {
        setTxCenterFrequencySetting(ui->centerFrequency->getValueNew());
    }

    sendSettings();
}

void USRPMIMOGUI::on_clockSource_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_clockSource = ui->clockSource->currentText();
    sendSettings();
}

void USRPMIMOGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void USRPMIMOGUI::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode)
    {
        m_settings.m_devSampleRate = value;
    }
    else
    {
        if (m_rxElseTx) {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftDecim);
        } else {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftInterp);
        }
    }

    updateSampleRate();
    sendSettings();
}

void USRPMIMOGUI::on_swDecimInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2SoftDecim = index;
        displaySampleRate();

        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftDecim);
        }
    }
    else
    {
        m_settings.m_log2SoftInterp = index;
        displaySampleRate();

        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftInterp);
        }
    }

    sendSettings();
}

void USRPMIMOGUI::on_rxGainMode_currentIndexChanged(int index)
{
    if (m_streamIndex == 0) {
        m_settings.m_rx0GainMode = (USRPMIMOSettings::GainMode) index;
    } else {
        m_settings.m_rx1GainMode = (USRPMIMOSettings::GainMode) index;
    }

    if (index == 0) {
        ui->rxGain->setEnabled(false);
    } else {
        ui->rxGain->setEnabled(true);
    }

    sendSettings();
}

void USRPMIMOGUI::on_rxGain_valueChanged(int value)
{
    if (m_streamIndex == 0) {
        m_settings.m_rx0Gain = value;
    } else {
        m_settings.m_rx1Gain = value;
    }

    ui->rxGainText->setText(tr("%1dB").arg(value));
    sendSettings();
}

void USRPMIMOGUI::on_txGain_valueChanged(int value)
{
    if (m_streamIndex == 0) {
        m_settings.m_tx0Gain = value;
    } else {
        m_settings.m_tx0Gain = value;
    }

    ui->txGainText->setText(tr("%1dB").arg(value));
    sendSettings();
}

void USRPMIMOGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void USRPMIMOGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void USRPMIMOGUI::on_lpf_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0) {
            m_settings.m_rx0LpfBW = value * 1000;
        } else {
            m_settings.m_rx1LpfBW = value * 1000;
        }
    }
    else
    {
        if (m_streamIndex == 0) {
            m_settings.m_tx0LpfBW = value * 1000;
        } else {
            m_settings.m_tx1LpfBW = value * 1000;
        }
    }

    sendSettings();
}

void USRPMIMOGUI::on_loOffset_changed(qint64 value)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0) {
            m_settings.m_rx0LOOffset = value * 1000;
        } else {
            m_settings.m_rx1LOOffset = value * 1000;
        }
    }
    else
    {
        if (m_streamIndex == 0) {
            m_settings.m_tx0LOOffset = value * 1000;
        } else {
            m_settings.m_tx1LOOffset = value * 1000;
        }
    }

    sendSettings();
}

void USRPMIMOGUI::openDeviceSettingsDialog(const QPoint& p)
{
    BasicDeviceSettingsDialog dialog(this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

    dialog.move(p);
    dialog.exec();

    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

    sendSettings();
}
