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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOGUI_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "usrpmimosettings.h"

class DeviceUISet;
class USRPMIMO;

namespace Ui {
    class USRPMIMOGUI;
}

class USRPMIMOGUI : public DeviceGUI {
    Q_OBJECT

public:
    explicit USRPMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
    virtual ~USRPMIMOGUI();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::USRPMIMOGUI* ui;

    DeviceUISet* m_deviceUISet;
    USRPMIMO* m_usrpMIMO; //!< Same object as above but gives easy access to USRPMIMO methods and attributes that are used intensively
    USRPMIMOSettings m_settings;
    bool m_rxElseTx;   //!< Which side is being dealt with
    int m_streamIndex; //!< Current stream index being dealt with
	bool m_spectrumRxElseTx;
    int m_spectrumStreamIndex; //!< Index of the stream displayed on main spectrum
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    //  int m_sampleRate;
    int m_rxBasebandSampleRate;
    int m_txBasebandSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in device
    int m_rxLastEngineState;
    int m_txLastEngineState;
    bool m_doApplySettings;
    bool m_forceSettings;
    int m_statusCounter;
    int m_deviceStatusCounter;
    MessageQueue m_inputMessageQueue;

    void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
    void setCenterFrequencyDisplay();
    void setRxCenterFrequencySetting(uint64_t kHzValue);
    void setTxCenterFrequencySetting(uint64_t kHzValue);
    void sendSettings(bool forceSettings = false);
    void updateSampleRateAndFrequency();
    void updateSampleRate();
    void updateFrequencyLimits();
    void setSampleRateLimits();
    void blockApplySettings(bool block);

private slots:
    void handleInputMessages();
	void on_streamSide_currentIndexChanged(int index);
	void on_streamIndex_currentIndexChanged(int index);
	void on_spectrumSide_currentIndexChanged(int index);
	void on_spectrumIndex_currentIndexChanged(int index);
	void on_startStopRx_toggled(bool checked);
	void on_startStopTx_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_antenna_currentIndexChanged(int index);
    void on_transverter_clicked();
    void on_clockSource_currentIndexChanged(int index);
    void on_sampleRateMode_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_swDecimInterp_currentIndexChanged(int index);
    void on_rxGainMode_currentIndexChanged(int index);
    void on_rxGain_valueChanged(int value);
    void on_txGain_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_lpf_changed(quint64 value);
    void on_loOffset_changed(qint64 value);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateHardware();
    void updateStatus();
};

#endif /* PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOGUI_H_ */
