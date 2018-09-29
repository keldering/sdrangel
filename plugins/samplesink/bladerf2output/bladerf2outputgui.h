///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_
#define PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_

#include <QTimer>
#include <QWidget>

#include "plugin/plugininstancegui.h"
#include "util/messagequeue.h"

#include "bladerf2output.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
    class BladeRF2OutputGui;
}

class BladeRF2OutputGui : public QWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    explicit BladeRF2OutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~BladeRF2OutputGui();
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;

    void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::BladeRF2OutputGui* ui;

    DeviceUISet* m_deviceUISet;
    bool m_doApplySettings;
    bool m_forceSettings;
    BladeRF2OutputSettings m_settings;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    BladeRF2Output* m_sampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void sendSettings();
    void updateSampleRateAndFrequency();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_biasTee_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_bandwidth_changed(quint64 value);
    void on_interp_currentIndexChanged(int index);
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_startStop_toggled(bool checked);
    void updateHardware();
    void updateStatus();
};



#endif /* PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_ */
