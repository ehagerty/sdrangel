///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"
#include "SWGStarTrackerDisplaySettings.h"

#include "dsp/dspengine.h"
#include "util/weather.h"
#include "util/units.h"
#include "maincore.h"

#include "startrackerreport.h"
#include "startrackerworker.h"
#include "startracker.h"

MESSAGE_CLASS_DEFINITION(StarTracker::MsgConfigureStarTracker, Message)
MESSAGE_CLASS_DEFINITION(StarTracker::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(StarTracker::MsgSetSolarFlux, Message)

const char* const StarTracker::m_featureIdURI = "sdrangel.feature.startracker";
const char* const StarTracker::m_featureId = "StarTracker";

StarTracker::StarTracker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("StarTracker::StarTracker: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_worker = new StarTrackerWorker(this, webAPIAdapterInterface);
    m_state = StIdle;
    m_errorMessage = "StarTracker error";
    connect(&m_updatePipesTimer, SIGNAL(timeout()), this, SLOT(updatePipes()));
    m_updatePipesTimer.start(1000);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    m_weather = nullptr;
    m_solarFlux = 0.0f;
    // Unfortunately, can't seem to access resources in static global constructor
    m_temps.append(new FITS(":/startracker/startracker/150mhz_ra_dec.fits"));
    m_temps.append(new FITS(":/startracker/startracker/408mhz_ra_dec.fits"));
    m_temps.append(new FITS(":/startracker/startracker/1420mhz_ra_dec.fits"));
    m_spectralIndex = new FITS(":/startracker/startracker/408mhz_ra_dec_spectral_index.fits");
}

StarTracker::~StarTracker()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
    if (m_weather)
    {
        disconnect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
        delete m_weather;
    }
    qDeleteAll(m_temps);
    delete m_spectralIndex;
}

void StarTracker::start()
{
    qDebug("StarTracker::start");

    m_worker->reset();
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    m_worker->getInputMessageQueue()->push(StarTrackerWorker::MsgConfigureStarTrackerWorker::create(m_settings, true));
    m_worker->getInputMessageQueue()->push(MsgSetSolarFlux::create(m_solarFlux));
}

void StarTracker::stop()
{
    qDebug("StarTracker::stop");
    m_worker->stopWork();
    m_state = StIdle;
    m_thread.quit();
    m_thread.wait();
}

bool StarTracker::handleMessage(const Message& cmd)
{
    if (MsgConfigureStarTracker::match(cmd))
    {
        MsgConfigureStarTracker& cfg = (MsgConfigureStarTracker&) cmd;
        qDebug() << "StarTracker::handleMessage: MsgConfigureStarTracker";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "StarTracker::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgSetSolarFlux::match(cmd))
    {
        MsgSetSolarFlux& msg = (MsgSetSolarFlux&) cmd;
        m_solarFlux = msg.getFlux();
        m_worker->getInputMessageQueue()->push(new MsgSetSolarFlux(msg));
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplaySettings::match(cmd))
    {
        MainCore::MsgStarTrackerDisplaySettings& settings = (MainCore::MsgStarTrackerDisplaySettings&) cmd;
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new MainCore::MsgStarTrackerDisplaySettings(settings));
        }
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplayLoSSettings::match(cmd))
    {
        MainCore::MsgStarTrackerDisplayLoSSettings& settings = (MainCore::MsgStarTrackerDisplayLoSSettings&) cmd;
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new MainCore::MsgStarTrackerDisplayLoSSettings(settings));
        }
        return true;
    }
    else
    {
        return false;
    }
}

void StarTracker::updatePipes()
{
    QList<AvailablePipeSource> availablePipes = updateAvailablePipeSources("startracker.display", StarTrackerSettings::m_pipeTypes, StarTrackerSettings::m_pipeURIs, this);

    if (availablePipes != m_availablePipes)
    {
        m_availablePipes = availablePipes;
        if (getMessageQueueToGUI())
        {
            MsgReportPipes *msgToGUI = MsgReportPipes::create();
            QList<AvailablePipeSource>& msgAvailablePipes = msgToGUI->getAvailablePipes();
            msgAvailablePipes.append(availablePipes);
            getMessageQueueToGUI()->push(msgToGUI);
        }
    }
}

QByteArray StarTracker::serialize() const
{
    return m_settings.serialize();
}

bool StarTracker::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void StarTracker::applySettings(const StarTrackerSettings& settings, bool force)
{
    qDebug() << "StarTracker::applySettings:"
            << " m_target: " << settings.m_target
            << " m_ra: " << settings.m_ra
            << " m_dec: " << settings.m_dec
            << " m_az: " << settings.m_az
            << " m_el: " << settings.m_el
            << " m_l: " << settings.m_l
            << " m_b: " << settings.m_b
            << " m_azOffset: " << settings.m_azOffset
            << " m_elOffset: " << settings.m_elOffset
            << " m_latitude: " << settings.m_latitude
            << " m_longitude: " << settings.m_longitude
            << " m_serverPort: " << settings.m_serverPort
            << " m_enableServer: " << settings.m_enableServer
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_target != settings.m_target) || force) {
        reverseAPIKeys.append("target");
    }
    if ((m_settings.m_ra != settings.m_ra) || force) {
        reverseAPIKeys.append("ra");
    }
    if ((m_settings.m_dec != settings.m_dec) || force) {
        reverseAPIKeys.append("dec");
    }
    if ((m_settings.m_latitude != settings.m_latitude) || force) {
        reverseAPIKeys.append("latitude");
    }
    if ((m_settings.m_longitude != settings.m_longitude) || force) {
        reverseAPIKeys.append("longitude");
    }
    if ((m_settings.m_dateTime != settings.m_dateTime) || force) {
        reverseAPIKeys.append("dateTime");
    }
    if ((m_settings.m_refraction != settings.m_refraction) || force) {
        reverseAPIKeys.append("refraction");
    }
    if ((m_settings.m_pressure != settings.m_pressure) || force) {
        reverseAPIKeys.append("pressure");
    }
    if ((m_settings.m_temperature != settings.m_temperature) || force) {
        reverseAPIKeys.append("temperature");
    }
    if ((m_settings.m_humidity != settings.m_humidity) || force) {
        reverseAPIKeys.append("humidity");
    }
    if ((m_settings.m_heightAboveSeaLevel != settings.m_heightAboveSeaLevel) || force) {
        reverseAPIKeys.append("heightAboveSeaLevel");
    }
    if ((m_settings.m_temperatureLapseRate != settings.m_temperatureLapseRate) || force) {
        reverseAPIKeys.append("temperatureLapseRate");
    }
    if ((m_settings.m_frequency != settings.m_frequency) || force) {
        reverseAPIKeys.append("frequency");
    }
    if ((m_settings.m_serverPort != settings.m_serverPort) || force) {
        reverseAPIKeys.append("stellariumPort");
    }
    if ((m_settings.m_enableServer != settings.m_enableServer) || force) {
        reverseAPIKeys.append("stellariumServerEnabled");
    }
    if ((m_settings.m_updatePeriod != settings.m_updatePeriod) || force) {
        reverseAPIKeys.append("updatePeriod");
    }
    if ((m_settings.m_jnow != settings.m_jnow) || force) {
        reverseAPIKeys.append("epoch");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_az != settings.m_az) || force) {
        reverseAPIKeys.append("azimuth");
    }
    if ((m_settings.m_el != settings.m_el) || force) {
        reverseAPIKeys.append("elevation");
    }
    if ((m_settings.m_l != settings.m_l) || force) {
        reverseAPIKeys.append("l");
    }
    if ((m_settings.m_b != settings.m_b) || force) {
        reverseAPIKeys.append("b");
    }
    if ((m_settings.m_azOffset != settings.m_azOffset) || force) {
        reverseAPIKeys.append("azimuthOffset");
    }
    if ((m_settings.m_elOffset != settings.m_elOffset) || force) {
        reverseAPIKeys.append("elevationOffset");
    }

    if ((m_settings.m_owmAPIKey != settings.m_owmAPIKey) || force)
    {
        if (m_weather)
        {
            disconnect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
            delete m_weather;
            m_weather = nullptr;
        }
        if (!settings.m_owmAPIKey.isEmpty())
        {
            m_weather = Weather::create(settings.m_owmAPIKey);
            if (m_weather) {
                connect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
            }
        }
    }

    if (   (m_settings.m_owmAPIKey != settings.m_owmAPIKey)
        || (m_settings.m_latitude != settings.m_latitude)
        || (m_settings.m_longitude != settings.m_longitude)
        || (m_settings.m_weatherUpdatePeriod != settings.m_weatherUpdatePeriod)
        || force)
    {
        if (m_weather) {
            m_weather->getWeatherPeriodically(m_settings.m_latitude, m_settings.m_longitude, settings.m_weatherUpdatePeriod);
        }
    }

    StarTrackerWorker::MsgConfigureStarTrackerWorker *msg = StarTrackerWorker::MsgConfigureStarTrackerWorker::create(
        settings, force
    );
    m_worker->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIFeatureSetIndex != settings.m_reverseAPIFeatureSetIndex) ||
                (m_settings.m_reverseAPIFeatureIndex != settings.m_reverseAPIFeatureIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int StarTracker::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int StarTracker::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setStarTrackerSettings(new SWGSDRangel::SWGStarTrackerSettings());
    response.getStarTrackerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int StarTracker::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    StarTrackerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("StarTracker::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureStarTracker *msgToGUI = MsgConfigureStarTracker::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);
    return 200;
}

void StarTracker::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const StarTrackerSettings& settings)
{
    response.getStarTrackerSettings()->setTarget(new QString(settings.m_target));
    response.getStarTrackerSettings()->setRa(new QString(settings.m_ra));
    response.getStarTrackerSettings()->setDec(new QString(settings.m_dec));
    response.getStarTrackerSettings()->setLatitude(settings.m_latitude);
    response.getStarTrackerSettings()->setLongitude(settings.m_longitude);
    response.getStarTrackerSettings()->setDateTime(new QString(settings.m_dateTime));
    response.getStarTrackerSettings()->setRefraction(new QString(settings.m_refraction));
    response.getStarTrackerSettings()->setPressure(settings.m_pressure);
    response.getStarTrackerSettings()->setTemperature(settings.m_temperature);
    response.getStarTrackerSettings()->setHumidity(settings.m_humidity);
    response.getStarTrackerSettings()->setHeightAboveSeaLevel(settings.m_heightAboveSeaLevel);
    response.getStarTrackerSettings()->setTemperatureLapseRate(settings.m_temperatureLapseRate);
    response.getStarTrackerSettings()->setFrequency(settings.m_frequency/1000000.0);
    response.getStarTrackerSettings()->setStellariumServerEnabled(settings.m_enableServer ? 1 : 0);
    response.getStarTrackerSettings()->setStellariumPort(settings.m_serverPort);
    response.getStarTrackerSettings()->setUpdatePeriod(settings.m_updatePeriod);
    response.getStarTrackerSettings()->setEpoch(settings.m_jnow ? new QString("JNOW") : new QString("J2000"));

    if (response.getStarTrackerSettings()->getTitle()) {
        *response.getStarTrackerSettings()->getTitle() = settings.m_title;
    } else {
        response.getStarTrackerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getStarTrackerSettings()->setRgbColor(settings.m_rgbColor);
    response.getStarTrackerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getStarTrackerSettings()->getReverseApiAddress()) {
        *response.getStarTrackerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getStarTrackerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getStarTrackerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getStarTrackerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getStarTrackerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    response.getStarTrackerSettings()->setAzimuth(settings.m_az);
    response.getStarTrackerSettings()->setElevation(settings.m_el);
    response.getStarTrackerSettings()->setL(settings.m_l);
    response.getStarTrackerSettings()->setB(settings.m_b);
    response.getStarTrackerSettings()->setAzimuthOffset(settings.m_azOffset);
    response.getStarTrackerSettings()->setElevationOffset(settings.m_elOffset);
}

void StarTracker::webapiUpdateFeatureSettings(
    StarTrackerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("target")) {
        settings.m_target = *response.getStarTrackerSettings()->getTarget();
    }
    if (featureSettingsKeys.contains("ra")) {
        settings.m_ra = *response.getStarTrackerSettings()->getRa();
    }
    if (featureSettingsKeys.contains("dec")) {
        settings.m_dec = *response.getStarTrackerSettings()->getDec();
    }
    if (featureSettingsKeys.contains("latitude")) {
        settings.m_latitude = response.getStarTrackerSettings()->getLatitude();
    }
    if (featureSettingsKeys.contains("longitude")) {
        settings.m_longitude = response.getStarTrackerSettings()->getLongitude();
    }
    if (featureSettingsKeys.contains("dateTime")) {
        settings.m_dateTime = *response.getStarTrackerSettings()->getDateTime();
    }
    if (featureSettingsKeys.contains("pressure")) {
        settings.m_pressure = response.getStarTrackerSettings()->getPressure();
    }
    if (featureSettingsKeys.contains("temperature")) {
        settings.m_temperature = response.getStarTrackerSettings()->getTemperature();
    }
    if (featureSettingsKeys.contains("humidity")) {
        settings.m_humidity = response.getStarTrackerSettings()->getHumidity();
    }
    if (featureSettingsKeys.contains("heightAboveSeaLevel")) {
        settings.m_heightAboveSeaLevel = response.getStarTrackerSettings()->getHeightAboveSeaLevel();
    }
    if (featureSettingsKeys.contains("temperatureLapseRate")) {
        settings.m_temperatureLapseRate = response.getStarTrackerSettings()->getTemperatureLapseRate();
    }
    if (featureSettingsKeys.contains("frequency")) {
        settings.m_frequency = response.getStarTrackerSettings()->getFrequency() * 100000.0;
    }
    if (featureSettingsKeys.contains("stellariumServerEnabled")) {
        settings.m_enableServer = response.getStarTrackerSettings()->getStellariumServerEnabled() == 1;
    }
    if (featureSettingsKeys.contains("stellariumPort")) {
        settings.m_serverPort = response.getStarTrackerSettings()->getStellariumPort();
    }
    if (featureSettingsKeys.contains("updatePeriod")) {
        settings.m_updatePeriod = response.getStarTrackerSettings()->getUpdatePeriod();
    }
    if (featureSettingsKeys.contains("epoch")) {
        settings.m_jnow = *response.getStarTrackerSettings()->getEpoch() == "JNOW";
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getStarTrackerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getStarTrackerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getStarTrackerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getStarTrackerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getStarTrackerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getStarTrackerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getStarTrackerSettings()->getReverseApiFeatureIndex();
    }
    if (featureSettingsKeys.contains("azimuth")) {
        settings.m_az = response.getStarTrackerSettings()->getAzimuth();
    }
    if (featureSettingsKeys.contains("elevation")) {
        settings.m_el = response.getStarTrackerSettings()->getElevation();
    }
    if (featureSettingsKeys.contains("l")) {
        settings.m_l = response.getStarTrackerSettings()->getL();
    }
    if (featureSettingsKeys.contains("b")) {
        settings.m_b = response.getStarTrackerSettings()->getB();
    }
    if (featureSettingsKeys.contains("azimuthOffset")) {
        settings.m_azOffset = response.getStarTrackerSettings()->getAzimuthOffset();
    }
    if (featureSettingsKeys.contains("elevationOffset")) {
        settings.m_elOffset = response.getStarTrackerSettings()->getElevationOffset();
    }
}

void StarTracker::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const StarTrackerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("StarTracker"));
    swgFeatureSettings->setStarTrackerSettings(new SWGSDRangel::SWGStarTrackerSettings());
    SWGSDRangel::SWGStarTrackerSettings *swgStarTrackerSettings = swgFeatureSettings->getStarTrackerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("target") || force) {
        swgStarTrackerSettings->setTarget(new QString(settings.m_target));
    }
    if (featureSettingsKeys.contains("ra") || force) {
        swgStarTrackerSettings->setRa(new QString(settings.m_ra));
    }
    if (featureSettingsKeys.contains("dec") || force) {
        swgStarTrackerSettings->setDec(new QString(settings.m_dec));
    }
    if (featureSettingsKeys.contains("latitude") || force) {
        swgStarTrackerSettings->setLatitude(settings.m_latitude);
    }
    if (featureSettingsKeys.contains("longitude") || force) {
        swgStarTrackerSettings->setLongitude(settings.m_longitude);
    }
    if (featureSettingsKeys.contains("dateTime") || force) {
        swgStarTrackerSettings->setDateTime(new QString(settings.m_dateTime));
    }
    if (featureSettingsKeys.contains("pressure") || force) {
        swgStarTrackerSettings->setPressure(settings.m_pressure);
    }
    if (featureSettingsKeys.contains("temperature") || force) {
        swgStarTrackerSettings->setTemperature(settings.m_temperature);
    }
    if (featureSettingsKeys.contains("humidity") || force) {
        swgStarTrackerSettings->setHumidity(settings.m_humidity);
    }
    if (featureSettingsKeys.contains("heightAboveSeaLevel") || force) {
        swgStarTrackerSettings->setHeightAboveSeaLevel(settings.m_heightAboveSeaLevel);
    }
    if (featureSettingsKeys.contains("temperatureLapseRate") || force) {
        swgStarTrackerSettings->setTemperatureLapseRate(settings.m_temperatureLapseRate);
    }
    if (featureSettingsKeys.contains("frequency") || force) {
        swgStarTrackerSettings->setFrequency(settings.m_frequency / 1000000.0);
    }
    if (featureSettingsKeys.contains("stellariumServerEnabled") || force) {
        swgStarTrackerSettings->setStellariumServerEnabled(settings.m_enableServer ? 1 : 0);
    }
    if (featureSettingsKeys.contains("stellariumPort") || force) {
        swgStarTrackerSettings->setStellariumPort(settings.m_serverPort);
    }
    if (featureSettingsKeys.contains("updatePeriod") || force) {
        swgStarTrackerSettings->setUpdatePeriod(settings.m_updatePeriod);
    }
    if (featureSettingsKeys.contains("epoch") || force) {
        swgStarTrackerSettings->setEpoch(settings.m_jnow ? new QString("JNOW") : new QString("J2000"));
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgStarTrackerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgStarTrackerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (featureSettingsKeys.contains("azimuth") || force) {
        swgStarTrackerSettings->setAzimuth(settings.m_az);
    }
    if (featureSettingsKeys.contains("elevation") || force) {
        swgStarTrackerSettings->setElevation(settings.m_el);
    }
    if (featureSettingsKeys.contains("l") || force) {
        swgStarTrackerSettings->setL(settings.m_l);
    }
    if (featureSettingsKeys.contains("b") || force) {
        swgStarTrackerSettings->setB(settings.m_b);
    }
    if (featureSettingsKeys.contains("azimuthOffset") || force) {
        swgStarTrackerSettings->setAzimuthOffset(settings.m_azOffset);
    }
    if (featureSettingsKeys.contains("elevationOffset") || force) {
        swgStarTrackerSettings->setElevationOffset(settings.m_elOffset);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void StarTracker::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "StarTracker::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("StarTracker::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void StarTracker::weatherUpdated(float temperature, float pressure, float humidity)
{
    if (!std::isnan(temperature)) {
        m_settings.m_temperature = temperature;
    }
    if (!std::isnan(pressure)) {
        m_settings.m_pressure = pressure;
    }
    if (!std::isnan(humidity)) {
        m_settings.m_humidity = humidity;
    }

    m_worker->getInputMessageQueue()->push(StarTrackerWorker::MsgConfigureStarTrackerWorker::create(m_settings, false));
    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureStarTracker::create(m_settings, false));
    }
}

double StarTracker::applyBeam(const FITS *fits, double beamwidth, double ra, double dec, int& imgX, int& imgY) const
{
    const double halfBeamwidth = beamwidth/2.0;
    // Use cos^p(x) for approximation of radiation pattern
    // (Essentially the same as Gaussian of exp(-4*ln(theta^2/beamwidth^2))
    // (See a2 in https://arxiv.org/pdf/1812.10084.pdf for Elliptical equivalent))
    // We have gain of 0dB (1) at 0 degrees, and -3dB (~0.5) at half-beamwidth degrees
    // Find exponent that correponds to -3dB at that angle
    double minus3dBLinear = pow(10.0, -3.0/10.0);
    double p = log(minus3dBLinear)/log(cos(Units::degreesToRadians(halfBeamwidth)));
    // Create an matrix with gain as a function of angle
    double degreesPerPixelH = abs(fits->degreesPerPixelH());
    double degreesPerPixelV = abs(fits->degreesPerPixelV());
    int numberOfCoeffsH = ceil(beamwidth/degreesPerPixelH);
    int numberOfCoeffsV = ceil(beamwidth/degreesPerPixelV);
    if ((numberOfCoeffsH & 1) == 0) {
        numberOfCoeffsH++;
    }
    if ((numberOfCoeffsV & 1) == 0) {
        numberOfCoeffsV++;
    }
    double *beam = new double[numberOfCoeffsH*numberOfCoeffsV];
    double sum = 0.0;
    int y0 = numberOfCoeffsV/2;
    int x0 =  numberOfCoeffsH/2;
    int nonZeroCount = 0;
    for (int y = 0; y < numberOfCoeffsV; y++)
    {
        for (int x = 0; x < numberOfCoeffsH; x++)
        {
            double xp = (x - x0) * degreesPerPixelH;
            double yp = (y - y0) * degreesPerPixelV;
            double r = sqrt(xp*xp+yp*yp);
            if (r < halfBeamwidth)
            {
                beam[y*numberOfCoeffsH+x] = pow(cos(Units::degreesToRadians(r)), p);
                sum += beam[y*numberOfCoeffsH+x];
                nonZeroCount++;
            }
            else
            {
                beam[y*numberOfCoeffsH+x] = 0.0;
            }
        }
    }

    // Get centre pixel coordinates
    double centreX;
    if (ra <= 12.0) {
        centreX = (12.0 - ra) / 24.0;
    } else {
        centreX = (24 - ra + 12) / 24.0;
    }
    double centreY = (90.0-dec) / 180.0;
    imgX = centreX * fits->width();
    imgY = centreY * fits->height();

    // Apply weighting to temperature data
    double weightedSum = 0.0;
    for (int y = 0; y < numberOfCoeffsV; y++)
    {
        for (int x = 0; x < numberOfCoeffsH; x++)
        {
            weightedSum += beam[y*numberOfCoeffsH+x] * fits->scaledWrappedValue(imgX + (x-x0), imgY + (y-y0));
        }
    }
    // From: https://www.cv.nrao.edu/~sransom/web/Ch3.html
    // The antenna temperature equals the source brightness temperature multiplied by the fraction of the beam solid angle filled by the source
    // So we scale the sum by the total number of non-zero pixels (i.e. beam area)
    // If we compare to some maps with different beamwidths here: https://www.cv.nrao.edu/~demerson/radiosky/sky_jun96.pdf
    // The values we've computed are a bit higher..
    double temp = weightedSum/nonZeroCount;

    delete[] beam;

    return temp;
}

bool StarTracker::calcSkyTemperature(double frequency, double beamwidth, double ra, double dec, double& temp) const
{
    const FITS *fits;
    int imgX, imgY;

    if ((frequency >= 1.4e9) && (frequency <= 1.45e9))
    {
        // Adjust temperature from 1420MHz FITS file, just using beamwidth
        fits = getTempFITS(2);
        if (fits && fits->valid())
        {
            temp = applyBeam(fits, beamwidth, ra, dec, imgX, imgY);
            return true;
        }
        else
        {
            qDebug() << "StarTracker::calcSkyTemperature: 1420MHz FITS temperature file not valid";
            return false;
        }
    }
    else
    {
        // Adjust temperature from 408MHz FITS file, taking in to account
        // observation frequency and beamwidth
        fits = getTempFITS(1);
        if (fits && fits->valid())
        {
            double temp408 = applyBeam(fits, beamwidth, ra, dec, imgX, imgY);

            // Scale according to frequency - CMB contribution constant
            // Power law at low frequencies, with slight variation in spectral index
            // See:
            // Global Sky Model: https://ascl.net/1011.010
            // An improved Model of Diffuse Galactic Radio Emission: https://arxiv.org/pdf/1605.04920.pdf
            // A high-resolution self-consistent whole sky foreground model: https://arxiv.org/abs/1812.10084
            // (De-striping:) Full sky study of diffuse Galactic emission at decimeter wavelength  https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
            //               Data here: http://cdsarc.u-strasbg.fr/viz-bin/cat/J/A+A/410/847
            // LFmap: https://www.faculty.ece.vt.edu/swe/lwa/memo/lwa0111.pdf
            double iso408 = 50 * pow(150e6/408e6, 2.75);                 // Extra-galactic isotropic in reference map at 408MHz
            double isoT = 50 * pow(150e6/frequency, 2.75);  // Extra-galactic isotropic at target frequency
            double cmbT = 2.725; // Cosmic microwave backgroud;
            double spectralIndex;
            const FITS *spectralIndexFITS = getSpectralIndexFITS();
            if (spectralIndexFITS && spectralIndexFITS->valid())
            {
                 // See https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
                 spectralIndex = spectralIndexFITS->scaledValue(imgX, imgY);
            }
            else
            {
                // See https://arxiv.org/abs/1812.10084 fig 2
                if (frequency < 200e6) {
                    spectralIndex = 2.55;
                } else if (frequency < 20e9) {
                     spectralIndex = 2.695;
                } else {
                     spectralIndex = 3.1;
                }
            }
            double galactic480 = temp408 - cmbT - iso408;
            double galacticT = galactic480 * pow(408e6/frequency, spectralIndex); // Scale galactic contribution by frequency
            temp = galacticT + cmbT + isoT;      // Final temperature

            return true;
        }
        else
        {
            qDebug() << "StarTracker::calcSkyTemperature: 408MHz FITS temperature file not valid";
            return false;
        }
    }
}
