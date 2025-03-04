/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube    ---   Limitations and specifcities:    * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 6.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGAFCSettings.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGAFCSettings::SWGAFCSettings(QString* json) {
    init();
    this->fromJson(*json);
}

SWGAFCSettings::SWGAFCSettings() {
    title = nullptr;
    m_title_isSet = false;
    rgb_color = 0;
    m_rgb_color_isSet = false;
    tracker_device_set_index = 0;
    m_tracker_device_set_index_isSet = false;
    tracked_device_set_index = 0;
    m_tracked_device_set_index_isSet = false;
    has_target_frequency = 0;
    m_has_target_frequency_isSet = false;
    transverter_target = 0;
    m_transverter_target_isSet = false;
    target_frequency = 0L;
    m_target_frequency_isSet = false;
    freq_tolerance = 0;
    m_freq_tolerance_isSet = false;
    tracker_adjust_period = 0;
    m_tracker_adjust_period_isSet = false;
    use_reverse_api = 0;
    m_use_reverse_api_isSet = false;
    reverse_api_address = nullptr;
    m_reverse_api_address_isSet = false;
    reverse_api_port = 0;
    m_reverse_api_port_isSet = false;
    reverse_api_feature_set_index = 0;
    m_reverse_api_feature_set_index_isSet = false;
    reverse_api_feature_index = 0;
    m_reverse_api_feature_index_isSet = false;
}

SWGAFCSettings::~SWGAFCSettings() {
    this->cleanup();
}

void
SWGAFCSettings::init() {
    title = new QString("");
    m_title_isSet = false;
    rgb_color = 0;
    m_rgb_color_isSet = false;
    tracker_device_set_index = 0;
    m_tracker_device_set_index_isSet = false;
    tracked_device_set_index = 0;
    m_tracked_device_set_index_isSet = false;
    has_target_frequency = 0;
    m_has_target_frequency_isSet = false;
    transverter_target = 0;
    m_transverter_target_isSet = false;
    target_frequency = 0L;
    m_target_frequency_isSet = false;
    freq_tolerance = 0;
    m_freq_tolerance_isSet = false;
    tracker_adjust_period = 0;
    m_tracker_adjust_period_isSet = false;
    use_reverse_api = 0;
    m_use_reverse_api_isSet = false;
    reverse_api_address = new QString("");
    m_reverse_api_address_isSet = false;
    reverse_api_port = 0;
    m_reverse_api_port_isSet = false;
    reverse_api_feature_set_index = 0;
    m_reverse_api_feature_set_index_isSet = false;
    reverse_api_feature_index = 0;
    m_reverse_api_feature_index_isSet = false;
}

void
SWGAFCSettings::cleanup() {
    if(title != nullptr) { 
        delete title;
    }









    if(reverse_api_address != nullptr) { 
        delete reverse_api_address;
    }



}

SWGAFCSettings*
SWGAFCSettings::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGAFCSettings::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&title, pJson["title"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&rgb_color, pJson["rgbColor"], "qint32", "");
    
    ::SWGSDRangel::setValue(&tracker_device_set_index, pJson["trackerDeviceSetIndex"], "qint32", "");
    
    ::SWGSDRangel::setValue(&tracked_device_set_index, pJson["trackedDeviceSetIndex"], "qint32", "");
    
    ::SWGSDRangel::setValue(&has_target_frequency, pJson["hasTargetFrequency"], "qint32", "");
    
    ::SWGSDRangel::setValue(&transverter_target, pJson["transverterTarget"], "qint32", "");
    
    ::SWGSDRangel::setValue(&target_frequency, pJson["targetFrequency"], "qint64", "");
    
    ::SWGSDRangel::setValue(&freq_tolerance, pJson["freqTolerance"], "qint32", "");
    
    ::SWGSDRangel::setValue(&tracker_adjust_period, pJson["trackerAdjustPeriod"], "qint32", "");
    
    ::SWGSDRangel::setValue(&use_reverse_api, pJson["useReverseAPI"], "qint32", "");
    
    ::SWGSDRangel::setValue(&reverse_api_address, pJson["reverseAPIAddress"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&reverse_api_port, pJson["reverseAPIPort"], "qint32", "");
    
    ::SWGSDRangel::setValue(&reverse_api_feature_set_index, pJson["reverseAPIFeatureSetIndex"], "qint32", "");
    
    ::SWGSDRangel::setValue(&reverse_api_feature_index, pJson["reverseAPIFeatureIndex"], "qint32", "");
    
}

QString
SWGAFCSettings::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGAFCSettings::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(title != nullptr && *title != QString("")){
        toJsonValue(QString("title"), title, obj, QString("QString"));
    }
    if(m_rgb_color_isSet){
        obj->insert("rgbColor", QJsonValue(rgb_color));
    }
    if(m_tracker_device_set_index_isSet){
        obj->insert("trackerDeviceSetIndex", QJsonValue(tracker_device_set_index));
    }
    if(m_tracked_device_set_index_isSet){
        obj->insert("trackedDeviceSetIndex", QJsonValue(tracked_device_set_index));
    }
    if(m_has_target_frequency_isSet){
        obj->insert("hasTargetFrequency", QJsonValue(has_target_frequency));
    }
    if(m_transverter_target_isSet){
        obj->insert("transverterTarget", QJsonValue(transverter_target));
    }
    if(m_target_frequency_isSet){
        obj->insert("targetFrequency", QJsonValue(target_frequency));
    }
    if(m_freq_tolerance_isSet){
        obj->insert("freqTolerance", QJsonValue(freq_tolerance));
    }
    if(m_tracker_adjust_period_isSet){
        obj->insert("trackerAdjustPeriod", QJsonValue(tracker_adjust_period));
    }
    if(m_use_reverse_api_isSet){
        obj->insert("useReverseAPI", QJsonValue(use_reverse_api));
    }
    if(reverse_api_address != nullptr && *reverse_api_address != QString("")){
        toJsonValue(QString("reverseAPIAddress"), reverse_api_address, obj, QString("QString"));
    }
    if(m_reverse_api_port_isSet){
        obj->insert("reverseAPIPort", QJsonValue(reverse_api_port));
    }
    if(m_reverse_api_feature_set_index_isSet){
        obj->insert("reverseAPIFeatureSetIndex", QJsonValue(reverse_api_feature_set_index));
    }
    if(m_reverse_api_feature_index_isSet){
        obj->insert("reverseAPIFeatureIndex", QJsonValue(reverse_api_feature_index));
    }

    return obj;
}

QString*
SWGAFCSettings::getTitle() {
    return title;
}
void
SWGAFCSettings::setTitle(QString* title) {
    this->title = title;
    this->m_title_isSet = true;
}

qint32
SWGAFCSettings::getRgbColor() {
    return rgb_color;
}
void
SWGAFCSettings::setRgbColor(qint32 rgb_color) {
    this->rgb_color = rgb_color;
    this->m_rgb_color_isSet = true;
}

qint32
SWGAFCSettings::getTrackerDeviceSetIndex() {
    return tracker_device_set_index;
}
void
SWGAFCSettings::setTrackerDeviceSetIndex(qint32 tracker_device_set_index) {
    this->tracker_device_set_index = tracker_device_set_index;
    this->m_tracker_device_set_index_isSet = true;
}

qint32
SWGAFCSettings::getTrackedDeviceSetIndex() {
    return tracked_device_set_index;
}
void
SWGAFCSettings::setTrackedDeviceSetIndex(qint32 tracked_device_set_index) {
    this->tracked_device_set_index = tracked_device_set_index;
    this->m_tracked_device_set_index_isSet = true;
}

qint32
SWGAFCSettings::getHasTargetFrequency() {
    return has_target_frequency;
}
void
SWGAFCSettings::setHasTargetFrequency(qint32 has_target_frequency) {
    this->has_target_frequency = has_target_frequency;
    this->m_has_target_frequency_isSet = true;
}

qint32
SWGAFCSettings::getTransverterTarget() {
    return transverter_target;
}
void
SWGAFCSettings::setTransverterTarget(qint32 transverter_target) {
    this->transverter_target = transverter_target;
    this->m_transverter_target_isSet = true;
}

qint64
SWGAFCSettings::getTargetFrequency() {
    return target_frequency;
}
void
SWGAFCSettings::setTargetFrequency(qint64 target_frequency) {
    this->target_frequency = target_frequency;
    this->m_target_frequency_isSet = true;
}

qint32
SWGAFCSettings::getFreqTolerance() {
    return freq_tolerance;
}
void
SWGAFCSettings::setFreqTolerance(qint32 freq_tolerance) {
    this->freq_tolerance = freq_tolerance;
    this->m_freq_tolerance_isSet = true;
}

qint32
SWGAFCSettings::getTrackerAdjustPeriod() {
    return tracker_adjust_period;
}
void
SWGAFCSettings::setTrackerAdjustPeriod(qint32 tracker_adjust_period) {
    this->tracker_adjust_period = tracker_adjust_period;
    this->m_tracker_adjust_period_isSet = true;
}

qint32
SWGAFCSettings::getUseReverseApi() {
    return use_reverse_api;
}
void
SWGAFCSettings::setUseReverseApi(qint32 use_reverse_api) {
    this->use_reverse_api = use_reverse_api;
    this->m_use_reverse_api_isSet = true;
}

QString*
SWGAFCSettings::getReverseApiAddress() {
    return reverse_api_address;
}
void
SWGAFCSettings::setReverseApiAddress(QString* reverse_api_address) {
    this->reverse_api_address = reverse_api_address;
    this->m_reverse_api_address_isSet = true;
}

qint32
SWGAFCSettings::getReverseApiPort() {
    return reverse_api_port;
}
void
SWGAFCSettings::setReverseApiPort(qint32 reverse_api_port) {
    this->reverse_api_port = reverse_api_port;
    this->m_reverse_api_port_isSet = true;
}

qint32
SWGAFCSettings::getReverseApiFeatureSetIndex() {
    return reverse_api_feature_set_index;
}
void
SWGAFCSettings::setReverseApiFeatureSetIndex(qint32 reverse_api_feature_set_index) {
    this->reverse_api_feature_set_index = reverse_api_feature_set_index;
    this->m_reverse_api_feature_set_index_isSet = true;
}

qint32
SWGAFCSettings::getReverseApiFeatureIndex() {
    return reverse_api_feature_index;
}
void
SWGAFCSettings::setReverseApiFeatureIndex(qint32 reverse_api_feature_index) {
    this->reverse_api_feature_index = reverse_api_feature_index;
    this->m_reverse_api_feature_index_isSet = true;
}


bool
SWGAFCSettings::isSet(){
    bool isObjectUpdated = false;
    do{
        if(title && *title != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_rgb_color_isSet){
            isObjectUpdated = true; break;
        }
        if(m_tracker_device_set_index_isSet){
            isObjectUpdated = true; break;
        }
        if(m_tracked_device_set_index_isSet){
            isObjectUpdated = true; break;
        }
        if(m_has_target_frequency_isSet){
            isObjectUpdated = true; break;
        }
        if(m_transverter_target_isSet){
            isObjectUpdated = true; break;
        }
        if(m_target_frequency_isSet){
            isObjectUpdated = true; break;
        }
        if(m_freq_tolerance_isSet){
            isObjectUpdated = true; break;
        }
        if(m_tracker_adjust_period_isSet){
            isObjectUpdated = true; break;
        }
        if(m_use_reverse_api_isSet){
            isObjectUpdated = true; break;
        }
        if(reverse_api_address && *reverse_api_address != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_reverse_api_port_isSet){
            isObjectUpdated = true; break;
        }
        if(m_reverse_api_feature_set_index_isSet){
            isObjectUpdated = true; break;
        }
        if(m_reverse_api_feature_index_isSet){
            isObjectUpdated = true; break;
        }
    }while(false);
    return isObjectUpdated;
}
}

