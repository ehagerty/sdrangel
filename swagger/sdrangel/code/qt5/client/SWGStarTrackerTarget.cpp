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


#include "SWGStarTrackerTarget.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGStarTrackerTarget::SWGStarTrackerTarget(QString* json) {
    init();
    this->fromJson(*json);
}

SWGStarTrackerTarget::SWGStarTrackerTarget() {
    name = nullptr;
    m_name_isSet = false;
    azimuth = 0.0f;
    m_azimuth_isSet = false;
    elevation = 0.0f;
    m_elevation_isSet = false;
    ra = 0.0f;
    m_ra_isSet = false;
    dec = 0.0f;
    m_dec_isSet = false;
    b = 0.0f;
    m_b_isSet = false;
    l = 0.0f;
    m_l_isSet = false;
    earth_rotation_velocity = 0.0f;
    m_earth_rotation_velocity_isSet = false;
    earth_orbit_velocity_bcrs = 0.0f;
    m_earth_orbit_velocity_bcrs_isSet = false;
    sun_velocity_lsr = 0.0f;
    m_sun_velocity_lsr_isSet = false;
    solar_flux = 0.0f;
    m_solar_flux_isSet = false;
    air_temperature = 0.0f;
    m_air_temperature_isSet = false;
    sky_temperature = 0.0f;
    m_sky_temperature_isSet = false;
    hpbw = 0.0f;
    m_hpbw_isSet = false;
}

SWGStarTrackerTarget::~SWGStarTrackerTarget() {
    this->cleanup();
}

void
SWGStarTrackerTarget::init() {
    name = new QString("");
    m_name_isSet = false;
    azimuth = 0.0f;
    m_azimuth_isSet = false;
    elevation = 0.0f;
    m_elevation_isSet = false;
    ra = 0.0f;
    m_ra_isSet = false;
    dec = 0.0f;
    m_dec_isSet = false;
    b = 0.0f;
    m_b_isSet = false;
    l = 0.0f;
    m_l_isSet = false;
    earth_rotation_velocity = 0.0f;
    m_earth_rotation_velocity_isSet = false;
    earth_orbit_velocity_bcrs = 0.0f;
    m_earth_orbit_velocity_bcrs_isSet = false;
    sun_velocity_lsr = 0.0f;
    m_sun_velocity_lsr_isSet = false;
    solar_flux = 0.0f;
    m_solar_flux_isSet = false;
    air_temperature = 0.0f;
    m_air_temperature_isSet = false;
    sky_temperature = 0.0f;
    m_sky_temperature_isSet = false;
    hpbw = 0.0f;
    m_hpbw_isSet = false;
}

void
SWGStarTrackerTarget::cleanup() {
    if(name != nullptr) { 
        delete name;
    }













}

SWGStarTrackerTarget*
SWGStarTrackerTarget::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGStarTrackerTarget::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&name, pJson["name"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&azimuth, pJson["azimuth"], "float", "");
    
    ::SWGSDRangel::setValue(&elevation, pJson["elevation"], "float", "");
    
    ::SWGSDRangel::setValue(&ra, pJson["ra"], "float", "");
    
    ::SWGSDRangel::setValue(&dec, pJson["dec"], "float", "");
    
    ::SWGSDRangel::setValue(&b, pJson["b"], "float", "");
    
    ::SWGSDRangel::setValue(&l, pJson["l"], "float", "");
    
    ::SWGSDRangel::setValue(&earth_rotation_velocity, pJson["earthRotationVelocity"], "float", "");
    
    ::SWGSDRangel::setValue(&earth_orbit_velocity_bcrs, pJson["earthOrbitVelocityBCRS"], "float", "");
    
    ::SWGSDRangel::setValue(&sun_velocity_lsr, pJson["sunVelocityLSR"], "float", "");
    
    ::SWGSDRangel::setValue(&solar_flux, pJson["solarFlux"], "float", "");
    
    ::SWGSDRangel::setValue(&air_temperature, pJson["airTemperature"], "float", "");
    
    ::SWGSDRangel::setValue(&sky_temperature, pJson["skyTemperature"], "float", "");
    
    ::SWGSDRangel::setValue(&hpbw, pJson["hpbw"], "float", "");
    
}

QString
SWGStarTrackerTarget::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGStarTrackerTarget::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(name != nullptr && *name != QString("")){
        toJsonValue(QString("name"), name, obj, QString("QString"));
    }
    if(m_azimuth_isSet){
        obj->insert("azimuth", QJsonValue(azimuth));
    }
    if(m_elevation_isSet){
        obj->insert("elevation", QJsonValue(elevation));
    }
    if(m_ra_isSet){
        obj->insert("ra", QJsonValue(ra));
    }
    if(m_dec_isSet){
        obj->insert("dec", QJsonValue(dec));
    }
    if(m_b_isSet){
        obj->insert("b", QJsonValue(b));
    }
    if(m_l_isSet){
        obj->insert("l", QJsonValue(l));
    }
    if(m_earth_rotation_velocity_isSet){
        obj->insert("earthRotationVelocity", QJsonValue(earth_rotation_velocity));
    }
    if(m_earth_orbit_velocity_bcrs_isSet){
        obj->insert("earthOrbitVelocityBCRS", QJsonValue(earth_orbit_velocity_bcrs));
    }
    if(m_sun_velocity_lsr_isSet){
        obj->insert("sunVelocityLSR", QJsonValue(sun_velocity_lsr));
    }
    if(m_solar_flux_isSet){
        obj->insert("solarFlux", QJsonValue(solar_flux));
    }
    if(m_air_temperature_isSet){
        obj->insert("airTemperature", QJsonValue(air_temperature));
    }
    if(m_sky_temperature_isSet){
        obj->insert("skyTemperature", QJsonValue(sky_temperature));
    }
    if(m_hpbw_isSet){
        obj->insert("hpbw", QJsonValue(hpbw));
    }

    return obj;
}

QString*
SWGStarTrackerTarget::getName() {
    return name;
}
void
SWGStarTrackerTarget::setName(QString* name) {
    this->name = name;
    this->m_name_isSet = true;
}

float
SWGStarTrackerTarget::getAzimuth() {
    return azimuth;
}
void
SWGStarTrackerTarget::setAzimuth(float azimuth) {
    this->azimuth = azimuth;
    this->m_azimuth_isSet = true;
}

float
SWGStarTrackerTarget::getElevation() {
    return elevation;
}
void
SWGStarTrackerTarget::setElevation(float elevation) {
    this->elevation = elevation;
    this->m_elevation_isSet = true;
}

float
SWGStarTrackerTarget::getRa() {
    return ra;
}
void
SWGStarTrackerTarget::setRa(float ra) {
    this->ra = ra;
    this->m_ra_isSet = true;
}

float
SWGStarTrackerTarget::getDec() {
    return dec;
}
void
SWGStarTrackerTarget::setDec(float dec) {
    this->dec = dec;
    this->m_dec_isSet = true;
}

float
SWGStarTrackerTarget::getB() {
    return b;
}
void
SWGStarTrackerTarget::setB(float b) {
    this->b = b;
    this->m_b_isSet = true;
}

float
SWGStarTrackerTarget::getL() {
    return l;
}
void
SWGStarTrackerTarget::setL(float l) {
    this->l = l;
    this->m_l_isSet = true;
}

float
SWGStarTrackerTarget::getEarthRotationVelocity() {
    return earth_rotation_velocity;
}
void
SWGStarTrackerTarget::setEarthRotationVelocity(float earth_rotation_velocity) {
    this->earth_rotation_velocity = earth_rotation_velocity;
    this->m_earth_rotation_velocity_isSet = true;
}

float
SWGStarTrackerTarget::getEarthOrbitVelocityBcrs() {
    return earth_orbit_velocity_bcrs;
}
void
SWGStarTrackerTarget::setEarthOrbitVelocityBcrs(float earth_orbit_velocity_bcrs) {
    this->earth_orbit_velocity_bcrs = earth_orbit_velocity_bcrs;
    this->m_earth_orbit_velocity_bcrs_isSet = true;
}

float
SWGStarTrackerTarget::getSunVelocityLsr() {
    return sun_velocity_lsr;
}
void
SWGStarTrackerTarget::setSunVelocityLsr(float sun_velocity_lsr) {
    this->sun_velocity_lsr = sun_velocity_lsr;
    this->m_sun_velocity_lsr_isSet = true;
}

float
SWGStarTrackerTarget::getSolarFlux() {
    return solar_flux;
}
void
SWGStarTrackerTarget::setSolarFlux(float solar_flux) {
    this->solar_flux = solar_flux;
    this->m_solar_flux_isSet = true;
}

float
SWGStarTrackerTarget::getAirTemperature() {
    return air_temperature;
}
void
SWGStarTrackerTarget::setAirTemperature(float air_temperature) {
    this->air_temperature = air_temperature;
    this->m_air_temperature_isSet = true;
}

float
SWGStarTrackerTarget::getSkyTemperature() {
    return sky_temperature;
}
void
SWGStarTrackerTarget::setSkyTemperature(float sky_temperature) {
    this->sky_temperature = sky_temperature;
    this->m_sky_temperature_isSet = true;
}

float
SWGStarTrackerTarget::getHpbw() {
    return hpbw;
}
void
SWGStarTrackerTarget::setHpbw(float hpbw) {
    this->hpbw = hpbw;
    this->m_hpbw_isSet = true;
}


bool
SWGStarTrackerTarget::isSet(){
    bool isObjectUpdated = false;
    do{
        if(name && *name != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_azimuth_isSet){
            isObjectUpdated = true; break;
        }
        if(m_elevation_isSet){
            isObjectUpdated = true; break;
        }
        if(m_ra_isSet){
            isObjectUpdated = true; break;
        }
        if(m_dec_isSet){
            isObjectUpdated = true; break;
        }
        if(m_b_isSet){
            isObjectUpdated = true; break;
        }
        if(m_l_isSet){
            isObjectUpdated = true; break;
        }
        if(m_earth_rotation_velocity_isSet){
            isObjectUpdated = true; break;
        }
        if(m_earth_orbit_velocity_bcrs_isSet){
            isObjectUpdated = true; break;
        }
        if(m_sun_velocity_lsr_isSet){
            isObjectUpdated = true; break;
        }
        if(m_solar_flux_isSet){
            isObjectUpdated = true; break;
        }
        if(m_air_temperature_isSet){
            isObjectUpdated = true; break;
        }
        if(m_sky_temperature_isSet){
            isObjectUpdated = true; break;
        }
        if(m_hpbw_isSet){
            isObjectUpdated = true; break;
        }
    }while(false);
    return isObjectUpdated;
}
}

