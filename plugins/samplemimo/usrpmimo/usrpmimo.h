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

#ifndef _USRO_USRPMIMO_H_
#define _USRO_USRPMIMO_H_

#include <stdint.h>
#include <uhd/usrp/multi_usrp.hpp>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplemimo.h"

#include "usrpmimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class USRPMIThread;
class USRPMOThread;
class DeviceUSRPParams;

class USRPMIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureUSRPMIMO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const USRPMIMOSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureUSRPMIMO* create(const USRPMIMOSettings& settings, bool force)
		{
			return new MsgConfigureUSRPMIMO(settings, force);
		}

	private:
		USRPMIMOSettings m_settings;
		bool m_force;

		MsgConfigureUSRPMIMO(const USRPMIMOSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgStartStop* create(bool startStop, bool rxElseTx) {
            return new MsgStartStop(startStop, rxElseTx);
        }

    protected:
        bool m_startStop;
        bool m_rxElseTx;

        MsgStartStop(bool startStop, bool rxElseTx) :
            Message(),
            m_startStop(startStop),
            m_rxElseTx(rxElseTx)
        { }
    };

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetStreamInfo* create()
        {
            return new MsgGetStreamInfo();
        }

    private:
        MsgGetStreamInfo() :
            Message()
        { }
    };

    class MsgGetDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetDeviceInfo* create()
        {
            return new MsgGetDeviceInfo();
        }

    private:
        MsgGetDeviceInfo() :
            Message()
        { }
    };

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool     getRxSuccess() const { return m_rxSuccess; }
        bool     getRxActive() const { return m_rxActive; }
        uint32_t getRxOverruns() const { return m_rxOverruns; }
        uint32_t getRxTimeouts() const { return m_rxTimeouts; }
        bool     getTxSuccess() const { return m_txSuccess; }
        bool     getTxActive() const { return m_txActive; }
        uint32_t getTxUnderrun() const { return m_txUnderrun; }
        uint32_t getTxDroppedPackets() const { return m_txDroppedPackets; }

        static MsgReportStreamInfo* create(
            bool     rxSuccess,
            bool     rxActive,
            uint32_t rxOverruns,
            uint32_t rxTimeouts,
            bool     txSuccess,
            bool     txActive,
            uint32_t txUnderrun,
            uint32_t txDroppedPackets
        )
        {
            return new MsgReportStreamInfo(
                rxSuccess,
                rxActive,
                rxOverruns,
                rxTimeouts,
                txSuccess,
                txActive,
                txUnderrun,
                txDroppedPackets
            );
        }

    private:
        bool     m_rxSuccess;
        bool     m_rxActive;
        uint32_t m_rxOverruns; //!< FIFO overrun count
        uint32_t m_rxTimeouts; //!< Number of dropped packets by HW
        bool     m_txSuccess;
        bool     m_txActive;              //!< Indicates whether the stream is currently active
        uint32_t m_txUnderrun;            //!< FIFO underrun count
        uint32_t m_txDroppedPackets;      //!< Number of dropped packets by HW


        MsgReportStreamInfo(
            bool     rxSuccess,
            bool     rxActive,
            uint32_t rxOverruns,
            uint32_t rxTimeouts,
            bool     txSuccess,
            bool     txActive,
            uint32_t txUnderrun,
            uint32_t txDroppedPackets
        ) :
            Message(),
            m_rxSuccess(rxSuccess),
            m_rxActive(rxActive),
            m_rxOverruns(rxOverruns),
            m_rxTimeouts(rxTimeouts),
            m_txSuccess(txSuccess),
            m_txActive(txActive),
            m_txUnderrun(txUnderrun),
            m_txDroppedPackets(txDroppedPackets)
        { }
    };

    USRPMIMO(DeviceAPI *deviceAPI);
	virtual ~USRPMIMO();
	virtual void destroy();

	virtual void init();
	virtual bool startRx();
	virtual void stopRx();
	virtual bool startTx();
	virtual void stopTx();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;

	virtual int getSourceSampleRate(int index) const;
    virtual void setSourceSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSourceCenterFrequency(int index) const;
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index);

	virtual int getSinkSampleRate(int index) const;
    virtual void setSinkSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSinkCenterFrequency(int index) const;
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index);

    virtual quint64 getMIMOCenterFrequency() const { return getSourceCenterFrequency(0); }
    virtual unsigned int getMIMOSampleRate() const { return getSourceSampleRate(0); }

	virtual bool handleMessage(const Message& message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

    virtual int webapiRunGet(
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const USRPMIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            USRPMIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    int getNbRx() { return m_nbRx; }
    int getNbTx() { return m_nbTx; }
    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

    void getSRRange(float& minF, float& maxF) const;
    QStringList getClockSources() const;

    std::size_t getRxChannelIndex();
    void getRxLORange(float& minF, float& maxF) const;
    void getRxLPRange(float& minF, float& maxF) const;
    void getRxGainRange(float& minF, float& maxF) const;
    QStringList getRxAntennas() const;
    QStringList getRxGainNames() const;

    std::size_t getTxChannelIndex();
    void getTxLORange(float& minF, float& maxF) const;
    void getTxLPRange(float& minF, float& maxF) const;
    void getTxGainRange(float& minF, float& maxF) const;
    QStringList getTxAntennas() const;

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
    DeviceUSRPParams *m_deviceParams;
    uhd::rx_streamer::sptr m_rxStream;
    uhd::tx_streamer::sptr m_txStream;
    size_t m_rxBufSamples;
    size_t m_txBufSamples;
    bool m_rxChannelAcquired;
    bool m_txChannelAcquired;
	USRPMIMOSettings m_settings;
	USRPMIThread* m_sourceThread;
    USRPMOThread* m_sinkThread;
	QString m_deviceDescription;
	bool m_runningRx;
	bool m_runningTx;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool m_open;
    int m_nbRx;
    int m_nbTx;

    bool openDevice();
    void closeDevice();

	bool applySettings(const USRPMIMOSettings& settings, bool preGetStream, bool force);
    void webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const USRPMIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _PLUTOSDR_PLUTOSDRMIMO_H_
