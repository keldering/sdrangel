///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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
#include "webapiadapterbase.h"

void WebAPIAdapterBase::webapiFormatPreferences(
    SWGSDRangel::SWGPreferences *apiPreferences,
    const Preferences& preferences
)
{
    apiPreferences->init();
    apiPreferences->setSourceDevice(new QString(preferences.getSourceDevice()));
    apiPreferences->setSourceIndex(preferences.getSourceIndex());
    apiPreferences->setAudioType(new QString(preferences.getAudioType()));
    apiPreferences->setAudioDevice(new QString(preferences.getAudioDevice()));
    apiPreferences->setLatitude(preferences.getLatitude());
    apiPreferences->setLongitude(preferences.getLongitude());
    apiPreferences->setConsoleMinLogLevel((int) preferences.getConsoleMinLogLevel());
    apiPreferences->setUseLogFile(preferences.getUseLogFile() ? 1 : 0);
    apiPreferences->setLogFileName(new QString(preferences.getLogFileName()));
    apiPreferences->setFileMinLogLevel((int) preferences.getFileMinLogLevel());
}

void WebAPIAdapterBase::webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
)
{
    apiPreset->init();
    apiPreset->setSourcePreset(preset.isSourcePreset() ? 1 : 0);
    apiPreset->setGroup(new QString(preset.getGroup()));
    apiPreset->setDescription(new QString(preset.getDescription()));
    apiPreset->setCenterFrequency(preset.getCenterFrequency());
    apiPreset->getSpectrumConfig()->init(); // TODO when spectrum config is extracted to sdrbase
    apiPreset->setDcOffsetCorrection(preset.hasDCOffsetCorrection() ? 1 : 0);
    apiPreset->setIqImbalanceCorrection(preset.hasIQImbalanceCorrection() ? 1 : 0);

    int nbChannels = preset.getChannelCount();
    for (int i = 0; i < nbChannels; i++)
    {
        const Preset::ChannelConfig& channelConfig = preset.getChannelConfig(i);
        QList<SWGSDRangel::SWGChannelConfig *> *swgChannelConfigs = apiPreset->getChannelConfigs();
        swgChannelConfigs->append(new SWGSDRangel::SWGChannelConfig);
        swgChannelConfigs->back()->setChannelIdUri(new QString(channelConfig.m_channelIdURI));
    }

    int nbDevices = preset.getDeviceCount();
    for (int i = 0; i < nbDevices; i++)
    {
        const Preset::DeviceConfig& deviceConfig = preset.getDeviceConfig(i);
        QList<SWGSDRangel::SWGDeviceConfig *> *swgdeviceConfigs = apiPreset->getDeviceConfigs();
        swgdeviceConfigs->append(new SWGSDRangel::SWGDeviceConfig);
        swgdeviceConfigs->back()->setDeviceId(new QString(deviceConfig.m_deviceId));
        swgdeviceConfigs->back()->setDeviceSerial(new QString(deviceConfig.m_deviceSerial));
        swgdeviceConfigs->back()->setDeviceSequence(deviceConfig.m_deviceSequence);
    }
}

void WebAPIAdapterBase::webapiFormatCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const Command& command
)
{
    apiCommand->init();
    apiCommand->setGroup(new QString(command.getGroup()));
    apiCommand->setDescription(new QString(command.getDescription()));
    apiCommand->setCommand(new QString(command.getCommand()));
    apiCommand->setArgString(new QString(command.getArgString()));
    apiCommand->setKey((int) command.getKey());
    apiCommand->setKeyModifiers((int) command.getKeyModifiers());
    apiCommand->setAssociateKey(command.getAssociateKey() ? 1 : 0);
    apiCommand->setRelease(command.getRelease() ? 1 : 0);
}